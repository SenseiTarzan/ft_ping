//
// Created by gcaptari on 02/08/2026.
//

#include "../binary_stream/binary_stream.h"
#include "packet_processors.h"

#include <stdio.h>
#include <stdlib.h>

static t_packet_processor packet_processor[255];

#define PACKET_PROCESSOR_COUNT (sizeof(packet_processor) / sizeof(packet_processor[0]))


bool packet_processor_status_is_failed(const t_packet_processor_status status) {
    return status != PACKET_PROCESSOR_STATUS_OK;
}

bool packet_processor_status_is_success(const t_packet_processor_status status) {
    return status == PACKET_PROCESSOR_STATUS_OK;
}

const char *packet_processor_status_message(const t_packet_processor_status status) {
    switch (status) {
        case PACKET_PROCESSOR_STATUS_OK:
            return "ok";
        case PACKET_PROCESSOR_STATUS_FAILURE_MALLOC:
            return "cannot allocate memory";
        case PACKET_PROCESSOR_STATUS_FAILURE_NO_DATA:
            return "data is null";
        case PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE:
            return "cannot serialize the packet";
        case PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE:
            return "cannot deserialize the packet";
        case PACKET_PROCESSOR_STATUS_FAILURE_CHECKSUM:
            return "checksum mismatch";
        default:
            return "failure";
    }
}

bool is_valid_packet_processor_id(const int id) {
    return id >= 0 && (size_t)id < PACKET_PROCESSOR_COUNT;
}

void register_packet_processor(const int id,
    t_packet_processor_status (*pre_serializer)(t_binary_stream *, const void *),
    t_packet_processor_status (*serializer)(t_binary_stream *, const void *),
    t_packet_processor_status (*post_serializer)(t_binary_stream *, const void *),
    void * (*constructor)(),
    t_packet_processor_status (*pre_deserializer)(t_binary_stream *, void *),
    t_packet_processor_status (*deserializer)(t_binary_stream *, void *),
    void (*destructor)(void * ),
    bool (*handler)(void *)) {
    if (!is_valid_packet_processor_id(id)) {
        return;
    }
    packet_processor[id].pre_serializer = pre_serializer;
    packet_processor[id].serializer = serializer;
    packet_processor[id].post_serializer = post_serializer;

    packet_processor[id].constructor = constructor;

    packet_processor[id].pre_deserializer = pre_deserializer;
    packet_processor[id].deserializer = deserializer;

    packet_processor[id].destructor = destructor;

    packet_processor[id].handler = handler;
}


void unregister_packet_processor(const int id) {
    if (!is_valid_packet_processor_id(id)) {
        return;
    }
    packet_processor[id].pre_serializer = NULL;
    packet_processor[id].serializer = NULL;
    packet_processor[id].post_serializer = NULL;

    packet_processor[id].constructor = NULL;

    packet_processor[id].pre_deserializer = NULL;
    packet_processor[id].deserializer = NULL;

    packet_processor[id].destructor = NULL;

    packet_processor[id].handler = NULL;
}

/**
 * Release a packet owned by a processor.
 * @param processor processor the packet belongs to
 * @param packet packet to release, NULL tolerated
 *
 * Falls back on free() when the processor declares no destructor, which is
 * what a processor whose constructor is a plain malloc() expects.
 */
void packet_processor_destroy(const t_packet_processor *processor, void *packet) {
    if (packet == NULL) {
        return;
    }
    if (processor->destructor != NULL) {
        processor->destructor(packet);
    }else {
        free(packet);
    }
}


t_packet_processor* get_packet_processor(const int id) {
    if (!is_valid_packet_processor_id(id)) {
        return NULL;
    }
    return &packet_processor[id];
}

t_packet_processor_status packet_processor_serialize(const int id, t_binary_stream *stream, const void *packet) {
    if (stream == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_NO_DATA;
    }
    if (!is_valid_packet_processor_id(id)) {
        return PACKET_PROCESSOR_STATUS_FAILED;
    }
    t_packet_processor* packet_processor = get_packet_processor(id);
    if (packet_processor == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_NOT_IMPLEMENTED;
    }
    t_packet_processor_status status = PACKET_PROCESSOR_STATUS_OK;
    if (packet_processor->pre_serializer != NULL) {
        status = packet_processor->pre_serializer(stream, packet);
        if (packet_processor_status_is_failed(status)) {
            return status;
        }
    }
    if (packet_processor->serializer == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_NOT_IMPLEMENTED;
    }
    status = packet_processor->serializer(stream, packet);
    if (packet_processor_status_is_failed(status)) {
        return status;
    }
    if (packet_processor->post_serializer != NULL) {
        status = packet_processor->post_serializer(stream, packet);
        if (packet_processor_status_is_failed(status)) {
            return status;
        }
    }
    return PACKET_PROCESSOR_STATUS_OK;
}

void *packet_processor_deserializer(t_binary_stream *stream) {
    if (stream == NULL || stream->data == NULL || stream->capacity == 0) {
        return NULL;
    }
    /* L'octet d'en-tete porte l'id du processeur. Il n'est pas consomme :
     * c'est au pre_deserializer d'avancer le curseur. */
    const int id = stream->data[0];
    if (!is_valid_packet_processor_id(id)) {
        return NULL;
    }
    const t_packet_processor* packet_processor = get_packet_processor(id);
    if (packet_processor == NULL) {
        return NULL;
    }
    if (packet_processor->constructor == NULL) {
        return NULL;
    }
    void *packet = packet_processor->constructor();
    if (packet == NULL) {
        return NULL;
    }
    t_packet_processor_status status = PACKET_PROCESSOR_STATUS_OK;
    if (packet_processor->pre_deserializer != NULL) {
        status = packet_processor->pre_deserializer(stream, packet);
        if (packet_processor_status_is_failed(status)) {
            packet_processor_destroy(packet_processor, packet);
            return NULL;
        }
    }
    if (packet_processor->deserializer == NULL) {
        packet_processor_destroy(packet_processor, packet);
        return NULL;
    }
    status = packet_processor->deserializer(stream, packet);
    if (packet_processor_status_is_failed(status)) {
        packet_processor_destroy(packet_processor, packet);
        return NULL;
    }
    return packet;
}

bool packet_processor_handler(const int id, void *packet) {
    if (!is_valid_packet_processor_id(id)) {
        return false;
    }
    const t_packet_processor *packet_processor = get_packet_processor(id);
    if (packet_processor == NULL) {
        return false;
    }
    if (packet_processor->handler == NULL) {
        return false;
    }
    /* Le paquet appartient au module a partir d'ici : il est detruit que le
     * handler l'accepte ou non. */
    const bool handled = packet_processor->handler(packet);
    packet_processor_destroy(packet_processor, packet);
    return handled;
}