//
// Created by gcaptari on 02/08/2026.
//

#ifndef FT_PING_BINARY_STREAM_H
#define FT_PING_BINARY_STREAM_H
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../types.h"

enum e_binary_stream_status {
    BINARY_STREAM_OK,
    BINARY_STREAM_FAILURE,
    BINARY_STREAM_FAILURE_MALLOC,
    BINARY_STREAM_FAILURE_REALLOC,
    BINARY_STREAM_FAILURE_NO_DATA,
    BINARY_STREAM_FAILURE_READ,
    BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS,
    BINARY_STREAM_FAILURE_SIZE_OUT_OF_CAPACITY,
    BINARY_STREAM_FAILURE_STRING_OUT_OF_BOUNDS,
    BINARY_STREAM_CORRUPT
};

/**
 * Alias over enum e_binary_stream_status, so callers can write
 * t_binary_stream_status without repeating the @c enum keyword.
 */
typedef enum e_binary_stream_status t_binary_stream_status;

enum e_binary_stream_endian {
    BINARY_STREAM_ENDIAN_LITTLE = 0,
    BINARY_STREAM_ENDIAN_BIG
};

struct s_binary_stream {
    uint8_t *data;
    enum e_binary_stream_endian endian;
    size_t max_capacity;
    size_t capacity;
    size_t index;
    struct {
        uint8_t *(*get_data)(const t_binary_stream *stream);
        bool (*is_empty)(const t_binary_stream *stream);
        bool (*has_capacity)(const t_binary_stream *stream, size_t additional);
        void (*reset)(t_binary_stream *stream);
        void (*print)(t_binary_stream *stream);
        t_binary_stream *(*copy)(t_binary_stream *stream);

        t_binary_stream_status (*write_char)(t_binary_stream *stream, int8_t value);
        t_binary_stream_status (*write_char_le)(t_binary_stream *stream, int8_t value);
        t_binary_stream_status (*write_unsigned_char)(t_binary_stream *stream, uint8_t value);
        t_binary_stream_status (*write_unsigned_char_le)(t_binary_stream *stream, uint8_t value);
        t_binary_stream_status (*write_short)(t_binary_stream *stream, int16_t value);
        t_binary_stream_status (*write_short_le)(t_binary_stream *stream, int16_t value);
        t_binary_stream_status (*write_unsigned_short)(t_binary_stream *stream, uint16_t value);
        t_binary_stream_status (*write_unsigned_short_le)(t_binary_stream *stream, uint16_t value);
        t_binary_stream_status (*write_int)(t_binary_stream *stream, int32_t value);
        t_binary_stream_status (*write_int_le)(t_binary_stream *stream, int32_t value);
        t_binary_stream_status (*write_unsigned_int)(t_binary_stream *stream, uint32_t value);
        t_binary_stream_status (*write_unsigned_int_le)(t_binary_stream *stream, uint32_t value);
        t_binary_stream_status (*write_long)(t_binary_stream *stream, int64_t value);
        t_binary_stream_status (*write_long_le)(t_binary_stream *stream, int64_t value);
        t_binary_stream_status (*write_unsigned_long)(t_binary_stream *stream, uint64_t value);
        t_binary_stream_status (*write_unsigned_long_le)(t_binary_stream *stream, uint64_t value);
        t_binary_stream_status (*write_string)(t_binary_stream *stream, const char *value);

        t_binary_stream_status (*read_char)(t_binary_stream *stream, int8_t *value);
        t_binary_stream_status (*read_char_le)(t_binary_stream *stream, int8_t *value);
        t_binary_stream_status (*read_unsigned_char)(t_binary_stream *stream, uint8_t *value);
        t_binary_stream_status (*read_unsigned_char_le)(t_binary_stream *stream, uint8_t *value);
        t_binary_stream_status (*read_short)(t_binary_stream *stream, int16_t *value);
        t_binary_stream_status (*read_short_le)(t_binary_stream *stream, int16_t *value);
        t_binary_stream_status (*read_unsigned_short)(t_binary_stream *stream, uint16_t *value);
        t_binary_stream_status (*read_unsigned_short_le)(t_binary_stream *stream, uint16_t *value);
        t_binary_stream_status (*read_int)(t_binary_stream *stream, int32_t *value);
        t_binary_stream_status (*read_int_le)(t_binary_stream *stream, int32_t *value);
        t_binary_stream_status (*read_unsigned_int)(t_binary_stream *stream, uint32_t *value);
        t_binary_stream_status (*read_unsigned_int_le)(t_binary_stream *stream, uint32_t *value);
        t_binary_stream_status (*read_long)(t_binary_stream *stream, int64_t *value);
        t_binary_stream_status (*read_long_le)(t_binary_stream *stream, int64_t *value);
        t_binary_stream_status (*read_unsigned_long)(t_binary_stream *stream, uint64_t *value);
        t_binary_stream_status (*read_unsigned_long_le)(t_binary_stream *stream, uint64_t *value);
        t_binary_stream_status (*read_string)(t_binary_stream *stream, const char **value);
    } methods;
};




/**
 * @file binary_stream.h
 *
 * Read/write cursor over a byte buffer.
 *
 * A stream owns three sizes:
 *   - @c capacity     bytes currently allocated
 *   - @c index        cursor, shared by reads and writes
 *   - @c max_capacity ceiling the buffer may grow to
 *
 * Writers grow the buffer on demand, up to @c max_capacity. Readers never
 * grow it and fail past @c capacity. There is a single cursor, so switching
 * from writing to reading requires binary_stream_reset().
 *
 * Every read and write returns a t_binary_stream_status by value.
 * BINARY_STREAM_OK means the operation went through, any other value tells
 * why it did not. Nothing to free.
 *
 * Payload values use fixed-width types, so the byte count written is the
 * same on every platform. The historical names are kept:
 *   char -> int8_t, short -> int16_t, int -> int32_t, long -> int64_t,
 * each with an unsigned counterpart. Sizes and offsets stay @c size_t,
 * which is the type the C memory functions expect.
 *
 * The @c _le suffix forces little endian regardless of the stream setting.
 * Functions without it follow the stream's own endianness. There is no
 * @c _be variant.
 */

/**
 * Create a stream holding a copy of an existing buffer.
 * @param data source bytes, copied
 * @param size number of bytes to copy
 * @param endian byte order used by the non @c _le accessors
 * @return the stream, or NULL on allocation failure. @c max_capacity is set
 *         to @p size, so the stream is readable but cannot grow.
 */
t_binary_stream *create_binary_stream(uint8_t *data, size_t size, enum e_binary_stream_endian endian);

/**
 * Create an empty stream that may grow up to @c INT32_MAX.
 * @param capacity bytes allocated up front, content left uninitialised
 * @param endian byte order used by the non @c _le accessors
 * @return the stream, or NULL on allocation failure
 */
t_binary_stream *create_binary_stream_with_capacity(size_t capacity, enum e_binary_stream_endian endian);

/**
 * Create an empty stream with an explicit growth ceiling.
 * @param capacity bytes allocated up front, content left uninitialised
 * @param max_capacity ceiling the buffer may grow to
 * @param endian byte order used by the non @c _le accessors
 * @return the stream, or NULL on allocation failure
 */
t_binary_stream *create_binary_stream_with_capacity_and_max_capacity(size_t capacity, size_t max_capacity, enum e_binary_stream_endian endian);

/**
 * Duplicate a stream, buffer included.
 * @param stream source stream
 * @return an independent stream with the same bytes, endianness and
 *         @c max_capacity, its cursor rewound to 0. NULL on allocation
 *         failure. Free it with binary_stream_free().
 */
t_binary_stream *binary_stream_copy(t_binary_stream *stream);


/**
 * Tell whether a status reports a failure.
 * @param status status returned by a read or a write
 * @return true for anything other than BINARY_STREAM_OK
 */
bool binary_stream_status_is_failed(t_binary_stream_status status);

/**
 * Tell whether a status reports a success.
 * @param status status returned by a read or a write
 * @return true for BINARY_STREAM_OK
 */
bool binary_stream_status_is_success(t_binary_stream_status status);

/**
 * Human readable message attached to a status.
 * @param status status to describe
 * @return a static string, never NULL
 */
const char *binary_stream_status_message(t_binary_stream_status status);

/**
 * Rewind the cursor to 0. The bytes are left untouched.
 * @param stream stream to rewind
 */
void binary_stream_reset(t_binary_stream *stream);

/**
 * Dump the stream to stdout, for debugging.
 * @param stream stream to dump, NULL tolerated
 */
void binary_stream_print(t_binary_stream *stream);

/**
 * Release a stream and its buffer.
 * @param stream stream to free
 */
void binary_stream_free(t_binary_stream *stream);

/**
 * Tell whether the cursor is still at the start.
 * @param stream stream to test
 * @return true if @c index is 0
 */
bool binary_stream_is_empty(const t_binary_stream *stream);

/**
 * Tell whether @p additional bytes can still be written at the cursor,
 * growing the buffer if needed.
 * @param stream stream to test
 * @param additional number of bytes to accommodate
 * @return true if @c index + @p additional fits within @c max_capacity
 */
bool binary_stream_has_capacity(const t_binary_stream *stream, size_t additional);

/**
 * Address of the cursor inside the buffer.
 * @param stream stream to inspect
 * @return @c data + @c index, or NULL if the buffer is NULL. Points into the
 *         stream, so it is invalidated by any write that triggers a growth.
 */
uint8_t *binary_stream_get_data(const t_binary_stream *stream);

/**
 * Write a signed 8 bit value.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_char(t_binary_stream *stream, int8_t value);

/**
 * Write a signed 8 bit value. Identical to binary_stream_write_char(): a
 * single byte has no byte order. Kept for symmetry with the wider types.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_char_le(t_binary_stream *stream, int8_t value);

/**
 * Write an unsigned 8 bit value.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_char(t_binary_stream *stream, uint8_t value);

/**
 * Write an unsigned 8 bit value. Identical to
 * binary_stream_write_unsigned_char(): a single byte has no byte order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_char_le(t_binary_stream *stream, uint8_t value);

/**
 * Write a signed 16 bit value in the stream's byte order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_short(t_binary_stream *stream, int16_t value);

/**
 * Write a signed 16 bit value in little endian, whatever the stream's order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_short_le(t_binary_stream *stream, int16_t value);

/**
 * Write an unsigned 16 bit value in the stream's byte order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_short(t_binary_stream *stream, uint16_t value);

/**
 * Write an unsigned 16 bit value in little endian, whatever the stream's order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_short_le(t_binary_stream *stream, uint16_t value);

/**
 * Write a signed 32 bit value in the stream's byte order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_int(t_binary_stream *stream, int32_t value);

/**
 * Write a signed 32 bit value in little endian, whatever the stream's order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_int_le(t_binary_stream *stream, int32_t value);

/**
 * Write an unsigned 32 bit value in the stream's byte order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_int(t_binary_stream *stream, uint32_t value);

/**
 * Write an unsigned 32 bit value in little endian, whatever the stream's order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_int_le(t_binary_stream *stream, uint32_t value);

/**
 * Write a signed 64 bit value in the stream's byte order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_long(t_binary_stream *stream, int64_t value);

/**
 * Write a signed 64 bit value in little endian, whatever the stream's order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_long_le(t_binary_stream *stream, int64_t value);

/**
 * Write an unsigned 64 bit value in the stream's byte order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_long(t_binary_stream *stream, uint64_t value);

/**
 * Write an unsigned 64 bit value in little endian, whatever the stream's order.
 * @param stream target stream
 * @param value value to write
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_unsigned_long_le(t_binary_stream *stream, uint64_t value);

/**
 * Write a length-prefixed string: an unsigned 32 bit length in the stream's
 * byte order, followed by the raw characters. The terminating NUL is not
 * written, so the byte order only affects the prefix.
 * @param stream target stream
 * @param value NUL-terminated source string
 * @return BINARY_STREAM_OK, or the failure that stopped the write
 */
t_binary_stream_status binary_stream_write_string(t_binary_stream *stream, const char *value);

/**
 * Read a signed 8 bit value.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_char(t_binary_stream *stream, int8_t *value);

/**
 * Read a signed 8 bit value. Identical to binary_stream_read_char(): a
 * single byte has no byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_char_le(t_binary_stream *stream, int8_t *value);

/**
 * Read an unsigned 8 bit value.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_char(t_binary_stream *stream, uint8_t *value);

/**
 * Read an unsigned 8 bit value. Identical to
 * binary_stream_read_unsigned_char(): a single byte has no byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_char_le(t_binary_stream *stream, uint8_t *value);

/**
 * Read a signed 16 bit value using the stream's byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_short(t_binary_stream *stream, int16_t *value);

/**
 * Read a signed 16 bit little endian value, whatever the stream's order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_short_le(t_binary_stream *stream, int16_t *value);

/**
 * Read an unsigned 16 bit value using the stream's byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_short(t_binary_stream *stream, uint16_t *value);

/**
 * Read an unsigned 16 bit little endian value, whatever the stream's order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_short_le(t_binary_stream *stream, uint16_t *value);

/**
 * Read a signed 32 bit value using the stream's byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_int(t_binary_stream *stream, int32_t *value);

/**
 * Read a signed 32 bit little endian value, whatever the stream's order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_int_le(t_binary_stream *stream, int32_t *value);

/**
 * Read an unsigned 32 bit value using the stream's byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_int(t_binary_stream *stream, uint32_t *value);

/**
 * Read an unsigned 32 bit little endian value, whatever the stream's order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_int_le(t_binary_stream *stream, uint32_t *value);

/**
 * Read a signed 64 bit value using the stream's byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_long(t_binary_stream *stream, int64_t *value);

/**
 * Read a signed 64 bit little endian value, whatever the stream's order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_long_le(t_binary_stream *stream, int64_t *value);

/**
 * Read an unsigned 64 bit value using the stream's byte order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_long(t_binary_stream *stream, uint64_t *value);

/**
 * Read an unsigned 64 bit little endian value, whatever the stream's order.
 * @param stream source stream
 * @param value out, value read
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_READ_OUT_OF_BOUNDS past the end of the buffer
 */
t_binary_stream_status binary_stream_read_unsigned_long_le(t_binary_stream *stream, uint64_t *value);

/**
 * Read a length-prefixed string written by binary_stream_write_string().
 * @param stream source stream
 * @param value out, NUL-terminated copy allocated on the heap. The caller
 *              owns it and must free() it. Left untouched on failure.
 * @return BINARY_STREAM_OK, or BINARY_STREAM_FAILURE_STRING_OUT_OF_BOUNDS if the announced length
 *         exceeds the bytes left in the buffer, which is what stops a
 *         corrupted or hostile length from reading out of bounds.
 */
t_binary_stream_status binary_stream_read_string(t_binary_stream *stream, const char **value);

/**
 * Dump a status to stdout, for debugging.
 * @param status status to dump
 */
void binary_stream_status_print(t_binary_stream_status status);

#endif //FT_PING_BINARY_STREAM_H
