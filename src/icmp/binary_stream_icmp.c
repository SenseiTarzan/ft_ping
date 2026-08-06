//
// Created by gcaptari on 02/08/2026.
//

#include "binary_stream_icmp.h"

#include <string.h>

static uint16_t icmp_checksum(const void *buf, size_t len) {
    const uint8_t *ptr = buf;
    uint32_t sum = 0;

    while (len > 1) {
        sum += ((uint32_t)ptr[0] << 8) | (uint32_t)ptr[1];
        ptr += 2;
        len -= 2;
    }

    if (len == 1) {
        sum += (uint32_t)ptr[0] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}


static void inline encode_uint16_little_endian(const uint16_t value, uint8_t *buffer) {
    buffer[0] = (uint8_t)(value & 0xff);
    buffer[1] = (uint8_t)((value >> 8) & 0xff);
}

static void inline encode_uint16_big_endian(const uint16_t value, uint8_t *buffer) {
    buffer[0] = (uint8_t)((value >> 8) & 0xff);
    buffer[1] = (uint8_t)(value & 0xff);
}

t_binary_stream_status binary_stream_icmp_write_checksum(t_binary_stream * stream, size_t index, size_t len) {
    if (stream->data == NULL) {
        return BINARY_STREAM_FAILURE_NO_DATA;
    }
    if (index + sizeof(uint16_t) > stream->capacity) {
        return BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS;
    }
    memset(stream->data + index, 0, sizeof(uint16_t));
    uint16_t checksum = icmp_checksum(stream->data, len);

    uint8_t *buffer = stream->data + index;
    if (stream->endian == BINARY_STREAM_ENDIAN_LITTLE) {
        encode_uint16_little_endian(checksum, buffer);
    } else {
        encode_uint16_big_endian(checksum, buffer);
    }
    return BINARY_STREAM_OK;
}


t_binary_stream_status binary_stream_icmp_check_checksum(t_binary_stream * stream, size_t len) {
    uint16_t checksum = icmp_checksum(stream->data, len);
    if (checksum != 0) {
        return BINARY_STREAM_CORRUPT;
    }
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_icmp_write_header(t_binary_stream *stream, const void *data) {
    if (stream->data == NULL) {
        return BINARY_STREAM_FAILURE_NO_DATA;
    }
    t_header_icmp *header = (t_header_icmp *) data;
    t_binary_stream_status status = BINARY_STREAM_OK;

    status = stream->methods.write_char(stream, header->type);
    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    status = stream->methods.write_char(stream, header->code);
    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    return stream->methods.write_unsigned_short(stream, header->checksum);
}

t_binary_stream_status binary_stream_icmp_read_header(t_binary_stream * stream, void *data) {
    t_header_icmp *header = (t_header_icmp *) data;
    t_binary_stream_status status = BINARY_STREAM_OK;

    if (stream->capacity < sizeof(t_header_icmp)) {
        return BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS;
    }

    status = stream->methods.read_char(stream, &header->type);
    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    status = stream->methods.read_char(stream, &header->code);
    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    return stream->methods.read_unsigned_short(stream, &header->checksum);
}
