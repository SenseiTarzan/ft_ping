//
// Created by gcaptari on 02/08/2026.
//

#ifndef FT_PING_BINARY_STREAM_ICMP_H
#define FT_PING_BINARY_STREAM_ICMP_H

#include "../binary_stream/binary_stream.h"

struct  s_header_icmp {
    int8_t type;
    int8_t code;
    uint16_t checksum;
};


t_binary_stream_status binary_stream_icmp_write_checksum(t_binary_stream * stream, size_t index, size_t len);
t_binary_stream_status binary_stream_icmp_check_checksum(t_binary_stream * stream, size_t len);
t_binary_stream_status binary_stream_icmp_write_header(t_binary_stream * stream, const void *data);
t_binary_stream_status binary_stream_icmp_read_header(t_binary_stream * stream, void *data);
#endif //FT_PING_BINARY_STREAM_ICMP_H
