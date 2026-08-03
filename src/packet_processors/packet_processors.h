//
// Created by gcaptari on 02/08/2026.
//

#ifndef FT_PING_PACKET_PROCESSORS_H
#define FT_PING_PACKET_PROCESSORS_H
#include <stdbool.h>

#include "../types.h"

enum e_packet_processor_status {
    PACKET_PROCESSOR_STATUS_FAILED = -1,
    PACKET_PROCESSOR_STATUS_OK = 0,
    PACKET_PROCESSOR_STATUS_FAILURE_MALLOC,
    PACKET_PROCESSOR_STATUS_FAILURE_NO_DATA,
    PACKET_PROCESSOR_STATUS_FAILURE_PRE_SERIALIZE,
    PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE,
    PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE,
    PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE,
    PACKET_PROCESSOR_STATUS_FAILURE_CHECKSUM,
    PACKET_PROCESSOR_STATUS_FAILURE_NOT_IMPLEMENTED
};

enum e_packet_network_type {
    PACKET_NETWORK_TYPE_UNKNOWN = -1,
    PACKET_NETWORK_TYPE_ETHERNET,
};

/**
 * Alias over enum e_packet_processor_status, so callers can write
 * t_packet_processor_status without repeating the @c enum keyword.
 */
typedef enum e_packet_processor_status t_packet_processor_status;

struct s_packet_processor {
    t_packet_processor_status (*pre_serializer)(t_binary_stream *, const void *);
    t_packet_processor_status (*serializer)(t_binary_stream *, const void *);
    t_packet_processor_status (*post_serializer)(t_binary_stream *, const void *);
    void * (*constructor)();
    t_packet_processor_status (*pre_deserializer)(t_binary_stream *, void *);
    t_packet_processor_status (*deserializer)(t_binary_stream *, void *);
    void (*destructor)(void *);
    bool (*handler)(void *);
};



/**
 * Tell whether a status reports a failure.
 * @param status status returned by a processor callback
 * @return true for anything other than PACKET_PROCESSOR_STATUS_OK
 */
bool packet_processor_status_is_failed(t_packet_processor_status status);

/**
 * Tell whether a status reports a success.
 * @param status status returned by a processor callback
 * @return true for PACKET_PROCESSOR_STATUS_OK
 */
bool packet_processor_status_is_success(t_packet_processor_status status);

/**
 * Human readable message attached to a status.
 * @param status status to describe
 * @return a static string, never NULL
 */
const char *packet_processor_status_message(t_packet_processor_status status);

bool is_valid_packet_processor_id(int id);

void register_packet_processor(int id,
    t_packet_processor_status (*pre_serializer)(t_binary_stream *, const void *),
    t_packet_processor_status (*serializer)(t_binary_stream *, const void *),
    t_packet_processor_status (*post_serializer)(t_binary_stream *, const void *),
    void * (*constructor)(),
    t_packet_processor_status (*pre_deserializer)(t_binary_stream *, void *),
    t_packet_processor_status (*deserializer)(t_binary_stream *, void *),
    void (*destructor)(void *),
    bool (*handler)(void *));
void unregister_packet_processor(int id);

t_packet_processor* get_packet_processor(int id);

t_packet_processor_status packet_processor_serialize(int id, t_binary_stream *stream, const void *packet);
void *packet_processor_deserializer(t_binary_stream *stream);
bool packet_processor_handler(int id, void *packet);
void packet_processor_destroy(const t_packet_processor *processor, void *packet);
#endif //FT_PING_PACKET_PROCESSORS_H
