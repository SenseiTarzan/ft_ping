//
// Created by gcaptari on 05/08/2026.
//

#ifndef FT_PING_PING_H
#define FT_PING_PING_H
#include <netinet/in.h>

#include "../types.h"


struct s_metric_ping {
    int lost_packets;
    int duplicated_packets;
    int transmitted_packets;
    int received_packets;
};

struct s_ping {
    int ping_id;
    struct sockaddr_in dest_addr;
    struct s_metric_ping metric;
    t_binary_stream *binary_stream;
    t_packet_pool *processor;
};
#endif //FT_PING_PING_H
