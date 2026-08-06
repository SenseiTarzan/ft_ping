//
// Created by gcaptari on 02/08/2026.
//

#include "../binary_stream/binary_stream.h"
#include "packet_processors.h"

#include <stdio.h>
#include <stdlib.h>

t_packet_pool *packet_pool_new(int size) {
    t_packet_pool *pool;
    pool = (t_packet_pool *)malloc(sizeof(t_packet_pool));
    if (pool == NULL) {
        return NULL;
    }
    pool->size = size;
    pool->processor = calloc(pool->size, sizeof(t_packet_processor));
    if (pool->processor == NULL) {
        free(pool);
        return NULL;
    }
    return pool;
}
void packet_pool_free(t_packet_pool *pool) {
    if (pool == NULL) {
        return;
    }
    if (pool->processor != NULL) {
        free(pool->processor);
    }
    pool->processor = NULL;
    free(pool);
}

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

bool is_valid_packet_processor_id(const t_packet_pool *pool,const int id) {
    return id >= 0 && (size_t)id < pool->size;
}

void register_packet_processor(const t_packet_pool *pool, const int id,
    t_packet_processor_status (*pre_serializer)(t_binary_stream *, const void *packet),
    t_packet_processor_status (*serializer)(t_binary_stream *, const void *packet),
    t_packet_processor_status (*post_serializer)(t_binary_stream *, const void *packet),
    void *this,
    void * (*constructor)(void),
    t_packet_processor_status (*pre_deserializer)(t_binary_stream *, void *packet),
    t_packet_processor_status (*deserializer)(t_binary_stream *, void *packet),
    void (*destructor)(void *packet),
    bool (*handler)(void *this, struct sockaddr *addr, struct iphdr *ip_hdr,  void * packet, bool success)) {
    if (!is_valid_packet_processor_id(pool, id)) {
        return;
    }
    pool->processor[id].pre_serializer = pre_serializer;
    pool->processor[id].serializer = serializer;
    pool->processor[id].post_serializer = post_serializer;

    pool->processor[id].this = this;
    pool->processor[id].constructor = constructor;

    pool->processor[id].pre_deserializer = pre_deserializer;
    pool->processor[id].deserializer = deserializer;

    pool->processor[id].destructor = destructor;

    pool->processor[id].handler = handler;
}


void unregister_packet_processor(const t_packet_pool *pool, const int id) {
    if (!is_valid_packet_processor_id(pool, id)) {
        return;
    }
    pool->processor[id].pre_serializer = NULL;
    pool->processor[id].serializer = NULL;
    pool->processor[id].post_serializer = NULL;

    pool->processor[id].constructor = NULL;
    pool->processor[id].this = NULL;

    pool->processor[id].pre_deserializer = NULL;
    pool->processor[id].deserializer = NULL;

    pool->processor[id].destructor = NULL;

    pool->processor[id].handler = NULL;
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


t_packet_processor* get_packet_processor(const t_packet_pool *pool, const int id) {
    if (!is_valid_packet_processor_id(pool, id)) {
        return NULL;
    }
    return &pool->processor[id];
}

void packet_processor_set_this(const t_packet_pool *pool, const int id, void *this) {
    if (!is_valid_packet_processor_id(pool, id)) {
        return;
    }
    pool->processor[id].this = this;
}

void packet_processor_set_handler(const t_packet_pool *pool, const int id, bool (*handler)(void *this, struct sockaddr *addr, struct iphdr *ip_hdr,  void * packet, bool success)) {
    if (!is_valid_packet_processor_id(pool, id)) {
        return;
    }
    pool->processor[id].handler = handler;
}

t_packet_processor_status packet_processor_serialize(const t_packet_pool *pool, const int id, t_binary_stream *stream, const void *packet) {
    if (stream == NULL) {
        return PACKET_PROCESSOR_STATUS_FAILURE_NO_DATA;
    }
    if (!is_valid_packet_processor_id(pool, id)) {
        return PACKET_PROCESSOR_STATUS_FAILED;
    }
    t_packet_processor* packet_processor = get_packet_processor(pool, id);
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

void *packet_processor_deserializer(const t_packet_pool *pool, t_binary_stream *stream) {
    if (stream == NULL || stream->data == NULL || stream->capacity == 0) {
        return NULL;
    }
    const uint8_t *buffer = stream->methods.get_data(stream);
    const int id = buffer[0];
    if (!is_valid_packet_processor_id(pool, id)) {
        return NULL;
    }
    const t_packet_processor* packet_processor = get_packet_processor(pool, id);
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

bool packet_processor_handler(const t_packet_pool *pool, const int id, struct sockaddr *addr, struct iphdr *ip_hdr,  void *packet) {
    if (!is_valid_packet_processor_id(pool, id)) {
        return false;
    }
    const t_packet_processor *packet_processor = get_packet_processor(pool, id);
    if (packet_processor == NULL) {
        return false;
    }
    if (packet_processor->handler == NULL) {
        return false;
    }
    /* Le paquet appartient au module a partir d'ici : il est detruit que le
     * handler l'accepte ou non. */
    const bool handled = packet_processor->handler(packet_processor->this, addr, ip_hdr,  packet, packet != NULL);
    packet_processor_destroy(packet_processor, packet);
    return handled;
}