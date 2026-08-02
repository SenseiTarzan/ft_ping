/*
 * Tests unitaires pour packet_processors.
 *
 * Trois volets :
 *   - le registre lui-même (register / unregister / get, bornes des id)
 *   - le pipeline de sérialisation : packet_processor_serialize()
 *   - le pipeline de désérialisation et le handler, cycle de vie du paquet
 *     inclus (constructor / destructor)
 *
 * Chaque test tourne dans un processus fils : un débordement du tableau
 * statique ou un déréférencement de NULL est rapporté comme un échec au lieu
 * de tuer la suite. Le registre étant global, le fork donne aussi à chaque
 * test un registre vierge.
 *
 * Depuis la racine du projet :
 *   cmake --build cmake-build-debug --target test_packet_processors
 *   ./cmake-build-debug/test_packet_processors
 *
 * Ou directement :
 *   cc -g3 -O0 -std=c99 -Wall -Wextra -o test_packet_processors \
 *      tests/test_packet_processors.c src/packet_processors/packet_processors.c \
 *      src/binary_stream/binary_stream.c
 *
 * Avec sanitizers (détecte le débordement du tableau global) :
 *   cc -g3 -O0 -std=c99 -fsanitize=address,undefined \
 *      -o test_packet_processors tests/test_packet_processors.c \
 *      src/packet_processors/packet_processors.c src/binary_stream/binary_stream.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../src/binary_stream/binary_stream.h"
#include "../src/packet_processors/packet_processors.h"

/* Taille du tableau statique dans packet_processors.c : les id valides vont
 * donc de 0 à 254 inclus. */
#define PROCESSOR_SLOTS 255

/* Id utilisés par les tests. PROC_ID est aussi l'octet d'en-tête écrit dans
 * le flux, puisque packet_processor_deserializer() relit l'id depuis
 * data[0]. */
#define PROC_ID   7
#define OTHER_ID  8

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

#define ASSERT_NULL(ptr)                                                    \
    do {                                                                    \
        if ((ptr) != NULL) {                                                \
            FAIL("attendu NULL : %s", #ptr);                                \
        }                                                                   \
    } while (0)

#define ASSERT_NOT_NULL(ptr)                                                \
    do {                                                                    \
        if ((ptr) == NULL) {                                                \
            FAIL("attendu non NULL : %s", #ptr);                            \
        }                                                                   \
    } while (0)

#define ASSERT_EQ_PTR(expected, actual)                                     \
    do {                                                                    \
        const void *e_ = (const void *)(expected);                          \
        const void *a_ = (const void *)(actual);                            \
        if (e_ != a_) {                                                     \
            FAIL("%s : attendu %p, obtenu %p", #actual, e_, a_);            \
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

#define ASSERT_EQ_STR(expected, actual)                                     \
    do {                                                                    \
        const char *e_ = (expected);                                        \
        const char *a_ = (actual);                                          \
        if (a_ == NULL || strcmp(e_, a_) != 0) {                            \
            FAIL("%s : attendu \"%s\", obtenu \"%s\"", #actual, e_,         \
                 a_ ? a_ : "(null)");                                       \
        }                                                                   \
    } while (0)

/* ------------------------------------------------------------------ */
/* Espions                                                             */
/* ------------------------------------------------------------------ */

enum e_event {
    EV_NONE = 0,
    EV_PRE_SER,
    EV_SER,
    EV_POST_SER,
    EV_CONSTRUCTOR,
    EV_PRE_DESER,
    EV_DESER,
    EV_DESTRUCTOR,
    EV_HANDLER
};

static enum e_event g_events[16];
static size_t       g_event_count;

/* Étape qui doit échouer, EV_NONE pour un chemin nominal. */
static enum e_event g_fail_stage;
static int          g_constructor_fails;
static void        *g_handler_arg;
static int          g_handler_result;
static void        *g_destroyed;

static void record(enum e_event ev) {
    if (g_event_count < sizeof(g_events) / sizeof(g_events[0])) {
        g_events[g_event_count] = ev;
    }
    g_event_count++;
}

static void spies_reset(void) {
    memset(g_events, 0, sizeof(g_events));
    g_event_count = 0;
    g_fail_stage = EV_NONE;
    g_constructor_fails = 0;
    g_handler_arg = NULL;
    g_handler_result = 1;
    g_destroyed = NULL;
}

struct s_payload {
    int value;
};

static t_packet_processor_status spy_pre_serializer(t_binary_stream *stream, const void *packet) {
    (void)packet;
    record(EV_PRE_SER);
    if (g_fail_stage == EV_PRE_SER) {
        return PACKET_PROCESSOR_STATUS_FAILURE_PRE_SERIALIZE;
    }
    /* L'octet d'en-tête porte l'id : c'est lui que la désérialisation relit
     * pour choisir le processeur. */
    if (binary_stream_status_is_failed(stream->methods.write_unsigned_char(stream, PROC_ID))) {
        return PACKET_PROCESSOR_STATUS_FAILURE_PRE_SERIALIZE;
    }
    return PACKET_PROCESSOR_STATUS_OK;
}

static t_packet_processor_status spy_serializer(t_binary_stream *stream, const void *packet) {
    const struct s_payload *payload = packet;

    record(EV_SER);
    if (g_fail_stage == EV_SER) {
        return PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE;
    }
    if (binary_stream_status_is_failed(stream->methods.write_int(stream, payload->value))) {
        return PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE;
    }
    return PACKET_PROCESSOR_STATUS_OK;
}

static t_packet_processor_status spy_post_serializer(t_binary_stream *stream, const void *packet) {
    (void)stream;
    (void)packet;
    record(EV_POST_SER);
    if (g_fail_stage == EV_POST_SER) {
        return PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE;
    }
    return PACKET_PROCESSOR_STATUS_OK;
}

static void *spy_constructor(void) {
    struct s_payload *payload;

    record(EV_CONSTRUCTOR);
    if (g_constructor_fails) {
        return NULL;
    }
    payload = malloc(sizeof(*payload));
    if (payload == NULL) {
        return NULL;
    }
    payload->value = 0;
    return payload;
}

static t_packet_processor_status spy_pre_deserializer(t_binary_stream *stream, void *packet) {
    uint8_t id = 0;

    (void)packet;
    record(EV_PRE_DESER);
    if (g_fail_stage == EV_PRE_DESER) {
        return PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE;
    }
    /* Consomme l'octet d'id laissé par pre_serializer : le module le lit dans
     * data[0] sans avancer le curseur. */
    if (binary_stream_status_is_failed(stream->methods.read_unsigned_char(stream, &id))) {
        return PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE;
    }
    return PACKET_PROCESSOR_STATUS_OK;
}

static t_packet_processor_status spy_deserializer(t_binary_stream *stream, void *packet) {
    struct s_payload *payload = packet;

    record(EV_DESER);
    if (g_fail_stage == EV_DESER) {
        return PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE;
    }
    if (binary_stream_status_is_failed(stream->methods.read_int(stream, &payload->value))) {
        return PACKET_PROCESSOR_STATUS_FAILURE_DESERIALIZE;
    }
    return PACKET_PROCESSOR_STATUS_OK;
}

static void spy_destructor(void *packet) {
    record(EV_DESTRUCTOR);
    g_destroyed = packet;
    free(packet);
}

static bool spy_handler(void *packet) {
    record(EV_HANDLER);
    g_handler_arg = packet;
    return g_handler_result ? true : false;
}

/* ------------------------------------------------------------------ */
/* Aides                                                               */
/* ------------------------------------------------------------------ */

static void register_full_spy(int id) {
    register_packet_processor(id,
                              spy_pre_serializer, spy_serializer, spy_post_serializer,
                              spy_constructor,
                              spy_pre_deserializer, spy_deserializer,
                              spy_destructor,
                              spy_handler);
}

static t_binary_stream *make_stream(void) {
    t_binary_stream *stream = create_binary_stream_with_capacity(16, BINARY_STREAM_ENDIAN_BIG);

    if (stream == NULL) {
        FAIL("create_binary_stream_with_capacity a echoue");
    }
    return stream;
}

/* Flux prêt à être désérialisé : octet d'id puis un entier. */
static t_binary_stream *make_encoded_stream(uint8_t id, int value) {
    t_binary_stream *stream = make_stream();

    if (binary_stream_status_is_failed(stream->methods.write_unsigned_char(stream, id))) {
        FAIL("write_unsigned_char a echoue");
    }
    if (binary_stream_status_is_failed(stream->methods.write_int(stream, value))) {
        FAIL("write_int a echoue");
    }
    binary_stream_reset(stream);
    return stream;
}

static void assert_events(const enum e_event *expected, size_t count) {
    size_t i;

    ASSERT_EQ_LL(count, g_event_count);
    for (i = 0; i < count; i++) {
        if (g_events[i] != expected[i]) {
            FAIL("evenement %zu : attendu %d, obtenu %d", i,
                 (int)expected[i], (int)g_events[i]);
        }
    }
}

/* ================================================================== */
/* Status                                                              */
/* ================================================================== */

static void test_status_helpers_are_opposite(void) {
    ASSERT_TRUE(packet_processor_status_is_success(PACKET_PROCESSOR_STATUS_OK));
    ASSERT_FALSE(packet_processor_status_is_failed(PACKET_PROCESSOR_STATUS_OK));
    ASSERT_TRUE(packet_processor_status_is_failed(PACKET_PROCESSOR_STATUS_FAILED));
    ASSERT_TRUE(packet_processor_status_is_failed(PACKET_PROCESSOR_STATUS_FAILURE_MALLOC));
    ASSERT_FALSE(packet_processor_status_is_success(PACKET_PROCESSOR_STATUS_FAILURE_CHECKSUM));
}

static void test_status_message_is_never_null(void) {
    int i;

    ASSERT_EQ_STR("ok", packet_processor_status_message(PACKET_PROCESSOR_STATUS_OK));
    for (i = -1; i <= PACKET_PROCESSOR_STATUS_FAILURE_NOT_IMPLEMENTED; i++) {
        ASSERT_NOT_NULL(packet_processor_status_message((t_packet_processor_status)i));
    }
}

/* ================================================================== */
/* Registre : cas nominaux                                             */
/* ================================================================== */

static void test_unregistered_slot_is_empty(void) {
    t_packet_processor *proc = get_packet_processor(42);

    ASSERT_NOT_NULL(proc);
    ASSERT_NULL(proc->pre_serializer);
    ASSERT_NULL(proc->serializer);
    ASSERT_NULL(proc->post_serializer);
    ASSERT_NULL(proc->constructor);
    ASSERT_NULL(proc->pre_deserializer);
    ASSERT_NULL(proc->deserializer);
    ASSERT_NULL(proc->destructor);
    ASSERT_NULL(proc->handler);
}

static void test_register_then_get_returns_all_callbacks(void) {
    t_packet_processor *proc;

    register_full_spy(PROC_ID);
    proc = get_packet_processor(PROC_ID);
    ASSERT_NOT_NULL(proc);
    ASSERT_EQ_PTR(spy_pre_serializer, proc->pre_serializer);
    ASSERT_EQ_PTR(spy_serializer, proc->serializer);
    ASSERT_EQ_PTR(spy_post_serializer, proc->post_serializer);
    ASSERT_EQ_PTR(spy_constructor, proc->constructor);
    ASSERT_EQ_PTR(spy_pre_deserializer, proc->pre_deserializer);
    ASSERT_EQ_PTR(spy_deserializer, proc->deserializer);
    ASSERT_EQ_PTR(spy_destructor, proc->destructor);
    ASSERT_EQ_PTR(spy_handler, proc->handler);
}

/* Les callbacks optionnels peuvent rester à NULL */
static void test_register_accepts_null_callbacks(void) {
    t_packet_processor *proc;

    register_packet_processor(3, NULL, spy_serializer, NULL, spy_constructor,
                              NULL, spy_deserializer, NULL, spy_handler);
    proc = get_packet_processor(3);
    ASSERT_NOT_NULL(proc);
    ASSERT_NULL(proc->pre_serializer);
    ASSERT_NULL(proc->post_serializer);
    ASSERT_NULL(proc->pre_deserializer);
    ASSERT_NULL(proc->destructor);
    ASSERT_EQ_PTR(spy_serializer, proc->serializer);
}

static void test_register_at_id_zero(void) {
    register_full_spy(0);
    ASSERT_EQ_PTR(spy_serializer, get_packet_processor(0)->serializer);
}

static void test_register_at_last_valid_id(void) {
    register_full_spy(PROCESSOR_SLOTS - 1);
    ASSERT_EQ_PTR(spy_serializer, get_packet_processor(PROCESSOR_SLOTS - 1)->serializer);
}

static void test_register_overwrites_previous(void) {
    t_packet_processor *proc;

    register_full_spy(5);
    register_packet_processor(5, NULL, spy_serializer, NULL, NULL, NULL, NULL, NULL, NULL);
    proc = get_packet_processor(5);
    ASSERT_NULL(proc->pre_serializer);
    ASSERT_NULL(proc->post_serializer);
    ASSERT_NULL(proc->constructor);
    ASSERT_NULL(proc->pre_deserializer);
    ASSERT_NULL(proc->deserializer);
    ASSERT_NULL(proc->destructor);
    ASSERT_NULL(proc->handler);
    ASSERT_EQ_PTR(spy_serializer, proc->serializer);
}

static void test_registrations_are_independent(void) {
    register_packet_processor(10, NULL, spy_serializer, NULL, NULL, NULL, NULL, NULL, spy_handler);
    register_packet_processor(11, NULL, NULL, NULL, spy_constructor, NULL, spy_deserializer, NULL, NULL);
    ASSERT_EQ_PTR(spy_serializer, get_packet_processor(10)->serializer);
    ASSERT_NULL(get_packet_processor(10)->deserializer);
    ASSERT_NULL(get_packet_processor(11)->serializer);
    ASSERT_EQ_PTR(spy_deserializer, get_packet_processor(11)->deserializer);
}

/* ================================================================== */
/* Registre : unregister                                               */
/* ================================================================== */

static void test_unregister_clears_all_callbacks(void) {
    t_packet_processor *proc;

    register_full_spy(9);
    unregister_packet_processor(9);
    proc = get_packet_processor(9);
    ASSERT_NOT_NULL(proc);
    ASSERT_NULL(proc->pre_serializer);
    ASSERT_NULL(proc->serializer);
    ASSERT_NULL(proc->post_serializer);
    ASSERT_NULL(proc->constructor);
    ASSERT_NULL(proc->pre_deserializer);
    ASSERT_NULL(proc->deserializer);
    ASSERT_NULL(proc->destructor);
    ASSERT_NULL(proc->handler);
}

static void test_unregister_leaves_other_ids_intact(void) {
    register_full_spy(20);
    register_full_spy(21);
    unregister_packet_processor(20);
    ASSERT_NULL(get_packet_processor(20)->serializer);
    ASSERT_EQ_PTR(spy_serializer, get_packet_processor(21)->serializer);
}

static void test_unregister_unused_slot_is_harmless(void) {
    unregister_packet_processor(77);
    ASSERT_NULL(get_packet_processor(77)->serializer);
}

/* ================================================================== */
/* Registre : bornes des identifiants                                  */
/* ================================================================== */

static void test_get_rejects_id_equal_to_slot_count(void) {
    /* 255 slots => dernier index valide 254 */
    ASSERT_NULL(get_packet_processor(PROCESSOR_SLOTS));
}

static void test_get_rejects_id_beyond_slot_count(void) {
    ASSERT_NULL(get_packet_processor(PROCESSOR_SLOTS + 1));
    ASSERT_NULL(get_packet_processor(100000));
}

static void test_get_rejects_negative_id(void) {
    ASSERT_NULL(get_packet_processor(-1));
    ASSERT_NULL(get_packet_processor(-100000));
}

static void test_register_rejects_id_equal_to_slot_count(void) {
    /* si l'id est accepté, l'écriture se fait hors du tableau statique */
    register_full_spy(PROCESSOR_SLOTS);
    ASSERT_NULL(get_packet_processor(PROCESSOR_SLOTS));
}

static void test_register_rejects_id_beyond_slot_count(void) {
    register_full_spy(PROCESSOR_SLOTS + 1);
    ASSERT_NULL(get_packet_processor(PROCESSOR_SLOTS + 1));
}

static void test_register_rejects_negative_id(void) {
    register_full_spy(-1);
    ASSERT_NULL(get_packet_processor(-1));
}

static void test_unregister_rejects_out_of_range_id(void) {
    unregister_packet_processor(PROCESSOR_SLOTS);
    unregister_packet_processor(-1);
    /* aucun slot valide ne doit avoir été touché */
    register_full_spy(1);
    unregister_packet_processor(PROCESSOR_SLOTS);
    ASSERT_EQ_PTR(spy_serializer, get_packet_processor(1)->serializer);
}

/* ================================================================== */
/* packet_processor_serialize                                          */
/* ================================================================== */

static void test_serialize_runs_pre_then_serializer_then_post(void) {
    static const enum e_event expected[] = { EV_PRE_SER, EV_SER, EV_POST_SER };
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;
    register_full_spy(PROC_ID);

    ASSERT_TRUE(packet_processor_status_is_success(
            packet_processor_serialize(PROC_ID, stream, &payload)));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    binary_stream_free(stream);
}

static void test_serialize_skips_null_optional_callbacks(void) {
    static const enum e_event expected[] = { EV_SER };
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;
    register_packet_processor(PROC_ID, NULL, spy_serializer, NULL, NULL, NULL, NULL, NULL, NULL);

    ASSERT_TRUE(packet_processor_status_is_success(
            packet_processor_serialize(PROC_ID, stream, &payload)));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    binary_stream_free(stream);
}

static void test_serialize_returns_ok_on_success(void) {
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;
    register_full_spy(PROC_ID);

    ASSERT_EQ_LL(PACKET_PROCESSOR_STATUS_OK,
                 packet_processor_serialize(PROC_ID, stream, &payload));
    binary_stream_free(stream);
}

static void test_serialize_writes_expected_bytes(void) {
    /* gros boutiste : octet d'id puis 12345 sur 4 octets */
    static const unsigned char expected[5] = { PROC_ID, 0x00, 0x00, 0x30, 0x39 };
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 12345;
    register_full_spy(PROC_ID);

    ASSERT_TRUE(packet_processor_status_is_success(
            packet_processor_serialize(PROC_ID, stream, &payload)));
    ASSERT_EQ_LL(sizeof(expected), stream->index);
    ASSERT_TRUE(memcmp(expected, stream->data, sizeof(expected)) == 0);
    binary_stream_free(stream);
}

static void test_serialize_stops_when_pre_fails(void) {
    static const enum e_event expected[] = { EV_PRE_SER };
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;
    g_fail_stage = EV_PRE_SER;
    register_full_spy(PROC_ID);

    ASSERT_EQ_LL(PACKET_PROCESSOR_STATUS_FAILURE_PRE_SERIALIZE,
                 packet_processor_serialize(PROC_ID, stream, &payload));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    binary_stream_free(stream);
}

static void test_serialize_stops_when_serializer_fails(void) {
    static const enum e_event expected[] = { EV_PRE_SER, EV_SER };
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;
    g_fail_stage = EV_SER;
    register_full_spy(PROC_ID);

    ASSERT_EQ_LL(PACKET_PROCESSOR_STATUS_FAILURE_SERIALIZE,
                 packet_processor_serialize(PROC_ID, stream, &payload));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    binary_stream_free(stream);
}

static void test_serialize_propagates_post_failure(void) {
    static const enum e_event expected[] = { EV_PRE_SER, EV_SER, EV_POST_SER };
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;
    g_fail_stage = EV_POST_SER;
    register_full_spy(PROC_ID);

    ASSERT_EQ_LL(PACKET_PROCESSOR_STATUS_FAILURE_POST_SERIALIZE,
                 packet_processor_serialize(PROC_ID, stream, &payload));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    binary_stream_free(stream);
}

static void test_serialize_without_serializer_is_not_implemented(void) {
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;
    register_packet_processor(PROC_ID, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    ASSERT_EQ_LL(PACKET_PROCESSOR_STATUS_FAILURE_NOT_IMPLEMENTED,
                 packet_processor_serialize(PROC_ID, stream, &payload));
    binary_stream_free(stream);
}

static void test_serialize_rejects_null_stream(void) {
    struct s_payload payload;

    spies_reset();
    payload.value = 1;
    register_full_spy(PROC_ID);

    ASSERT_TRUE(packet_processor_status_is_failed(
            packet_processor_serialize(PROC_ID, NULL, &payload)));
    ASSERT_EQ_LL(0, g_event_count);
}

static void test_serialize_rejects_out_of_range_id(void) {
    struct s_payload payload;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 1;

    ASSERT_TRUE(packet_processor_status_is_failed(
            packet_processor_serialize(PROCESSOR_SLOTS, stream, &payload)));
    ASSERT_TRUE(packet_processor_status_is_failed(
            packet_processor_serialize(-1, stream, &payload)));
    ASSERT_EQ_LL(0, g_event_count);
    binary_stream_free(stream);
}

/* ================================================================== */
/* packet_processor_deserializer                                       */
/* ================================================================== */

static void test_deserialize_runs_constructor_pre_then_deserializer(void) {
    static const enum e_event expected[] = { EV_CONSTRUCTOR, EV_PRE_DESER, EV_DESER };
    t_binary_stream *stream = make_encoded_stream(PROC_ID, 1);
    struct s_payload *decoded;

    spies_reset();
    register_full_spy(PROC_ID);

    decoded = packet_processor_deserializer(stream);
    ASSERT_NOT_NULL(decoded);
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    free(decoded);
    binary_stream_free(stream);
}

static void test_deserialize_returns_the_decoded_packet(void) {
    t_binary_stream *stream = make_encoded_stream(PROC_ID, 0x0BADF00D);
    struct s_payload *decoded;

    spies_reset();
    register_full_spy(PROC_ID);

    decoded = packet_processor_deserializer(stream);
    ASSERT_NOT_NULL(decoded);
    ASSERT_EQ_LL(0x0BADF00D, decoded->value);
    free(decoded);
    binary_stream_free(stream);
}

static void test_deserialize_reads_id_from_first_byte(void) {
    t_binary_stream *stream = make_encoded_stream(OTHER_ID, 1);
    struct s_payload *decoded;

    spies_reset();
    /* seul OTHER_ID est enregistré : si le module lisait un autre octet, il
     * ne trouverait pas de constructeur */
    register_full_spy(OTHER_ID);

    decoded = packet_processor_deserializer(stream);
    ASSERT_NOT_NULL(decoded);
    free(decoded);
    binary_stream_free(stream);
}

static void test_deserialize_rejects_null_stream(void) {
    spies_reset();
    register_full_spy(PROC_ID);

    ASSERT_NULL(packet_processor_deserializer(NULL));
    ASSERT_EQ_LL(0, g_event_count);
}

static void test_deserialize_without_constructor_returns_null(void) {
    t_binary_stream *stream = make_encoded_stream(PROC_ID, 1);

    spies_reset();
    register_packet_processor(PROC_ID, NULL, spy_serializer, NULL, NULL,
                              NULL, spy_deserializer, NULL, NULL);

    ASSERT_NULL(packet_processor_deserializer(stream));
    ASSERT_EQ_LL(0, g_event_count);
    binary_stream_free(stream);
}

static void test_deserialize_returns_null_when_constructor_fails(void) {
    static const enum e_event expected[] = { EV_CONSTRUCTOR };
    t_binary_stream *stream = make_encoded_stream(PROC_ID, 1);

    spies_reset();
    g_constructor_fails = 1;
    register_full_spy(PROC_ID);

    ASSERT_NULL(packet_processor_deserializer(stream));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    binary_stream_free(stream);
}

static void test_deserialize_destroys_packet_when_pre_fails(void) {
    static const enum e_event expected[] = { EV_CONSTRUCTOR, EV_PRE_DESER, EV_DESTRUCTOR };
    t_binary_stream *stream = make_encoded_stream(PROC_ID, 1);

    spies_reset();
    g_fail_stage = EV_PRE_DESER;
    register_full_spy(PROC_ID);

    ASSERT_NULL(packet_processor_deserializer(stream));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    ASSERT_NOT_NULL(g_destroyed);
    binary_stream_free(stream);
}

static void test_deserialize_destroys_packet_when_deserializer_fails(void) {
    static const enum e_event expected[] = {
        EV_CONSTRUCTOR, EV_PRE_DESER, EV_DESER, EV_DESTRUCTOR
    };
    t_binary_stream *stream = make_encoded_stream(PROC_ID, 1);

    spies_reset();
    g_fail_stage = EV_DESER;
    register_full_spy(PROC_ID);

    ASSERT_NULL(packet_processor_deserializer(stream));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    ASSERT_NOT_NULL(g_destroyed);
    binary_stream_free(stream);
}

/* Un processeur sans deserializer ne doit pas faire tomber le module */
static void test_deserialize_without_deserializer_is_refused(void) {
    t_binary_stream *stream = make_encoded_stream(PROC_ID, 1);

    spies_reset();
    register_packet_processor(PROC_ID, NULL, NULL, NULL, spy_constructor,
                              NULL, NULL, spy_destructor, NULL);

    ASSERT_NULL(packet_processor_deserializer(stream));
    binary_stream_free(stream);
}

/* Le module lit data[0] : sur un flux vide c'est une lecture hors buffer */
static void test_deserialize_rejects_empty_stream(void) {
    t_binary_stream *stream = create_binary_stream_with_capacity(0, BINARY_STREAM_ENDIAN_BIG);

    spies_reset();
    register_full_spy(PROC_ID);

    ASSERT_NULL(packet_processor_deserializer(stream));
    binary_stream_free(stream);
}

static void test_roundtrip_serialize_then_deserialize(void) {
    struct s_payload payload;
    struct s_payload *decoded;
    t_binary_stream *stream = make_stream();

    spies_reset();
    payload.value = 0x0BADF00D;
    register_full_spy(PROC_ID);

    ASSERT_TRUE(packet_processor_status_is_success(
            packet_processor_serialize(PROC_ID, stream, &payload)));
    binary_stream_reset(stream);

    decoded = packet_processor_deserializer(stream);
    ASSERT_NOT_NULL(decoded);
    ASSERT_EQ_LL(payload.value, decoded->value);
    free(decoded);
    binary_stream_free(stream);
}

/* ================================================================== */
/* packet_processor_handler                                            */
/* ================================================================== */

static void test_handler_receives_the_packet(void) {
    struct s_payload *packet = malloc(sizeof(*packet));

    spies_reset();
    ASSERT_NOT_NULL(packet);
    register_full_spy(PROC_ID);

    /* la propriété du paquet passe au module : il le détruit après usage */
    ASSERT_TRUE(packet_processor_handler(PROC_ID, packet));
    ASSERT_EQ_PTR(packet, g_handler_arg);
}

static void test_handler_can_refuse_packet(void) {
    struct s_payload *packet = malloc(sizeof(*packet));

    spies_reset();
    ASSERT_NOT_NULL(packet);
    g_handler_result = 0;
    register_full_spy(PROC_ID);

    ASSERT_FALSE(packet_processor_handler(PROC_ID, packet));
    ASSERT_EQ_PTR(packet, g_handler_arg);
}

static void test_handler_destroys_packet_after_use(void) {
    static const enum e_event expected[] = { EV_HANDLER, EV_DESTRUCTOR };
    struct s_payload *packet = malloc(sizeof(*packet));

    spies_reset();
    ASSERT_NOT_NULL(packet);
    register_full_spy(PROC_ID);

    ASSERT_TRUE(packet_processor_handler(PROC_ID, packet));
    assert_events(expected, sizeof(expected) / sizeof(expected[0]));
    ASSERT_EQ_PTR(packet, g_destroyed);
}

static void test_handler_rejects_out_of_range_id(void) {
    spies_reset();

    ASSERT_FALSE(packet_processor_handler(PROCESSOR_SLOTS, NULL));
    ASSERT_FALSE(packet_processor_handler(-1, NULL));
    ASSERT_EQ_LL(0, g_event_count);
}

/* Un processeur désenregistré n'a plus de handler : l'appel doit être refusé,
 * pas déréférencer NULL */
static void test_handler_without_callback_is_refused(void) {
    struct s_payload *packet = malloc(sizeof(*packet));

    spies_reset();
    ASSERT_NOT_NULL(packet);
    register_full_spy(PROC_ID);
    unregister_packet_processor(PROC_ID);

    ASSERT_FALSE(packet_processor_handler(PROC_ID, packet));
    free(packet);
}

static void test_unregistered_processor_has_no_callable(void) {
    t_packet_processor *proc;

    register_full_spy(1);
    unregister_packet_processor(1);
    proc = get_packet_processor(1);
    ASSERT_NULL(proc->serializer);
    ASSERT_NULL(proc->deserializer);
    ASSERT_NULL(proc->handler);
}

/* ================================================================== */
/* Runner                                                              */
/* ================================================================== */

struct s_test {
    const char *name;
    void      (*fn)(void);
};

static const struct s_test tests[] = {
    { "status_helpers_are_opposite",            test_status_helpers_are_opposite },
    { "status_message_is_never_null",           test_status_message_is_never_null },

    { "unregistered_slot_is_empty",             test_unregistered_slot_is_empty },
    { "register_then_get_returns_all_callbacks", test_register_then_get_returns_all_callbacks },
    { "register_accepts_null_callbacks",        test_register_accepts_null_callbacks },
    { "register_at_id_zero",                    test_register_at_id_zero },
    { "register_at_last_valid_id",              test_register_at_last_valid_id },
    { "register_overwrites_previous",           test_register_overwrites_previous },
    { "registrations_are_independent",          test_registrations_are_independent },

    { "unregister_clears_all_callbacks",        test_unregister_clears_all_callbacks },
    { "unregister_leaves_other_ids_intact",     test_unregister_leaves_other_ids_intact },
    { "unregister_unused_slot_is_harmless",     test_unregister_unused_slot_is_harmless },

    { "get_rejects_id_equal_to_slot_count",     test_get_rejects_id_equal_to_slot_count },
    { "get_rejects_id_beyond_slot_count",       test_get_rejects_id_beyond_slot_count },
    { "get_rejects_negative_id",                test_get_rejects_negative_id },
    { "register_rejects_id_equal_to_slot_count", test_register_rejects_id_equal_to_slot_count },
    { "register_rejects_id_beyond_slot_count",  test_register_rejects_id_beyond_slot_count },
    { "register_rejects_negative_id",           test_register_rejects_negative_id },
    { "unregister_rejects_out_of_range_id",     test_unregister_rejects_out_of_range_id },

    { "serialize_runs_pre_then_serializer_then_post", test_serialize_runs_pre_then_serializer_then_post },
    { "serialize_skips_null_optional_callbacks", test_serialize_skips_null_optional_callbacks },
    { "serialize_returns_ok_on_success",        test_serialize_returns_ok_on_success },
    { "serialize_writes_expected_bytes",        test_serialize_writes_expected_bytes },
    { "serialize_stops_when_pre_fails",         test_serialize_stops_when_pre_fails },
    { "serialize_stops_when_serializer_fails",  test_serialize_stops_when_serializer_fails },
    { "serialize_propagates_post_failure",      test_serialize_propagates_post_failure },
    { "serialize_without_serializer_is_not_implemented", test_serialize_without_serializer_is_not_implemented },
    { "serialize_rejects_null_stream",          test_serialize_rejects_null_stream },
    { "serialize_rejects_out_of_range_id",      test_serialize_rejects_out_of_range_id },

    { "deserialize_runs_constructor_pre_then_deserializer", test_deserialize_runs_constructor_pre_then_deserializer },
    { "deserialize_returns_the_decoded_packet", test_deserialize_returns_the_decoded_packet },
    { "deserialize_reads_id_from_first_byte",   test_deserialize_reads_id_from_first_byte },
    { "deserialize_rejects_null_stream",        test_deserialize_rejects_null_stream },
    { "deserialize_without_constructor_returns_null", test_deserialize_without_constructor_returns_null },
    { "deserialize_returns_null_when_constructor_fails", test_deserialize_returns_null_when_constructor_fails },
    { "deserialize_destroys_packet_when_pre_fails", test_deserialize_destroys_packet_when_pre_fails },
    { "deserialize_destroys_packet_when_deserializer_fails", test_deserialize_destroys_packet_when_deserializer_fails },
    { "deserialize_without_deserializer_is_refused", test_deserialize_without_deserializer_is_refused },
    { "deserialize_rejects_empty_stream",       test_deserialize_rejects_empty_stream },
    { "roundtrip_serialize_then_deserialize",   test_roundtrip_serialize_then_deserialize },

    { "handler_receives_the_packet",            test_handler_receives_the_packet },
    { "handler_can_refuse_packet",              test_handler_can_refuse_packet },
    { "handler_destroys_packet_after_use",      test_handler_destroys_packet_after_use },
    { "handler_rejects_out_of_range_id",        test_handler_rejects_out_of_range_id },
    { "handler_without_callback_is_refused",    test_handler_without_callback_is_refused },

    { "unregistered_processor_has_no_callable", test_unregistered_processor_has_no_callable }
};

#define TEST_COUNT (sizeof(tests) / sizeof(tests[0]))

static int run_forked(const struct s_test *test) {
    pid_t pid;
    int status = 0;

    printf("  %-55s", test->name);
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

    printf("packet_processors : %zu tests\n\n", (size_t)TEST_COUNT);
    for (i = 0; i < TEST_COUNT; i++) {
        passed += (size_t)run_forked(&tests[i]);
    }
    printf("\n%zu/%zu\n", passed, (size_t)TEST_COUNT);
    return passed == TEST_COUNT ? 0 : 1;
}
