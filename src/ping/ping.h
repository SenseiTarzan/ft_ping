//
// Created by gcaptari on 05/08/2026.
//

#ifndef FT_PING_PING_H
#define FT_PING_PING_H
#include <netdb.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/time.h>

#include "../types.h"

enum  e_ping_option {
    PING_OPTION_NONE = 0,
    PING_OPTION_VERBOSE = 1 << 0,
};

struct s_metric_ping {
    int lost_packets;
    int duplicated_packets;
    int transmitted_packets;
    int received_packets;
    double rtt_min;
    double rtt_max;
    double rtt_sum;
    double rtt_sum_squared;
};

struct s_sequence_ping_history {
    uint16_t sequence;
    t_sequence_ping_history *next;
};

struct s_ping {
    struct s_metric_ping metric;
    struct addrinfo *dest_addr;
    t_sequence_ping_history *history;
    t_binary_stream *send;
    t_binary_stream *receive;
    t_packet_pool *processor;
    struct timeval start_time;
    int socket;
    uint16_t current_sequence;
    uint16_t ping_id;
    int option;
};

t_sequence_ping_history *sequence_ping_history_new(uint16_t sequence);

bool sequence_ping_history_has(const t_sequence_ping_history *history, uint16_t sequence);

bool sequence_ping_history_add(t_sequence_ping_history **history, uint16_t sequence);

bool sequence_ping_history_free(t_sequence_ping_history *history);

#endif //FT_PING_PING_H
