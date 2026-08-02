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
    t_binary_stream *stream = create_binary_stream_with_capacity(sizeof(t_echo_request), BINARY_STREAM_ENDIAN_BIG);
    packet_processor_serialize(8, stream, packet);
    free(packet);
    stream->methods.print(stream);
    t_binary_stream *copy = stream->methods.copy(stream);
    copy->data[0] = 0;
    binary_stream_icmp_write_checksum(copy, 2, copy->capacity);
    copy->methods.print(copy);
    t_echo_reply *packet_decode = packet_processor_deserializer(copy);
    if (packet_decode != NULL) {
        printf("t_echo_reply{id=%u, seq=%u}\n", packet_decode->identifier, packet_decode->sequence);
    }else {
        fprintf(stderr, "t_echo_reply: NULL\n");
    }
    return 0;
}