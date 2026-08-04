//
// Created by gcaptari on 02/08/2026.
//

#include "echo_request_packet.h"
#include "../../packet_processors/packet_processors.h"
#include <stdlib.h>

t_echo_request *echo_request_new(uint8_t identifier, uint8_t sequence, char *payload) {
    t_echo_request *request = malloc(sizeof(struct s_echo_request));
    if (request == NULL) {
        return NULL;
    }
    request->packet_header.type = 8;
    request->packet_header.code = 0;
    request->packet_header.checksum = 0;
    request->identifier = identifier;
    request->sequence = sequence;
    gettimeofday(&(request->timestamp), NULL);
    request->payload = payload;
    return request;
}

bool echo_request_is_valid(t_echo_request *request) {
    if (request == NULL) {
        return false;
    }
    return true;
}

t_packet_processor_status serializer_echo_request(t_binary_stream *stream, const void *buffer) {
    if (buffer == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_NO_DATA;
    }
     const t_echo_request *request = buffer;
    t_binary_stream_status status = stream->methods.write_unsigned_short(stream, request->identifier);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE;
    }
     status = stream->methods.write_unsigned_short(stream, request->sequence);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE;
    }
     status = stream->methods.write_long(stream, request->timestamp.tv_sec);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE;
    }
     status = stream->methods.write_long(stream, request->timestamp.tv_usec);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE;
    }

     status = stream->methods.write_string(stream, request->payload);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE;
    }

    return PACKET_PROCESSOR_STATUS_OK;
}
