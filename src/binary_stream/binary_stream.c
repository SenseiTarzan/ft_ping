//
// Created by gcaptari on 02/08/2026.
//

#include <string.h>
#include <stdlib.h>
#include "binary_stream.h"


bool binary_stream_status_is_failed(const t_binary_stream_status status) {
    return status != BINARY_STREAM_OK;
}

bool binary_stream_status_is_success(const t_binary_stream_status status) {
    return status == BINARY_STREAM_OK;
}

const char *binary_stream_status_message(const t_binary_stream_status status) {
    switch (status) {
        case BINARY_STREAM_OK:
            return "ok";
        case BINARY_STREAM_FAILURE_MALLOC:
            return "cannot allocate memory";
        case BINARY_STREAM_FAILURE_REALLOC:
            return "cannot reallocate memory";
        case BINARY_STREAM_FAILURE_NO_DATA:
            return "data is null";
        case BINARY_STREAM_FAILURE_READ:
            return "cannot read the buffer";
        case BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS:
            return "read past the end of the buffer";
        case BINARY_STREAM_FAILURE_SIZE_OUT_OF_CAPACITY:
            return "cannot recalculate capacity";
        case BINARY_STREAM_FAILURE_STRING_OUT_OF_BOUNDS:
            return "string length exceeds remaining data";
        case BINARY_STREAM_CORRUPT:
            return "binary_stream is corrupt";
        default:
            return "failure";
    }
}

static t_binary_stream_status binary_stream_recalculate_capacity(t_binary_stream *stream, size_t additional_size) {
    size_t new_capacity = stream->capacity + additional_size;
    if (new_capacity > stream->max_capacity) {
        return BINARY_STREAM_FAILURE_SIZE_OUT_OF_CAPACITY;
    }
    uint8_t *new_data = realloc(stream->data, new_capacity);
    if (!new_data) {
        return BINARY_STREAM_FAILURE_REALLOC;
    }
    stream->data = new_data;
    stream->capacity = new_capacity;
    return BINARY_STREAM_OK;
}

static t_binary_stream_status binary_stream_can_write(t_binary_stream *stream, size_t size) {
    if (stream->data == NULL) {
        return BINARY_STREAM_FAILURE_NO_DATA;
    }
    if (!binary_stream_has_capacity(stream, size)) {
        return BINARY_STREAM_FAILURE_SIZE_OUT_OF_CAPACITY;
    }
    size_t left = stream->capacity - stream->index;
    if (left < size) {
        return binary_stream_recalculate_capacity(stream, size - left);
    }
    return BINARY_STREAM_OK;
}

static t_binary_stream_status binary_stream_can_read(const t_binary_stream *stream, size_t size) {
    if (stream->data == NULL) {
        return BINARY_STREAM_FAILURE_NO_DATA;
    }
    if (size > stream->capacity - stream->index) {
        return BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS;
    }
    return BINARY_STREAM_OK;
}

static void init_methods_on_binary_stream(t_binary_stream *stream) {
    stream->methods.get_data = &binary_stream_get_data;
    stream->methods.is_empty = &binary_stream_is_empty;
    stream->methods.has_capacity = &binary_stream_has_capacity;
    stream->methods.print = &binary_stream_print;
    stream->methods.reset = &binary_stream_reset;
    stream->methods.copy = &binary_stream_copy;

    stream->methods.write_char = &binary_stream_write_char;
    stream->methods.write_char_le = &binary_stream_write_char_le;
    stream->methods.write_unsigned_char = &binary_stream_write_unsigned_char;
    stream->methods.write_unsigned_char_le = &binary_stream_write_unsigned_char_le;
    stream->methods.write_short = &binary_stream_write_short;
    stream->methods.write_short_le = &binary_stream_write_short_le;
    stream->methods.write_unsigned_short = &binary_stream_write_unsigned_short;
    stream->methods.write_unsigned_short_le = &binary_stream_write_unsigned_short_le;
    stream->methods.write_int = &binary_stream_write_int;
    stream->methods.write_int_le = &binary_stream_write_int_le;
    stream->methods.write_unsigned_int = &binary_stream_write_unsigned_int;
    stream->methods.write_unsigned_int_le = &binary_stream_write_unsigned_int_le;
    stream->methods.write_long = &binary_stream_write_long;
    stream->methods.write_long_le = &binary_stream_write_long_le;
    stream->methods.write_unsigned_long = &binary_stream_write_unsigned_long;
    stream->methods.write_unsigned_long_le = &binary_stream_write_unsigned_long_le;
    stream->methods.write_string = &binary_stream_write_string;

    stream->methods.read_char = &binary_stream_read_char;
    stream->methods.read_char_le = &binary_stream_read_char_le;
    stream->methods.read_unsigned_char = &binary_stream_read_unsigned_char;
    stream->methods.read_unsigned_char_le = &binary_stream_read_unsigned_char_le;
    stream->methods.read_short = &binary_stream_read_short;
    stream->methods.read_short_le = &binary_stream_read_short_le;
    stream->methods.read_unsigned_short = &binary_stream_read_unsigned_short;
    stream->methods.read_unsigned_short_le = &binary_stream_read_unsigned_short_le;
    stream->methods.read_int = &binary_stream_read_int;
    stream->methods.read_int_le = &binary_stream_read_int_le;
    stream->methods.read_unsigned_int = &binary_stream_read_unsigned_int;
    stream->methods.read_unsigned_int_le = &binary_stream_read_unsigned_int_le;
    stream->methods.read_long = &binary_stream_read_long;
    stream->methods.read_long_le = &binary_stream_read_long_le;
    stream->methods.read_unsigned_long = &binary_stream_read_unsigned_long;
    stream->methods.read_unsigned_long_le = &binary_stream_read_unsigned_long_le;
    stream->methods.read_string = &binary_stream_read_string;
}

t_binary_stream *create_binary_stream(uint8_t *data, size_t size, enum e_binary_stream_endian endian) {
    t_binary_stream *stream = malloc(sizeof(t_binary_stream));
    if (stream == NULL) {
        return NULL;
    }
    stream->endian = endian;
    stream->data = (uint8_t *)malloc(size);
    if (stream->data == NULL) {
        free(stream);
        return NULL;
    }
    memmove(stream->data, data, size);
    stream->capacity = size;
    stream->max_capacity = size;
    stream->index = 0;
    init_methods_on_binary_stream(stream);
    return stream;
}

t_binary_stream *create_binary_stream_with_capacity(size_t capacity, enum e_binary_stream_endian endian) {
    t_binary_stream *stream = malloc(sizeof(t_binary_stream));
    if (stream == NULL) {
        return NULL;
    }
    stream->endian = endian;
    stream->data = (uint8_t *)malloc(capacity);
    if (stream->data == NULL) {
        free(stream);
        return NULL;
    }
    stream->capacity = capacity;
    stream->max_capacity = INT32_MAX;
    stream->index = 0;
    init_methods_on_binary_stream(stream);
    return stream;
}

t_binary_stream *create_binary_stream_with_capacity_and_max_capacity(size_t capacity, size_t max_capacity, enum e_binary_stream_endian endian) {
    t_binary_stream *stream = malloc(sizeof(t_binary_stream));
    if (stream == NULL) {
        return NULL;
    }
    stream->endian = endian;
    stream->data = (uint8_t *)malloc(capacity);
    if (stream->data == NULL) {
        free(stream);
        return NULL;
    }
    stream->capacity = capacity;
    stream->max_capacity = max_capacity;
    stream->index = 0;
    init_methods_on_binary_stream(stream);
    return stream;
}

t_binary_stream *binary_stream_copy(t_binary_stream *stream) {
    t_binary_stream *copy = malloc(sizeof(t_binary_stream));
    if (copy == NULL) {
        return NULL;
    }
    copy->endian = stream->endian;
    copy->data = (uint8_t *)malloc(stream->capacity);
    if (copy->data == NULL) {
        free(copy);
        return NULL;
    }
    memmove(copy->data, stream->data, stream->capacity);
    copy->capacity = stream->capacity;
    copy->max_capacity = stream->max_capacity;
    copy->index = 0;
    init_methods_on_binary_stream(copy);
    return copy;
}

void binary_stream_reset(t_binary_stream *stream) {
    if (!stream) {
        return;
    }
    stream->index = 0;
}

void binary_stream_print(t_binary_stream *stream) {
    if (stream == NULL) {
        printf("t_binary_stream{(nil)}\n");
        return;
    }
    printf("t_binary_stream{endian=%i,data={", stream->endian);
    size_t last_index = stream->capacity - 1;
    for (size_t i = 0; i < stream->capacity; i++) {
        if (i == last_index) {
            printf("%02X", stream->data[i]);
        }else {
            printf("%02X,", stream->data[i]);
        }
    }
    printf("},capacity=%zu,max_capacity=%zu,index=%zu}\n", stream->capacity, stream->max_capacity, stream->index);
}

void binary_stream_status_print(const t_binary_stream_status status) {
    printf("t_binary_stream_status{status=%s,message=%s}\n", binary_stream_status_is_success(status) ? "SUCCESS" : "FAILURE", binary_stream_status_message(status));
}

void binary_stream_free(t_binary_stream *stream) {
    free(stream->data);
    free(stream);
}

bool binary_stream_is_empty(const t_binary_stream *stream) {
    return stream->index == 0;
}

bool binary_stream_has_capacity(const t_binary_stream *stream, const size_t additional) {
    if (additional > stream->max_capacity) {
        return false;
    }
    return stream->index <= stream->max_capacity - additional;
}

uint8_t *binary_stream_get_data(const t_binary_stream *stream) {
    if (stream->data == NULL) {
        return NULL;
    }
    return stream->data + stream->index;
}

static inline void encode_uint8(const uint8_t value, uint8_t *buffer) {
    buffer[0] = value;
}

static inline void encode_uint16_little_endian(const uint16_t value, uint8_t *buffer) {
    buffer[0] = (uint8_t)(value & 0xff);
    buffer[1] = (uint8_t)((value >> 8) & 0xff);
}

static inline void encode_uint16_big_endian(const uint16_t value, uint8_t *buffer) {
    buffer[0] = (uint8_t)((value >> 8) & 0xff);
    buffer[1] = (uint8_t)(value & 0xff);
}

static inline void encode_uint32_little_endian(const uint32_t value, uint8_t *buffer) {
    buffer[0] = (uint8_t)(value & 0xff);
    buffer[1] = (uint8_t)((value >> 8) & 0xff);
    buffer[2] = (uint8_t)((value >> 16) & 0xff);
    buffer[3] = (uint8_t)((value >> 24) & 0xff);
}

static inline void encode_uint32_big_endian(const uint32_t value, uint8_t *buffer) {
    buffer[0] = (uint8_t)((value >> 24) & 0xff);
    buffer[1] = (uint8_t)((value >> 16) & 0xff);
    buffer[2] = (uint8_t)((value >> 8) & 0xff);
    buffer[3] = (uint8_t)(value & 0xff);
}

static inline void encode_uint64_little_endian(const uint64_t value, uint8_t *buffer) {
    for (size_t i = 0; i < 8; i++) {
        buffer[i] = (uint8_t)((value >> (8 * i)) & 0xff);
    }
}

static inline void encode_uint64_big_endian(const uint64_t value, uint8_t *buffer) {
    for (size_t i = 0; i < 8; i++) {
        buffer[i] = (uint8_t)((value >> (8 * (7 - i))) & 0xff);
    }
}

static inline uint8_t decode_uint8(const uint8_t *buffer) {
    return buffer[0];
}

static inline uint16_t decode_uint16_little_endian(const uint8_t *buffer) {
    return (uint16_t)(((uint16_t)buffer[1] << 8) | (uint16_t)buffer[0]);
}

static inline uint16_t decode_uint16_big_endian(const uint8_t *buffer) {
    return (uint16_t)(((uint16_t)buffer[0] << 8) | (uint16_t)buffer[1]);
}

static inline uint32_t decode_uint32_little_endian(const uint8_t *buffer) {
    return ((uint32_t)buffer[3] << 24) | ((uint32_t)buffer[2] << 16)
         | ((uint32_t)buffer[1] << 8)  | (uint32_t)buffer[0];
}

static inline uint32_t decode_uint32_big_endian(const uint8_t *buffer) {
    return ((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1] << 16)
         | ((uint32_t)buffer[2] << 8)  | (uint32_t)buffer[3];
}

static inline uint64_t decode_uint64_little_endian(const uint8_t *buffer) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; i++) {
        value |= (uint64_t)buffer[i] << (8 * i);
    }
    return value;
}

static inline uint64_t decode_uint64_big_endian(const uint8_t *buffer) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; i++) {
        value |= (uint64_t)buffer[i] << (8 * (7 - i));
    }
    return value;
}

t_binary_stream_status binary_stream_write_char(t_binary_stream *stream, const int8_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(int8_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    encode_uint8((uint8_t)value, stream->data + stream->index);
    stream->index += sizeof(int8_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_char_le(t_binary_stream *stream, const int8_t value) {
    return binary_stream_write_char(stream, value);
}

t_binary_stream_status binary_stream_write_unsigned_char(t_binary_stream *stream, const uint8_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(uint8_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    encode_uint8(value, stream->data + stream->index);
    stream->index += sizeof(uint8_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_unsigned_char_le(t_binary_stream *stream, const uint8_t value) {
    return binary_stream_write_unsigned_char(stream, value);
}

t_binary_stream_status binary_stream_write_unsigned_short(t_binary_stream *stream, const uint16_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(uint16_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    uint8_t *buffer = stream->data + stream->index;
    if (stream->endian == BINARY_STREAM_ENDIAN_LITTLE) {
        encode_uint16_little_endian(value, buffer);
    }else {
        encode_uint16_big_endian(value, buffer);
    }
    stream->index += sizeof(uint16_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_unsigned_short_le(t_binary_stream *stream, const uint16_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(uint16_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    encode_uint16_little_endian(value, stream->data + stream->index);
    stream->index += sizeof(uint16_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_short(t_binary_stream *stream, const int16_t value) {
    return binary_stream_write_unsigned_short(stream, (uint16_t)value);
}

t_binary_stream_status binary_stream_write_short_le(t_binary_stream *stream, const int16_t value) {
    return binary_stream_write_unsigned_short_le(stream, (uint16_t)value);
}

t_binary_stream_status binary_stream_write_unsigned_int(t_binary_stream *stream, const uint32_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(uint32_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    uint8_t *buffer = stream->data + stream->index;
    if (stream->endian == BINARY_STREAM_ENDIAN_LITTLE) {
        encode_uint32_little_endian(value, buffer);
    }else {
        encode_uint32_big_endian(value, buffer);
    }
    stream->index += sizeof(uint32_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_unsigned_int_le(t_binary_stream *stream, const uint32_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(uint32_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    encode_uint32_little_endian(value, stream->data + stream->index);
    stream->index += sizeof(uint32_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_int(t_binary_stream *stream, const int32_t value) {
    return binary_stream_write_unsigned_int(stream, (uint32_t)value);
}

t_binary_stream_status binary_stream_write_int_le(t_binary_stream *stream, const int32_t value) {
    return binary_stream_write_unsigned_int_le(stream, (uint32_t)value);
}

t_binary_stream_status binary_stream_write_unsigned_long(t_binary_stream *stream, const uint64_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(uint64_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    uint8_t *buffer = stream->data + stream->index;
    if (stream->endian == BINARY_STREAM_ENDIAN_LITTLE) {
        encode_uint64_little_endian(value, buffer);
    }else {
        encode_uint64_big_endian(value, buffer);
    }
    stream->index += sizeof(uint64_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_unsigned_long_le(t_binary_stream *stream, const uint64_t value) {
    t_binary_stream_status status = binary_stream_can_write(stream, sizeof(uint64_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    encode_uint64_little_endian(value, stream->data + stream->index);
    stream->index += sizeof(uint64_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_write_long(t_binary_stream *stream, const int64_t value) {
    return binary_stream_write_unsigned_long(stream, (uint64_t)value);
}

t_binary_stream_status binary_stream_write_long_le(t_binary_stream *stream, const int64_t value) {
    return binary_stream_write_unsigned_long_le(stream, (uint64_t)value);
}

t_binary_stream_status binary_stream_read_unsigned_char(t_binary_stream *stream, uint8_t *value) {
    t_binary_stream_status status = binary_stream_can_read(stream, sizeof(uint8_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    *value = decode_uint8(stream->data + stream->index);
    stream->index += sizeof(uint8_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_unsigned_char_le(t_binary_stream *stream, uint8_t *value) {
    return binary_stream_read_unsigned_char(stream, value);
}

t_binary_stream_status binary_stream_read_char(t_binary_stream *stream, int8_t *value) {
    uint8_t raw = 0;
    t_binary_stream_status status = binary_stream_read_unsigned_char(stream, &raw);

    if (binary_stream_status_is_success(status)) {
        *value = (int8_t)raw;
    }
    return status;
}

t_binary_stream_status binary_stream_read_char_le(t_binary_stream *stream, int8_t *value) {
    return binary_stream_read_char(stream, value);
}

t_binary_stream_status binary_stream_read_unsigned_short(t_binary_stream *stream, uint16_t *value) {
    t_binary_stream_status status = binary_stream_can_read(stream, sizeof(uint16_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    if (stream->endian == BINARY_STREAM_ENDIAN_LITTLE) {
        *value = decode_uint16_little_endian(stream->data + stream->index);
    }else {
        *value = decode_uint16_big_endian(stream->data + stream->index);
    }
    stream->index += sizeof(uint16_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_unsigned_short_le(t_binary_stream *stream, uint16_t *value) {
    t_binary_stream_status status = binary_stream_can_read(stream, sizeof(uint16_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    *value = decode_uint16_little_endian(stream->data + stream->index);
    stream->index += sizeof(uint16_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_short(t_binary_stream *stream, int16_t *value) {
    uint16_t raw = 0;
    t_binary_stream_status status = binary_stream_read_unsigned_short(stream, &raw);

    if (binary_stream_status_is_success(status)) {
        *value = (int16_t)raw;
    }
    return status;
}

t_binary_stream_status binary_stream_read_short_le(t_binary_stream *stream, int16_t *value) {
    uint16_t raw = 0;
    t_binary_stream_status status = binary_stream_read_unsigned_short_le(stream, &raw);

    if (binary_stream_status_is_success(status)) {
        *value = (int16_t)raw;
    }
    return status;
}

t_binary_stream_status binary_stream_read_unsigned_int(t_binary_stream *stream, uint32_t *value) {
    t_binary_stream_status status = binary_stream_can_read(stream, sizeof(uint32_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    if (stream->endian == BINARY_STREAM_ENDIAN_LITTLE) {
        *value = decode_uint32_little_endian(stream->data + stream->index);
    }else {
        *value = decode_uint32_big_endian(stream->data + stream->index);
    }
    stream->index += sizeof(uint32_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_unsigned_int_le(t_binary_stream *stream, uint32_t *value) {
    t_binary_stream_status status = binary_stream_can_read(stream, sizeof(uint32_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    *value = decode_uint32_little_endian(stream->data + stream->index);
    stream->index += sizeof(uint32_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_int(t_binary_stream *stream, int32_t *value) {
    uint32_t raw = 0;
    t_binary_stream_status status = binary_stream_read_unsigned_int(stream, &raw);

    if (binary_stream_status_is_success(status)) {
        *value = (int32_t)raw;
    }
    return status;
}

t_binary_stream_status binary_stream_read_int_le(t_binary_stream *stream, int32_t *value) {
    uint32_t raw = 0;
    t_binary_stream_status status = binary_stream_read_unsigned_int_le(stream, &raw);

    if (binary_stream_status_is_success(status)) {
        *value = (int32_t)raw;
    }
    return status;
}

t_binary_stream_status binary_stream_read_unsigned_long(t_binary_stream *stream, uint64_t *value) {
    t_binary_stream_status status = binary_stream_can_read(stream, sizeof(uint64_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    if (stream->endian == BINARY_STREAM_ENDIAN_LITTLE) {
        *value = decode_uint64_little_endian(stream->data + stream->index);
    }else {
        *value = decode_uint64_big_endian(stream->data + stream->index);
    }
    stream->index += sizeof(uint64_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_unsigned_long_le(t_binary_stream *stream, uint64_t *value) {
    t_binary_stream_status status = binary_stream_can_read(stream, sizeof(uint64_t));

    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    *value = decode_uint64_little_endian(stream->data + stream->index);
    stream->index += sizeof(uint64_t);
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_long(t_binary_stream *stream, int64_t *value) {
    uint64_t raw = 0;
    t_binary_stream_status status = binary_stream_read_unsigned_long(stream, &raw);

    if (binary_stream_status_is_success(status)) {
        *value = (int64_t)raw;
    }
    return status;
}

t_binary_stream_status binary_stream_read_long_le(t_binary_stream *stream, int64_t *value) {
    uint64_t raw = 0;
    t_binary_stream_status status = binary_stream_read_unsigned_long_le(stream, &raw);

    if (binary_stream_status_is_success(status)) {
        *value = (int64_t)raw;
    }
    return status;
}

t_binary_stream_status binary_stream_write_string(t_binary_stream *stream, const char *value) {
    t_binary_stream_status status = BINARY_STREAM_OK;

    if (stream->data == NULL) {
        return BINARY_STREAM_FAILURE_NO_DATA;
    }
    uint32_t value_length = (uint32_t)strlen(value);
    size_t value_real_length = sizeof(uint32_t) + (size_t)value_length;

    status = binary_stream_can_write(stream, value_real_length);
    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    status = binary_stream_write_unsigned_int(stream, value_length);
    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    memcpy(stream->data + stream->index, value, value_length);
    stream->index += value_length;
    return BINARY_STREAM_OK;
}

t_binary_stream_status binary_stream_read_string(t_binary_stream *stream, const char **value) {
    t_binary_stream_status status = BINARY_STREAM_OK;
    uint32_t data_len = 0;

    if (stream->data == NULL) {
        return BINARY_STREAM_FAILURE_NO_DATA;
    }
    status = binary_stream_read_unsigned_int(stream, &data_len);
    if (binary_stream_status_is_failed(status)) {
        return status;
    }
    if (data_len > stream->capacity - stream->index) {
        return BINARY_STREAM_FAILURE_STRING_OUT_OF_BOUNDS;
    }
    char *copy = malloc((size_t)data_len + 1);
    if (copy == NULL) {
        return BINARY_STREAM_FAILURE_MALLOC;
    }
    memmove(copy, stream->data + stream->index, data_len);
    copy[data_len] = '\0';
    stream->index += data_len;
    *value = copy;
    return BINARY_STREAM_OK;
}
