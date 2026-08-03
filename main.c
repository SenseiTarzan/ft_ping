#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/icmp/binary_stream_icmp.h"
#include "src/icmp/packet_processors_icmp.h"
#include "src/icmp/packet/echo_reply_packet.h"
#include "src/icmp/packet/echo_request_packet.h"
#include "src/packet_processors/packet_processors.h"

int main(void) {
    init_packet_processors_icmp();
    t_echo_request *packet = echo_request_new(5, 6);
    if (packet == NULL) {
        return 1;
    }
    t_binary_stream *stream = create_binary_stream_with_capacity(sizeof(t_echo_request), BINARY_STREAM_ENDIAN_BIG);
    if (stream == NULL) {
        return 1;
    }
    packet_processor_serialize(8, stream, packet);
    free(packet);
    stream->methods.print(stream);
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
    return 0;
}