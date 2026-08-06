//
// Created by gcaptari on 05/08/2026.
//

#ifndef FT_PING_PING_H
#define FT_PING_PING_H
#include <stdbool.h>
#include <netinet/in.h>

#include "../types.h"

struct s_metric_ping {
    int lost_packets;
    int duplicated_packets;
    int transmitted_packets;
    int received_packets;
};

struct s_sequence_ping_history {
    uint16_t sequence;
    t_sequence_ping_history *next;
};

struct s_ping {
    struct s_metric_ping metric;
    struct sockaddr_in dest_addr;
    t_sequence_ping_history *history;
    t_binary_stream *binary_stream;
    t_packet_pool *processor;
    uint16_t current_sequence;
    uint16_t ping_id;
};

t_sequence_ping_history *sequence_ping_history_new(uint16_t sequence);

bool sequence_ping_history_has(const t_sequence_ping_history *history, uint16_t sequence);

bool sequence_ping_history_add(t_sequence_ping_history **history, uint16_t sequence);

bool sequence_ping_history_free(t_sequence_ping_history *history);

#endif //FT_PING_PING_H
