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
    const t_binary_stream_status status = binary_stream_icmp_write_checksum(stream, 2, stream->index);
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

bool handler_echo_reply_packet(void *packet) {
    if (packet == NULL) {
        return false;
    }
    t_echo_reply *reply = packet;
    printf("t_echo_reply_packet: echo reply received id=%u, seq=%u", reply->identifier, reply->sequence);
    return true;
}


void init_packet_processors_icmp(void) {
    register_packet_processor(8, &serialize_header_icmp, &serializer_echo_request, &serializer_checksum_icmp, NULL,NULL, NULL, NULL, NULL);
    register_packet_processor(0, NULL, NULL, NULL, &factory_echo_reply_packet, &pre_deserializer_header_icmp, &deserializer_echo_reply, &free, &handler_echo_reply_packet);
}


