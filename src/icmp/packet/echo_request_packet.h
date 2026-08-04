//
// Created by gcaptari on 02/08/2026.
//

#ifndef FT_PING_ECHO_REQUEST_PACKET_H
#define FT_PING_ECHO_REQUEST_PACKET_H
#include <stdbool.h>
#include <stdint.h>
#include "../binary_stream_icmp.h"
#include "../../packet_processors/packet_processors.h"
#include <sys/time.h>
struct s_echo_request {
    t_header_icmp packet_header;
    uint16_t identifier;
    uint16_t sequence;
    struct timeval timestamp;
    char *payload;
};

/**
 * Allocate an echo request, its ICMP header already filled in.
 * @param identifier identifier carried by the request
 * @param sequence sequence number carried by the request
 * @return the request, or NULL on allocation failure. The caller owns it and
 *         must free() it.
 */
t_echo_request *echo_request_new(uint8_t identifier, uint8_t sequence, char *payload);
/**
 * Tell whether a request may be sent.
 * @param request request to check, NULL tolerated
 * @return false for NULL, true otherwise
 */
bool echo_request_is_valid(t_echo_request *request);

t_packet_processor_status serializer_echo_request(t_binary_stream *stream, const void *buffer);

#endif //FT_PING_ECHO_REQUEST_PACKET_H
