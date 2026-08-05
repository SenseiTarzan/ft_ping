#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "src/icmp/binary_stream_icmp.h"
#include "src/icmp/packet_processors_icmp.h"
#include "src/icmp/packet/echo_reply_packet.h"
#include "src/icmp/packet/echo_request_packet.h"
#include "src/packet_processors/packet_processors.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "src/ping/ping.h"
#include <netinet/ip_icmp.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <IP address>\n", argv[0]);
        return 1;
    }
    t_ping ping;
    ping.processor = packet_pool_new(256);
    if (ping.processor == NULL) {
        return 1;
    }
    ping.binary_stream = create_binary_stream_with_capacity(1500, BINARY_STREAM_ENDIAN_BIG);
    ping.ping_id = getpid() & 0xFFFFF;
    ping.dest_addr.sin_family = AF_INET;
    memset(&ping.dest_addr, 0, sizeof(ping.dest_addr));
    ping.dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET,  argv[1], &ping.dest_addr.sin_addr);
    const t_packet_pool *pool = packet_pool_new(256);
    if (pool == NULL) {
        return 1;
    }
    init_packet_processors_icmp(pool);
    t_echo_request *packet = echo_request_new(5, 6, ""); //TAILLE max est de 1480 octect
    if (packet == NULL) {
        return 1;
    }

    t_binary_stream *stream = create_binary_stream_with_capacity(sizeof(t_echo_request), BINARY_STREAM_ENDIAN_BIG);
    if (stream == NULL) {
        return 1;
    }
    packet_processor_serialize(pool, 8, stream, packet);
    free(packet);
    stream->methods.print(stream);
    // Création du socket raw ICMP
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket (besoin de root)");
        free(packet);
        return 1;
    }


    // Adresse de destination

    // Envoi du paquet
    ssize_t sent = sendto(sockfd, stream->data, stream->capacity, 0,
                          (struct sockaddr *)&ping.dest_addr, sizeof(ping.dest_addr));

    if (sent < 0) {
        perror("sendto");
    } else {
        printf("Paquet ICMP envoyé : %zd octets vers %s\n", sent, argv[1]);
    }

    t_binary_stream *test = create_binary_stream_with_capacity(1500, BINARY_STREAM_ENDIAN_BIG);
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    test->methods.reset(test);
    ssize_t n = recvfrom(sockfd, test->data, test->capacity, 0,
                             (struct sockaddr *)&from_addr, &from_len);

    struct iphdr *ip_hdr = (struct iphdr *)test->data;
    size_t ip_hdr_len = ip_hdr->ihl * 4;  // ip_hl en mots de 32 bits

    // Vérifier que le protocole est bien ICMP
    if (ip_hdr->protocol != IPPROTO_ICMP) {
        return 2;  // Ignorer les paquets non-ICMP
    }
    test->index = ip_hdr_len;

    if (test == NULL) {
        return 1;
    }
    test->methods.print(test);
    t_echo_reply *packet_decode = packet_processor_deserializer(pool, test);
    if (packet_decode == NULL) {
        binary_stream_free(stream);
        binary_stream_free(test);
        return 1;
    }
    packet_processor_icmp_handler(pool, packet_decode);
    binary_stream_free(test);
    close(sockfd);
/*

    t_binary_stream *slice = stream->methods.slice(stream, 2, 4);
        if (slice == NULL) {
            return 1;
        }
        slice->methods.print(slice);
        binary_stream_free(slice);
        t_binary_stream *copy = stream->methods.copy(stream);
        if (copy == NULL) {
            binary_stream_free(stream);
            return 1;
        }
        copy->data[0] = 0;
        binary_stream_icmp_write_checksum(copy, 2, copy->capacity);
        copy->methods.print(copy);
        t_echo_reply *packet_decode = packet_processor_deserializer(copy);
        if (packet_decode == NULL) {
            binary_stream_free(stream);
            binary_stream_free(copy);
            return 1;
        }
        packet_processor_icmp_handler(packet_decode);
        binary_stream_free(stream);
        binary_stream_free(copy);
 */
    return 0;
}