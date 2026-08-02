/*
 * Tests unitaires pour binary_stream.
 *
 * Chaque test tourne dans un processus fils : un segfault ou un abort est
 * rapporté comme un échec au lieu de tuer toute la suite.
 *
 * Depuis la racine du projet :
 *   cmake --build build --target test_binary_stream && ./build/test_binary_stream
 *
 * Ou directement :
 *   cc -g3 -O0 -std=c99 -Wall -Wextra -o test_binary_stream \
 *      tests/test_binary_stream.c src/binary_stream/binary_stream.c
 *
 * Avec sanitizers (recommandé, détecte les débordements et les UB) :
 *   cc -g3 -O0 -std=c99 -fsanitize=address,undefined \
 *      -o test_binary_stream tests/test_binary_stream.c \
 *      src/binary_stream/binary_stream.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../src/binary_stream/binary_stream.h"

/* ------------------------------------------------------------------ */
/* Macros d'assertion                                                  */
/* ------------------------------------------------------------------ */

#define FAIL(fmt, ...)                                                      \
    do {                                                                    \
        printf("\n      %s:%d\n      " fmt "\n", __FILE__, __LINE__,        \
               ##__VA_ARGS__);                                              \
        exit(1);                                                            \
    } while (0)

#define ASSERT_TRUE(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            FAIL("attendu vrai : %s", #cond);                               \
        }                                                                   \
    } while (0)

#define ASSERT_FALSE(cond)                                                  \
    do {                                                                    \
        if ((cond)) {                                                       \
            FAIL("attendu faux : %s", #cond);                               \
        }                                                                   \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                \
    do {                                                                    \
        if ((ptr) == NULL) {                                                \
            FAIL("attendu non NULL : %s", #ptr);                            \
        }                                                                   \
    } while (0)

#define ASSERT_EQ_LL(expected, actual)                                      \
    do {                                                                    \
        long long e_ = (long long)(expected);                               \
        long long a_ = (long long)(actual);                                 \
        if (e_ != a_) {                                                     \
            FAIL("%s : attendu %lld, obtenu %lld", #actual, e_, a_);        \
        }                                                                   \
    } while (0)

#define ASSERT_EQ_ULL(expected, actual)                                     \
    do {                                                                    \
        unsigned long long e_ = (unsigned long long)(expected);             \
        unsigned long long a_ = (unsigned long long)(actual);               \
        if (e_ != a_) {                                                     \
            FAIL("%s : attendu %llu, obtenu %llu", #actual, e_, a_);        \
        }                                                                   \
    } while (0)

#define ASSERT_EQ_STR(expected, actual)                                     \
    do {                                                                    \
        const char *e_ = (expected);                                        \
        const char *a_ = (actual);                                          \
        if (a_ == NULL || strcmp(e_, a_) != 0) {                            \
            FAIL("%s : attendu \"%s\", obtenu \"%s\"", #actual, e_,         \
                 a_ ? a_ : "(null)");                                       \
        }                                                                   \
    } while (0)

static void assert_bytes(const uint8_t *expected,
                         const uint8_t *actual, size_t n,
                         const char *what, int line) {
    size_t i;
    if (memcmp(expected, actual, n) == 0) {
        return;
    }
    printf("\n      %s:%d\n      %s : octets differents\n", __FILE__, line, what);
    printf("      attendu :");
    for (i = 0; i < n; i++) {
        printf(" %02X", expected[i]);
    }
    printf("\n      obtenu  :");
    for (i = 0; i < n; i++) {
        printf(" %02X", actual[i]);
    }
    printf("\n");
    exit(1);
}

#define ASSERT_BYTES(expected, actual, n)                                   \
    assert_bytes((expected), (actual), (n), #actual, __LINE__)

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Dit si le status est en succès. */
static int ok(t_binary_stream_status status) {
    return status == BINARY_STREAM_OK;
}

/* Dit si le status est en échec, quelle qu'en soit la cause. */
static int failed(t_binary_stream_status status) {
    return status != BINARY_STREAM_OK;
}

#define ASSERT_WRITE_OK(call)                                               \
    do {                                                                    \
        if (!ok(call)) {                                                    \
            FAIL("attendu BINARY_STREAM_SUCCESS : %s", #call);              \
        }                                                                   \
    } while (0)

#define ASSERT_WRITE_FAILS(call)                                            \
    do {                                                                    \
        if (!failed(call)) {                                                \
            FAIL("attendu BINARY_STREAM_FAILURE : %s", #call);              \
        }                                                                   \
    } while (0)

/* ================================================================== */
/* Création                                                            */
/* ================================================================== */

static void test_create_with_capacity(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_NOT_NULL(s);
    ASSERT_NOT_NULL(s->data);
    ASSERT_EQ_ULL(16, s->capacity);
    ASSERT_EQ_ULL(0, s->index);
    ASSERT_EQ_LL(BINARY_STREAM_ENDIAN_BIG, s->endian);
    ASSERT_TRUE(s->max_capacity > s->capacity);
    binary_stream_free(s);
}

static void test_create_with_max_capacity(void) {
    t_binary_stream *s = create_binary_stream_with_capacity_and_max_capacity(
        4, 64, BINARY_STREAM_ENDIAN_LITTLE);
    ASSERT_NOT_NULL(s);
    ASSERT_EQ_ULL(4, s->capacity);
    ASSERT_EQ_ULL(64, s->max_capacity);
    ASSERT_EQ_ULL(0, s->index);
    ASSERT_EQ_LL(BINARY_STREAM_ENDIAN_LITTLE, s->endian);
    binary_stream_free(s);
}

static void test_create_from_data_copies(void) {
    uint8_t src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    t_binary_stream *s = create_binary_stream(src, sizeof(src), BINARY_STREAM_ENDIAN_BIG);
    ASSERT_NOT_NULL(s);
    ASSERT_EQ_ULL(4, s->capacity);
    ASSERT_EQ_ULL(0, s->index);
    ASSERT_BYTES(src, s->data, 4);
    /* la copie doit etre independante du buffer source */
    ASSERT_TRUE(s->data != src);
    src[0] = 0x00;
    ASSERT_EQ_LL(0xDE, s->data[0]);
    binary_stream_free(s);
}

/* ================================================================== */
/* Encodage : disposition exacte des octets                            */
/* ================================================================== */

static void test_write_short_big_endian_layout(void) {
    uint8_t expected[2] = { 0x12, 0x34 };
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x1234));
    ASSERT_BYTES(expected, s->data, 2);
    ASSERT_EQ_ULL(2, s->index);
    binary_stream_free(s);
}

static void test_write_short_little_endian_layout(void) {
    uint8_t expected[2] = { 0x34, 0x12 };
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_LITTLE);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x1234));
    ASSERT_BYTES(expected, s->data, 2);
    binary_stream_free(s);
}

/* write_short_le doit forcer le little endian meme sur un stream big endian */
static void test_write_short_le_forces_little_endian(void) {
    uint8_t expected[2] = { 0x34, 0x12 };
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_short_le(s, 0x1234));
    ASSERT_BYTES(expected, s->data, 2);
    binary_stream_free(s);
}

static void test_write_int_big_endian_layout(void) {
    uint8_t expected[4] = { 0x01, 0x02, 0x03, 0x04 };
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_int(s, 0x01020304));
    ASSERT_BYTES(expected, s->data, 4);
    ASSERT_EQ_ULL(4, s->index);
    binary_stream_free(s);
}

static void test_write_int_le_layout(void) {
    uint8_t expected[4] = { 0x04, 0x03, 0x02, 0x01 };
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_int_le(s, 0x01020304));
    ASSERT_BYTES(expected, s->data, 4);
    binary_stream_free(s);
}

static void test_write_long_big_endian_layout(void) {
    uint8_t expected[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_long(s, 0x0102030405060708LL));
    ASSERT_BYTES(expected, s->data, 8);
    ASSERT_EQ_ULL(8, s->index);
    binary_stream_free(s);
}

static void test_write_long_le_layout(void) {
    uint8_t expected[8] = { 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_long_le(s, 0x0102030405060708LL));
    ASSERT_BYTES(expected, s->data, 8);
    binary_stream_free(s);
}

static void test_write_string_layout(void) {
    /* longueur sur 4 octets big endian, puis les caracteres, sans NUL */
    uint8_t expected[7] = { 0x00, 0x00, 0x00, 0x03, 'a', 'b', 'c' };
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_string(s, "abc"));
    ASSERT_BYTES(expected, s->data, 7);
    ASSERT_EQ_ULL(7, s->index);
    binary_stream_free(s);
}

/* ================================================================== */
/* Aller-retour écriture / lecture                                     */
/* ================================================================== */

static void test_roundtrip_short_big_endian(void) {
    int16_t out = 0;
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_short(s, -12345));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_short(s, &out));
    ASSERT_EQ_LL(-12345, out);
    ASSERT_EQ_ULL(2, s->index);
    binary_stream_free(s);
}

static void test_roundtrip_short_little_endian(void) {
    int16_t out = 0;
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_LITTLE);
    ASSERT_WRITE_OK(s->methods.write_short(s, -12345));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_short(s, &out));
    ASSERT_EQ_LL(-12345, out);
    binary_stream_free(s);
}

static void test_roundtrip_unsigned_short_max(void) {
    uint16_t out = 0;
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_unsigned_short(s, 0xFFFF));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_unsigned_short(s, &out));
    ASSERT_EQ_ULL(0xFFFF, out);
    binary_stream_free(s);
}

static void test_roundtrip_int_extremes(void) {
    int32_t values[4];
    int32_t out = 0;
    size_t i;
    values[0] = INT_MIN;
    values[1] = -1;
    values[2] = 0;
    values[3] = INT_MAX;

    for (i = 0; i < 4; i++) {
        t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
        ASSERT_WRITE_OK(s->methods.write_int(s, values[i]));
        s->methods.reset(s);
        ASSERT_WRITE_OK(s->methods.read_int(s, &out));
        ASSERT_EQ_LL(values[i], out);
        binary_stream_free(s);
    }
}

static void test_roundtrip_unsigned_int_max(void) {
    uint32_t out = 0;
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_unsigned_int(s, 0xFFFFFFFFu));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_unsigned_int(s, &out));
    ASSERT_EQ_ULL(0xFFFFFFFFu, out);
    binary_stream_free(s);
}

static void test_roundtrip_unsigned_int_le(void) {
    uint32_t out = 0;
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_unsigned_int_le(s, 0xDEADBEEFu));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_unsigned_int_le(s, &out));
    ASSERT_EQ_ULL(0xDEADBEEFu, out);
    binary_stream_free(s);
}

static void test_roundtrip_long_extremes(void) {
    int64_t values[4];
    int64_t out = 0;
    size_t i;
    values[0] = INT64_MIN;
    values[1] = -1;
    values[2] = 0;
    values[3] = INT64_MAX;

    for (i = 0; i < 4; i++) {
        t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
        ASSERT_WRITE_OK(s->methods.write_long(s, values[i]));
        s->methods.reset(s);
        ASSERT_WRITE_OK(s->methods.read_long(s, &out));
        ASSERT_EQ_LL(values[i], out);
        binary_stream_free(s);
    }
}

static void test_roundtrip_unsigned_long_max(void) {
    uint64_t out = 0;
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_unsigned_long(s, UINT64_MAX));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_unsigned_long(s, &out));
    ASSERT_EQ_ULL(UINT64_MAX, out);
    binary_stream_free(s);
}

static void test_roundtrip_unsigned_long_le(void) {
    uint64_t out = 0;
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_unsigned_long_le(s, 0xDEADBEEFCAFEBABEull));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_unsigned_long_le(s, &out));
    ASSERT_EQ_ULL(0xDEADBEEFCAFEBABEull, out);
    binary_stream_free(s);
}

/* Les deux ordres doivent produire des octets miroirs l'un de l'autre */
static void test_long_endianness_is_mirrored(void) {
    t_binary_stream *be = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    t_binary_stream *le = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_LITTLE);
    size_t i;

    ASSERT_WRITE_OK(be->methods.write_unsigned_long(be, 0x0011223344556677ull));
    ASSERT_WRITE_OK(le->methods.write_unsigned_long(le, 0x0011223344556677ull));
    for (i = 0; i < 8; i++) {
        ASSERT_EQ_ULL(be->data[i], le->data[7 - i]);
    }
    binary_stream_free(le);
    binary_stream_free(be);
}

static void test_roundtrip_string(void) {
    const char *out = NULL;
    t_binary_stream *s = create_binary_stream_with_capacity(4, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_string(s, "AAAAAAAAAAA"));
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_string(s, &out));
    ASSERT_EQ_STR("AAAAAAAAAAA", out);
    free((void *)out);
    binary_stream_free(s);
}

static void test_roundtrip_empty_string(void) {
    const char *out = NULL;
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_string(s, ""));
    ASSERT_EQ_ULL(4, s->index);
    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_string(s, &out));
    ASSERT_EQ_STR("", out);
    free((void *)out);
    binary_stream_free(s);
}

/* Le scénario de main.c : short + unsigned short + string */
static void test_roundtrip_mixed_packet(void) {
    uint8_t expected[19] = {
        0x00, 0x07,
        0x2C, 0x39,
        0x00, 0x00, 0x00, 0x0B,
        'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A'
    };
    int16_t a = 0;
    uint16_t b = 0;
    const char *c = NULL;
    t_binary_stream *s = create_binary_stream_with_capacity(10, BINARY_STREAM_ENDIAN_BIG);

    ASSERT_WRITE_OK(s->methods.write_short(s, 7));
    ASSERT_WRITE_OK(s->methods.write_unsigned_short(s, 0x2C39));
    ASSERT_WRITE_OK(s->methods.write_string(s, "AAAAAAAAAAA"));
    ASSERT_EQ_ULL(19, s->index);
    ASSERT_BYTES(expected, s->data, 19);

    s->methods.reset(s);
    ASSERT_WRITE_OK(s->methods.read_short(s, &a));
    ASSERT_WRITE_OK(s->methods.read_unsigned_short(s, &b));
    ASSERT_WRITE_OK(s->methods.read_string(s, &c));
    ASSERT_EQ_LL(7, a);
    ASSERT_EQ_ULL(0x2C39, b);
    ASSERT_EQ_STR("AAAAAAAAAAA", c);
    ASSERT_EQ_ULL(19, s->index);

    free((void *)c);
    binary_stream_free(s);
}

/* Lecture d'un buffer reçu tel quel : le cas ft_ping */
static void test_read_from_fixed_buffer(void) {
    uint8_t wire[8] = { 0x00, 0x07, 0x78, 0xA7, 0x00, 0x00, 0x00, 0x00 };
    int16_t a = 0;
    uint16_t b = 0;
    uint32_t c = 0;
    t_binary_stream *s = create_binary_stream(wire, sizeof(wire), BINARY_STREAM_ENDIAN_BIG);

    ASSERT_WRITE_OK(s->methods.read_short(s, &a));
    ASSERT_WRITE_OK(s->methods.read_unsigned_short(s, &b));
    ASSERT_WRITE_OK(s->methods.read_unsigned_int(s, &c));
    ASSERT_EQ_LL(7, a);
    ASSERT_EQ_ULL(0x78A7, b);
    ASSERT_EQ_ULL(0, c);
    binary_stream_free(s);
}

/* ================================================================== */
/* Index et état                                                       */
/* ================================================================== */

static void test_index_advances_per_type(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(64, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_EQ_ULL(0, s->index);
    ASSERT_WRITE_OK(s->methods.write_short(s, 1));
    ASSERT_EQ_ULL(2, s->index);
    ASSERT_WRITE_OK(s->methods.write_unsigned_short(s, 1));
    ASSERT_EQ_ULL(4, s->index);
    ASSERT_WRITE_OK(s->methods.write_int(s, 1));
    ASSERT_EQ_ULL(8, s->index);
    ASSERT_WRITE_OK(s->methods.write_unsigned_int(s, 1));
    ASSERT_EQ_ULL(12, s->index);
    ASSERT_WRITE_OK(s->methods.write_string(s, "xy"));
    ASSERT_EQ_ULL(18, s->index);
    binary_stream_free(s);
}

static void test_reset_rewinds_index(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_int(s, 42));
    ASSERT_EQ_ULL(4, s->index);
    s->methods.reset(s);
    ASSERT_EQ_ULL(0, s->index);
    binary_stream_free(s);
}

static void test_get_data_points_at_cursor(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_TRUE(s->methods.get_data(s) == s->data);
    ASSERT_WRITE_OK(s->methods.write_short(s, 1));
    ASSERT_TRUE(s->methods.get_data(s) == s->data + 2);
    binary_stream_free(s);
}

static void test_is_empty_reflects_content(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_TRUE(s->methods.is_empty(s));
    ASSERT_WRITE_OK(s->methods.write_short(s, 1));
    ASSERT_FALSE(s->methods.is_empty(s));
    binary_stream_free(s);
}

/* ================================================================== */
/* Croissance de la capacité                                           */
/* ================================================================== */

static void test_growth_beyond_initial_capacity(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(2, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x1122));
    ASSERT_EQ_ULL(2, s->capacity);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x3344));
    ASSERT_TRUE(s->capacity >= 4);
    ASSERT_EQ_ULL(4, s->index);
    binary_stream_free(s);
}

static void test_growth_preserves_previous_bytes(void) {
    uint8_t expected[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    t_binary_stream *s = create_binary_stream_with_capacity(2, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x1122));
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x3344));
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x5566));
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x7788));
    ASSERT_EQ_ULL(8, s->index);
    ASSERT_BYTES(expected, s->data, 8);
    binary_stream_free(s);
}

static void test_growth_from_zero_capacity(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(0, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_NOT_NULL(s);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x1234));
    ASSERT_EQ_ULL(2, s->index);
    ASSERT_EQ_LL(0x12, s->data[0]);
    ASSERT_EQ_LL(0x34, s->data[1]);
    binary_stream_free(s);
}

/* ================================================================== */
/* copy                                                                */
/* ================================================================== */

static void test_copy_duplicates_bytes(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    t_binary_stream *c = NULL;
    ASSERT_WRITE_OK(s->methods.write_int(s, 0x01020304));
    c = s->methods.copy(s);
    ASSERT_NOT_NULL(c);
    ASSERT_TRUE(c->data != s->data);
    ASSERT_BYTES(s->data, c->data, 4);
    ASSERT_EQ_LL(s->endian, c->endian);
    ASSERT_EQ_ULL(0, c->index);
    binary_stream_free(c);
    binary_stream_free(s);
}

static void test_copy_is_independent(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    t_binary_stream *c = NULL;
    ASSERT_WRITE_OK(s->methods.write_int(s, 0x01020304));
    c = s->methods.copy(s);
    ASSERT_NOT_NULL(c);
    s->data[0] = 0xFF;
    ASSERT_EQ_LL(0x01, c->data[0]);
    binary_stream_free(c);
    binary_stream_free(s);
}

static void test_copy_is_writable(void) {
    t_binary_stream *s = create_binary_stream_with_capacity(8, BINARY_STREAM_ENDIAN_BIG);
    t_binary_stream *c = NULL;
    ASSERT_WRITE_OK(s->methods.write_int(s, 0x01020304));
    c = s->methods.copy(s);
    ASSERT_NOT_NULL(c);
    ASSERT_WRITE_OK(c->methods.write_short(c, 0x1234));
    binary_stream_free(c);
    binary_stream_free(s);
}

/* ================================================================== */
/* Bornes et erreurs                                                   */
/* ================================================================== */

static void test_read_past_end_fails(void) {
    uint8_t wire[2] = { 0x00, 0x07 };
    int16_t a = 0;
    t_binary_stream *s = create_binary_stream(wire, sizeof(wire), BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.read_short(s, &a));
    ASSERT_WRITE_FAILS(s->methods.read_short(s, &a));
    binary_stream_free(s);
}

static void test_read_int_needs_four_bytes(void) {
    uint8_t wire[2] = { 0x00, 0x07 };
    int a = 0;
    t_binary_stream *s = create_binary_stream(wire, sizeof(wire), BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_FAILS(s->methods.read_int(s, &a));
    binary_stream_free(s);
}

/* Un stream dimensionné exactement doit accepter l'écriture qui le remplit */
static void test_write_fitting_exactly_in_max_capacity(void) {
    t_binary_stream *s = create_binary_stream_with_capacity_and_max_capacity(
        2, 2, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x1234));
    ASSERT_EQ_ULL(2, s->index);
    binary_stream_free(s);
}

static void test_write_exceeding_max_capacity_fails(void) {
    t_binary_stream *s = create_binary_stream_with_capacity_and_max_capacity(
        2, 2, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_WRITE_OK(s->methods.write_short(s, 0x1234));
    ASSERT_WRITE_FAILS(s->methods.write_short(s, 0x5678));
    binary_stream_free(s);
}

/* Une longueur de chaine corrompue ne doit pas faire lire hors du buffer */
static void test_read_string_rejects_bogus_length(void) {
    uint8_t wire[8] = { 0x00, 0x00, 0x03, 0xE8, 'a', 'b', 'c', 'd' };
    const char *out = NULL;
    t_binary_stream *s = create_binary_stream(wire, sizeof(wire), BINARY_STREAM_ENDIAN_BIG);
    /* annonce 1000 octets alors qu'il n'en reste que 4 */
    ASSERT_WRITE_FAILS(s->methods.read_string(s, &out));
    binary_stream_free(s);
}

static void test_has_capacity_predicate(void) {
    t_binary_stream *s = create_binary_stream_with_capacity_and_max_capacity(
        4, 8, BINARY_STREAM_ENDIAN_BIG);
    ASSERT_TRUE(s->methods.has_capacity(s, 8));
    ASSERT_FALSE(s->methods.has_capacity(s, 9));
    binary_stream_free(s);
}

/* ================================================================== */
/* Runner                                                              */
/* ================================================================== */

struct s_test {
    const char *name;
    void (*fn)(void);
};

static const struct s_test tests[] = {
    { "create_with_capacity",                test_create_with_capacity },
    { "create_with_max_capacity",            test_create_with_max_capacity },
    { "create_from_data_copies",             test_create_from_data_copies },

    { "write_short_big_endian_layout",       test_write_short_big_endian_layout },
    { "write_short_little_endian_layout",    test_write_short_little_endian_layout },
    { "write_short_le_forces_little_endian", test_write_short_le_forces_little_endian },
    { "write_int_big_endian_layout",         test_write_int_big_endian_layout },
    { "write_int_le_layout",                 test_write_int_le_layout },
    { "write_long_big_endian_layout",        test_write_long_big_endian_layout },
    { "write_long_le_layout",                test_write_long_le_layout },
    { "write_string_layout",                 test_write_string_layout },

    { "roundtrip_short_big_endian",          test_roundtrip_short_big_endian },
    { "roundtrip_short_little_endian",       test_roundtrip_short_little_endian },
    { "roundtrip_unsigned_short_max",        test_roundtrip_unsigned_short_max },
    { "roundtrip_int_extremes",              test_roundtrip_int_extremes },
    { "roundtrip_unsigned_int_max",          test_roundtrip_unsigned_int_max },
    { "roundtrip_unsigned_int_le",           test_roundtrip_unsigned_int_le },
    { "roundtrip_long_extremes",             test_roundtrip_long_extremes },
    { "roundtrip_unsigned_long_max",         test_roundtrip_unsigned_long_max },
    { "roundtrip_unsigned_long_le",          test_roundtrip_unsigned_long_le },
    { "long_endianness_is_mirrored",         test_long_endianness_is_mirrored },
    { "roundtrip_string",                    test_roundtrip_string },
    { "roundtrip_empty_string",              test_roundtrip_empty_string },
    { "roundtrip_mixed_packet",              test_roundtrip_mixed_packet },
    { "read_from_fixed_buffer",              test_read_from_fixed_buffer },

    { "index_advances_per_type",             test_index_advances_per_type },
    { "reset_rewinds_index",                 test_reset_rewinds_index },
    { "get_data_points_at_cursor",           test_get_data_points_at_cursor },
    { "is_empty_reflects_content",           test_is_empty_reflects_content },

    { "growth_beyond_initial_capacity",      test_growth_beyond_initial_capacity },
    { "growth_preserves_previous_bytes",     test_growth_preserves_previous_bytes },
    { "growth_from_zero_capacity",           test_growth_from_zero_capacity },

    { "copy_duplicates_bytes",               test_copy_duplicates_bytes },
    { "copy_is_independent",                 test_copy_is_independent },
    { "copy_is_writable",                    test_copy_is_writable },

    { "read_past_end_fails",                 test_read_past_end_fails },
    { "read_int_needs_four_bytes",           test_read_int_needs_four_bytes },
    { "write_fitting_exactly_in_max_capacity", test_write_fitting_exactly_in_max_capacity },
    { "write_exceeding_max_capacity_fails",  test_write_exceeding_max_capacity_fails },
    { "read_string_rejects_bogus_length",    test_read_string_rejects_bogus_length },
    { "has_capacity_predicate",              test_has_capacity_predicate }
};

#define TEST_COUNT (sizeof(tests) / sizeof(tests[0]))

/* Lance un test dans un fils : renvoie 1 si succes. */
static int run_forked(const struct s_test *test) {
    pid_t pid;
    int status = 0;

    printf("  %-42s", test->name);
    fflush(stdout);

    pid = fork();
    if (pid < 0) {
        printf("FORK KO\n");
        return 0;
    }
    if (pid == 0) {
        test->fn();
        exit(0);
    }
    if (waitpid(pid, &status, 0) < 0) {
        printf("WAIT KO\n");
        return 0;
    }
    if (WIFSIGNALED(status)) {
        printf("CRASH (signal %d)\n", WTERMSIG(status));
        return 0;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("ok\n");
        return 1;
    }
    printf("KO\n");
    return 0;
}

int main(void) {
    size_t i;
    size_t passed = 0;

    printf("binary_stream : %zu tests\n\n", (size_t)TEST_COUNT);
    for (i = 0; i < TEST_COUNT; i++) {
        passed += (size_t)run_forked(&tests[i]);
    }
    printf("\n%zu/%zu\n", passed, (size_t)TEST_COUNT);
    return passed == TEST_COUNT ? 0 : 1;
}
