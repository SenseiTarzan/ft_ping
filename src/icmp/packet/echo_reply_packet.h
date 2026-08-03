//
// Created by gcaptari on 02/08/2026.
//

#ifndef FT_PING_ECHO_REPLY_PACKET_H
#define FT_PING_ECHO_REPLY_PACKET_H
#include <stdbool.h>
#include <stdint.h>
#include "../binary_stream_icmp.h"
#include "../../packet_processors/packet_processors.h"
struct s_echo_reply {
    t_header_icmp packet_header;
    uint16_t identifier;
    uint16_t sequence;
};

/**
 * Allocate an echo reply, its ICMP header already filled in.
 * @param identifier identifier carried by the reply
 * @param sequence sequence number carried by the reply
 * @return the reply, or NULL on allocation failure. The caller owns it and
 *         must free() it.
 */
t_echo_reply *echo_reply_new(uint8_t identifier, uint8_t sequence);

/**
 * Tell whether a reply may be sent.
 * @param reply reply to check, NULL tolerated
 * @return false for NULL, true otherwise
 */
bool echo_reply_is_valid(t_echo_reply *reply);


void *factory_echo_reply_packet();

t_packet_processor_status deserializer_echo_reply(t_binary_stream *stream, void *return_value);
#endif //FT_PING_ECHO_REPLY_PACKET_H
