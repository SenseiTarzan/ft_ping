//
// Created by gcaptari on 02/08/2026.
//

#include "packet_processors_icmp.h"

#include <stdlib.h>

#include "binary_stream_icmp.h"

#include "packet/echo_request_packet.h"
#include "packet/echo_reply_packet.h"

static t_packet_processor_status serialize_header_icmp(t_binary_stream *stream, const void *buffer) {
    if (buffer == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_PRE_SERIALIZE;
    }
    const t_binary_stream_status status = binary_stream_icmp_write_header(stream, buffer);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE;
    }
    return PACKET_PROCESSOR_STATUS_OK;
}

static t_packet_processor_status serializer_checksum_icmp(t_binary_stream *stream, const void *buffer) {
    if (buffer == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE;
    }
    t_binary_stream_status status;
    ssize_t index = 64 - (ssize_t)stream->index;
    while (index > 0) {
        status = stream->methods.write_char(stream, 0);
        if (binary_stream_status_is_failed(status)) {
            return PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE;
        }
        --index;
    }
    status =  binary_stream_icmp_write_checksum(stream, 2, stream->index);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE;
    }

    return PACKET_PROCESSOR_STATUS_OK;
}

static t_packet_processor_status pre_deserializer_header_icmp(t_binary_stream *stream, void *buffer) {
    if (buffer == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE;
    }
    if (binary_stream_icmp_check_checksum(stream, stream->capacity) == BINARY_STREAM_CORRUPT) {
        return PACKET_PROCESSOR_STATUS_FAILURE_CHECKSUM;
    }
    binary_stream_icmp_read_header(stream, buffer);
    return PACKET_PROCESSOR_STATUS_OK;
}

bool packet_processor_icmp_handler(const t_packet_pool *pool, struct sockaddr *addr, struct iphdr *ip_hdr, void *packet) {
    if (packet == NULL) {
        return false;
    }
    const t_header_icmp *header = packet;
    return packet_processor_handler(pool, header->type, addr, ip_hdr, packet);
}


void init_packet_processors_icmp(const t_packet_pool *pool) {
    register_packet_processor(pool,8, &serialize_header_icmp, &serializer_echo_request, &serializer_checksum_icmp, NULL, NULL,NULL, NULL, NULL, NULL);
    register_packet_processor(pool, 0, NULL, NULL, NULL, NULL, &factory_echo_reply_packet, &pre_deserializer_header_icmp, &deserializer_echo_reply, &free, NULL);
}


