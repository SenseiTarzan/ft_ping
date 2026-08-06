//
// Created by gcaptari on 02/08/2026.
//

#ifndef FT_PING_PACKET_PROCESSORS_ICMP_H
#define FT_PING_PACKET_PROCESSORS_ICMP_H

#include "../packet_processors/packet_processors.h"

void init_packet_processors_icmp(const t_packet_pool *pool);

bool packet_processor_icmp_handler(const t_packet_pool *pool, struct sockaddr *addr, struct iphdr *ip_hdr, void *packet);

#endif //FT_PING_PACKET_PROCESSORS_ICMP_H
