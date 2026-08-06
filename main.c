#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <sys/select.h>

#include "src/icmp/binary_stream_icmp.h"
#include "src/icmp/packet_processors_icmp.h"
#include "src/icmp/packet/echo_request_packet.h"
#include "src/packet_processors/packet_processors.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "src/icmp/packet/echo_reply_packet.h"
#include "src/ping/ping.h"
#include <getopt.h>
#include <netdb.h>

#define PING_ICMP_PACKET_SIZE 64
#define PING_ICMP_HEADER_SIZE 8
#define PING_IP_HEADER_SIZE 20

static volatile sig_atomic_t g_ping_interrupted = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_ping_interrupted = 1;
}

static const char *ai_family_to_string(int family) {
    switch (family) {
        case AF_INET: return "AF_INET";
        case AF_INET6: return "AF_INET6";
        case AF_UNSPEC: return "AF_UNSPEC";
        default: return "AF_UNKNOWN";
    }
}

static bool ping_gettimeofday(struct timeval *tv) {
    if (gettimeofday(tv, NULL) != 0) {
        perror("gettimeofday");
        return false;
    }
    return true;
}

bool handler_echo_reply(void *this, struct sockaddr *from_addr, struct iphdr *ip_hdr, void *packet_raw, bool success) {
    if (this == NULL || !success) {
        return false;
    }
    t_ping *ping = this;
    t_echo_reply *packet = packet_raw;
    if (ping->ping_id != packet->identifier) {
        fprintf(stderr, "Warning nor correct identifier %hu pid: %hu\n", packet->identifier, ping->ping_id);
        return false;
    }

    char nom_domaine[NI_MAXHOST] = {0};
    char ip_str[NI_MAXHOST] = {0};

    getnameinfo(from_addr, (socklen_t)sizeof(struct sockaddr_storage),
                nom_domaine, sizeof(nom_domaine),
                NULL, 0, 0);
    getnameinfo(from_addr, (socklen_t)sizeof(struct sockaddr_storage),
                ip_str, sizeof(ip_str),
                NULL, 0, NI_NUMERICHOST);

    struct timeval tv;
    if (!ping_gettimeofday(&tv)) {
        return false;
    }
    const double temps_ms = ((double)(tv.tv_sec - packet->timestamp.tv_sec) * 1000.0)
                           + ((double)(tv.tv_usec - packet->timestamp.tv_usec) / 1000.0);

    if (sequence_ping_history_has(ping->history, packet->sequence)) {
        ping->metric.duplicated_packets++;
        printf("%d bytes from %s (%s): icmp_seq=%d ttl=%d time=%.2f ms (DUP!)\n",
               PING_ICMP_PACKET_SIZE, nom_domaine, ip_str, packet->sequence, ip_hdr->ttl, temps_ms);
        return true;
    }
    sequence_ping_history_add(&ping->history, packet->sequence);

    ping->metric.received_packets++;
    if (temps_ms < ping->metric.rtt_min) ping->metric.rtt_min = temps_ms;
    if (temps_ms > ping->metric.rtt_max) ping->metric.rtt_max = temps_ms;
    ping->metric.rtt_sum += temps_ms;
    ping->metric.rtt_sum_squared += temps_ms * temps_ms;

    printf("%d bytes from %s (%s): icmp_seq=%d ttl=%d time=%.2f ms\n",
           PING_ICMP_PACKET_SIZE, nom_domaine, ip_str, packet->sequence, ip_hdr->ttl, temps_ms);
    return true;
}

static int resolve_address(const char *host, int family, struct addrinfo **res) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = SOCK_RAW;
    hints.ai_flags = AI_CANONNAME;

    const int status = getaddrinfo(host, NULL, &hints, res);
    if (status != 0) {
        fprintf(stderr, "%s: %s\n", host, gai_strerror(status));
        return -1;
    }
    return 0;
}

static void print_usage(const char *prog) {
    printf("Usage: %s [-v] [-h] <IP address>\n", prog);
}

static struct option long_options[] = {
  {"verbose", no_argument, NULL, 'v'},
  {"help",    no_argument, NULL, 'h'},
  {NULL, 0, NULL, 0}
};

static void populate_option(t_ping *ping, int argc, char *argv[]) {
      int opt;
      while ((opt = getopt_long(argc, argv, "vh", long_options, NULL)) != -1) {
          switch (opt) {
              case 'v':
                  ping->option |= PING_OPTION_VERBOSE;
                  break;
              case 'h':
                  print_usage(argv[0]);
                  exit(0);
              default:
                  print_usage(argv[0]);
                  exit(1);
          }
      }

    if (optind >= argc) {
        print_usage(argv[0]);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    t_ping ping;
    memset(&ping, 0, sizeof(ping));
    populate_option(&ping, argc, argv);
    const char *host = argv[optind];

    t_packet_pool *pool = packet_pool_new(256);
    if (pool == NULL) {
        return 1;
    }
    ping.processor = pool;
    ping.receive = create_binary_stream_with_capacity(1500, BINARY_STREAM_ENDIAN_BIG);
    if (ping.receive == NULL) {
        packet_pool_free(pool);
        return 1;
    }
    ping.send = create_binary_stream_with_capacity(sizeof(t_echo_request), BINARY_STREAM_ENDIAN_BIG);
    if (ping.send == NULL) {
        binary_stream_free(ping.receive);
        packet_pool_free(pool);
        return 1;
    }
    ping.ping_id = getpid() & 0xFFFF;
    ping.metric.rtt_min = DBL_MAX;

    init_packet_processors_icmp(pool);
    packet_processor_set_this(pool, 0, &ping);
    packet_processor_set_handler(pool, 0, handler_echo_reply);

    const int sock4 = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    const int sock4_errno = errno;
    const int sock6 = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);

    int hints_family = AF_UNSPEC;
    if (sock4 >= 0 && sock6 >= 0) {
        hints_family = AF_UNSPEC;
    } else if (sock4 >= 0) {
        hints_family = AF_INET;
    } else if (sock6 >= 0) {
        hints_family = AF_INET6;
    } else {
        fprintf(stderr, "%s: %s (are you root?)\n", argv[0], strerror(sock4_errno));
        binary_stream_free(ping.send);
        binary_stream_free(ping.receive);
        packet_pool_free(pool);
        return 1;
    }

    if (ping.option & PING_OPTION_VERBOSE) {
        printf("ping: sock4.fd: %d (socktype: SOCK_RAW), sock6.fd: %d (socktype: SOCK_RAW), hints.ai_family: %s\n\n",
               sock4, sock6, ai_family_to_string(hints_family));
    }

    if (resolve_address(host, hints_family, &ping.dest_addr) != 0) {
        if (sock4 >= 0) close(sock4);
        if (sock6 >= 0) close(sock6);
        binary_stream_free(ping.send);
        binary_stream_free(ping.receive);
        packet_pool_free(pool);
        return 1;
    }

    struct addrinfo *rp = NULL;
    for (struct addrinfo *cur = ping.dest_addr; cur != NULL; cur = cur->ai_next) {
        if (cur->ai_family == AF_INET) {
            rp = cur;
            break;
        }
    }
    if (rp == NULL || sock4 < 0) {
        fprintf(stderr, "%s: no usable IPv4 address found\n", host);
        if (sock4 >= 0) close(sock4);
        if (sock6 >= 0) close(sock6);
        freeaddrinfo(ping.dest_addr);
        binary_stream_free(ping.send);
        binary_stream_free(ping.receive);
        packet_pool_free(pool);
        return 1;
    }

    if (ping.option & PING_OPTION_VERBOSE) {
        printf("ai->ai_family: %s, ai->ai_canonname: '%s'\n",
               ai_family_to_string(rp->ai_family), rp->ai_canonname ? rp->ai_canonname : host);
    }

    if (sock6 >= 0) {
        close(sock6);
    }
    ping.socket = sock4;

    if (connect(ping.socket, rp->ai_addr, rp->ai_addrlen) == -1) {
        perror("connect");
        close(ping.socket);
        freeaddrinfo(ping.dest_addr);
        binary_stream_free(ping.send);
        binary_stream_free(ping.receive);
        packet_pool_free(pool);
        return 1;
    }

    char ip_str[NI_MAXHOST] = {0};
    getnameinfo(rp->ai_addr, rp->ai_addrlen, ip_str, sizeof(ip_str), NULL, 0, NI_NUMERICHOST);
    printf("PING %s (%s) %d(%d) bytes of data.\n", host, ip_str,
           PING_ICMP_PACKET_SIZE - PING_ICMP_HEADER_SIZE, PING_ICMP_PACKET_SIZE + PING_IP_HEADER_SIZE);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    if (!ping_gettimeofday(&ping.start_time)) {
        close(ping.socket);
        freeaddrinfo(ping.dest_addr);
        binary_stream_free(ping.send);
        binary_stream_free(ping.receive);
        packet_pool_free(pool);
        return 1;
    }

    uint16_t sequence = 1;
    struct timeval next_send = ping.start_time;

    while (!g_ping_interrupted) {
        struct timeval now;
        if (!ping_gettimeofday(&now)) {
            continue;
        }

        const bool due_to_send = !timercmp(&now, &next_send, <);
        struct timeval timeout = {0, 0};
        if (!due_to_send) {
            timersub(&next_send, &now, &timeout);
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(ping.socket, &fds);

        const int ready = due_to_send
            ? select(ping.socket + 1, NULL, &fds, NULL, &timeout)
            : select(ping.socket + 1, &fds, NULL, NULL, &timeout);
        if (ready < 0) {
            if (errno == EINTR) break;
            perror("select");
            break;
        }

        if (due_to_send) {
            t_echo_request *packet = echo_request_new(ping.ping_id, (uint8_t)sequence);
            if (packet == NULL) {
                break;
            }
            ping.send->methods.reset(ping.send);
            packet_processor_serialize(pool, packet->packet_header.type, ping.send, packet);
            const ssize_t sent = send(ping.socket, ping.send->data, ping.send->index, 0);
            free(packet);

            if (sent < 0) {
                if (errno == EINTR) break;
                perror("send");
            } else {
                ping.metric.transmitted_packets++;
                sequence++;
            }
            next_send = now;
            next_send.tv_sec += 1;
        } else if (ready > 0) {
            struct sockaddr_storage from_addr;
            memset(&from_addr, 0, sizeof(from_addr));
            socklen_t from_len = sizeof(from_addr);
            const ssize_t n = recvfrom(ping.socket, ping.receive->data, ping.receive->capacity, 0,
                                        (struct sockaddr *)&from_addr, &from_len);
            if (n < 0) {
                if (errno == EINTR) break;
                perror("recvfrom");
                continue;
            }

            struct iphdr *ip_hdr = (struct iphdr *)ping.receive->data;
            if (ip_hdr->protocol != IPPROTO_ICMP) {
                continue;
            }
            ping.receive->index = ip_hdr->ihl * 4;
            void *packet_decode = packet_processor_deserializer(pool, ping.receive);
            if (packet_decode == NULL) {
                continue;
            }
            packet_processor_icmp_handler(pool, (struct sockaddr *)&from_addr, ip_hdr, packet_decode);
        }
    }

    struct timeval end_time;
    if (!ping_gettimeofday(&end_time)) {
        end_time = ping.start_time;
    }
    const long elapsed_ms = (end_time.tv_sec - ping.start_time.tv_sec) * 1000
                           + (end_time.tv_usec - ping.start_time.tv_usec) / 1000;

    const int transmitted = ping.metric.transmitted_packets;
    const int received = ping.metric.received_packets;
    const int duplicated = ping.metric.duplicated_packets;
    const int loss_percent = transmitted > 0 ? ((transmitted - received) * 100) / transmitted : 0;

    printf("\n--- %s ping statistics ---\n", host);
    if (duplicated > 0) {
        printf("%d packets transmitted, %d received, +%d duplicates, %d%% packet loss, time %ldms\n",
               transmitted, received, duplicated, loss_percent, elapsed_ms);
    } else {
        printf("%d packets transmitted, %d received, %d%% packet loss, time %ldms\n",
               transmitted, received, loss_percent, elapsed_ms);
    }
    if (received > 0) {
        const double avg = ping.metric.rtt_sum / received;
        const double mdev = sqrt(ping.metric.rtt_sum_squared / received - avg * avg);
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n",
               ping.metric.rtt_min, avg, ping.metric.rtt_max, mdev);
    }

    sequence_ping_history_free(ping.history);
    freeaddrinfo(ping.dest_addr);
    binary_stream_free(ping.send);
    binary_stream_free(ping.receive);
    close(ping.socket);
    packet_pool_free(pool);
    return 0;
}
