//
// Created by gcaptari on 02/08/2026.
//

#include "echo_reply_packet.h"
#include <stdlib.h>

void *factory_echo_reply_packet() {
    return malloc(sizeof(t_echo_reply));
}

t_packet_processor_status deserializer_echo_reply(t_binary_stream *stream, void *return_value) {
    t_echo_reply *reply = return_value;
    t_binary_stream_status status = stream->methods.read_unsigned_short(stream, &reply->identifier);
    if (binary_stream_status_is_failed(status)) {
        return  PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE;
    }
    status = stream->methods.read_unsigned_short(stream, &reply->sequence);
    if (binary_stream_status_is_failed(status)) {
        return PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE;
    }
    return PACKET_PROCESSOR_STATUS_OK;
}