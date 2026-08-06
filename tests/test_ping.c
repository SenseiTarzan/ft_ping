/*
 * Tests unitaires pour l'historique des sequences de ping
 * (sequence_ping_history_new / has / add / free).
 *
 * Chaque test tourne dans un processus fils : un segfault ou un abort est
 * rapporte comme un echec au lieu de tuer toute la suite.
 *
 * Depuis la racine du projet :
 *   cmake --build build --target test_ping && ./build/test_ping
 *
 * Ou directement :
 *   cc -g3 -O0 -std=c99 -Wall -Wextra -o test_ping \
 *      tests/test_ping.c src/ping/ping.c
 *
 * Avec sanitizers (recommande, detecte les fuites et les UB) :
 *   cc -g3 -O0 -std=c99 -fsanitize=address,undefined \
 *      -o test_ping tests/test_ping.c src/ping/ping.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../src/ping/ping.h"

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
        const void *e_ = (const void *)(expected);                         \
        const void *a_ = (const void *)(actual);                           \
        if (e_ != a_) {                                                    \
            FAIL("%s : attendu %p, obtenu %p", #actual, e_, a_);           \
        }                                                                   \
    } while (0)

#define ASSERT_EQ_ULL(expected, actual)                                    \
    do {                                                                   \
        unsigned long long e_ = (unsigned long long)(expected);            \
        unsigned long long a_ = (unsigned long long)(actual);              \
        if (e_ != a_) {                                                    \
            FAIL("%s : attendu %llu, obtenu %llu", #actual, e_, a_);       \
        }                                                                  \
    } while (0)

/* ================================================================== */
/* sequence_ping_history_new                                          */
/* ================================================================== */

static void test_new_returns_non_null(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(1);
    ASSERT_NOT_NULL(h);
    sequence_ping_history_free(h);
}

static void test_new_sets_sequence(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(42);
    ASSERT_NOT_NULL(h);
    ASSERT_EQ_ULL(42, h->sequence);
    sequence_ping_history_free(h);
}

static void test_new_sets_next_to_null(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(1);
    ASSERT_NOT_NULL(h);
    ASSERT_NULL(h->next);
    sequence_ping_history_free(h);
}

static void test_new_accepts_zero(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(0);
    ASSERT_NOT_NULL(h);
    ASSERT_EQ_ULL(0, h->sequence);
    sequence_ping_history_free(h);
}

static void test_new_accepts_max_uint16(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(65535);
    ASSERT_NOT_NULL(h);
    ASSERT_EQ_ULL(65535, h->sequence);
    sequence_ping_history_free(h);
}

/* ================================================================== */
/* sequence_ping_history_has                                          */
/* ================================================================== */

static void test_has_on_null_history_returns_false(void) {
    ASSERT_FALSE(sequence_ping_history_has(NULL, 0));
    ASSERT_FALSE(sequence_ping_history_has(NULL, 42));
}

static void test_has_finds_the_only_node(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(7);
    ASSERT_NOT_NULL(h);
    ASSERT_TRUE(sequence_ping_history_has(h, 7));
    sequence_ping_history_free(h);
}

static void test_has_rejects_missing_value_in_single_node(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(7);
    ASSERT_NOT_NULL(h);
    ASSERT_FALSE(sequence_ping_history_has(h, 8));
    sequence_ping_history_free(h);
}

static void test_has_finds_head_middle_and_tail(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(1);
    ASSERT_NOT_NULL(h);
    ASSERT_TRUE(sequence_ping_history_add(&h, 2));
    ASSERT_TRUE(sequence_ping_history_add(&h, 3));

    ASSERT_TRUE(sequence_ping_history_has(h, 1));
    ASSERT_TRUE(sequence_ping_history_has(h, 2));
    ASSERT_TRUE(sequence_ping_history_has(h, 3));
    ASSERT_FALSE(sequence_ping_history_has(h, 4));
    sequence_ping_history_free(h);
}

static void test_has_distinguishes_close_values(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(5);
    ASSERT_NOT_NULL(h);
    ASSERT_FALSE(sequence_ping_history_has(h, 4));
    ASSERT_FALSE(sequence_ping_history_has(h, 6));
    ASSERT_TRUE(sequence_ping_history_has(h, 5));
    sequence_ping_history_free(h);
}

static void test_has_handles_boundary_values_together(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(0);
    ASSERT_NOT_NULL(h);
    ASSERT_TRUE(sequence_ping_history_add(&h, 65535));

    ASSERT_TRUE(sequence_ping_history_has(h, 0));
    ASSERT_TRUE(sequence_ping_history_has(h, 65535));
    ASSERT_FALSE(sequence_ping_history_has(h, 1));
    sequence_ping_history_free(h);
}

/* ================================================================== */
/* sequence_ping_history_add                                          */
/* ================================================================== */

static void test_add_rejects_null_history_pointer(void) {
    ASSERT_FALSE(sequence_ping_history_add(NULL, 1));
}

static void test_add_on_empty_history_creates_head(void) {
    t_sequence_ping_history *h = NULL;

    ASSERT_TRUE(sequence_ping_history_add(&h, 1));
    ASSERT_NOT_NULL(h);
    ASSERT_EQ_ULL(1, h->sequence);
    ASSERT_NULL(h->next);
    sequence_ping_history_free(h);
}

static void test_add_appends_after_existing_head(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(1);
    t_sequence_ping_history *original_head = h;

    ASSERT_NOT_NULL(h);
    ASSERT_TRUE(sequence_ping_history_add(&h, 2));

    /* le pointeur de tete ne doit pas changer quand on ajoute a la queue */
    ASSERT_EQ_PTR(original_head, h);
    ASSERT_NOT_NULL(h->next);
    ASSERT_EQ_ULL(2, h->next->sequence);
    ASSERT_NULL(h->next->next);
    sequence_ping_history_free(h);
}

static void test_add_preserves_insertion_order(void) {
    t_sequence_ping_history *h = NULL;
    t_sequence_ping_history *cursor;

    ASSERT_TRUE(sequence_ping_history_add(&h, 10));
    ASSERT_TRUE(sequence_ping_history_add(&h, 20));
    ASSERT_TRUE(sequence_ping_history_add(&h, 30));

    cursor = h;
    ASSERT_NOT_NULL(cursor);
    ASSERT_EQ_ULL(10, cursor->sequence);
    cursor = cursor->next;
    ASSERT_NOT_NULL(cursor);
    ASSERT_EQ_ULL(20, cursor->sequence);
    cursor = cursor->next;
    ASSERT_NOT_NULL(cursor);
    ASSERT_EQ_ULL(30, cursor->sequence);
    ASSERT_NULL(cursor->next);

    sequence_ping_history_free(h);
}

static void test_add_makes_new_entries_findable_via_has(void) {
    t_sequence_ping_history *h = NULL;

    ASSERT_TRUE(sequence_ping_history_add(&h, 100));
    ASSERT_FALSE(sequence_ping_history_has(h, 200));
    ASSERT_TRUE(sequence_ping_history_add(&h, 200));
    ASSERT_TRUE(sequence_ping_history_has(h, 100));
    ASSERT_TRUE(sequence_ping_history_has(h, 200));

    sequence_ping_history_free(h);
}

/* L'implementation n'a pas de logique de deduplication : un doublon cree un
 * second maillon plutot que d'etre rejete. */
static void test_add_allows_duplicate_sequences(void) {
    t_sequence_ping_history *h = NULL;

    ASSERT_TRUE(sequence_ping_history_add(&h, 5));
    ASSERT_TRUE(sequence_ping_history_add(&h, 5));

    ASSERT_NOT_NULL(h);
    ASSERT_EQ_ULL(5, h->sequence);
    ASSERT_NOT_NULL(h->next);
    ASSERT_EQ_ULL(5, h->next->sequence);
    ASSERT_NULL(h->next->next);

    sequence_ping_history_free(h);
}

/* ================================================================== */
/* sequence_ping_history_free                                         */
/* ================================================================== */

static void test_free_null_returns_false(void) {
    ASSERT_FALSE(sequence_ping_history_free(NULL));
}

static void test_free_single_node_returns_true(void) {
    t_sequence_ping_history *h = sequence_ping_history_new(1);
    ASSERT_NOT_NULL(h);
    ASSERT_TRUE(sequence_ping_history_free(h));
}

static void test_free_multiple_nodes_returns_true(void) {
    t_sequence_ping_history *h = NULL;

    ASSERT_TRUE(sequence_ping_history_add(&h, 1));
    ASSERT_TRUE(sequence_ping_history_add(&h, 2));
    ASSERT_TRUE(sequence_ping_history_add(&h, 3));

    ASSERT_TRUE(sequence_ping_history_free(h));
}

/* ================================================================== */
/* Scenario complet                                                    */
/* ================================================================== */

static void test_roundtrip_add_then_has_for_many_sequences(void) {
    t_sequence_ping_history *h = NULL;
    uint16_t i;

    for (i = 0; i < 50; i++) {
        ASSERT_TRUE(sequence_ping_history_add(&h, i));
    }
    for (i = 0; i < 50; i++) {
        ASSERT_TRUE(sequence_ping_history_has(h, i));
    }
    ASSERT_FALSE(sequence_ping_history_has(h, 50));

    sequence_ping_history_free(h);
}

/* ================================================================== */
/* Runner                                                              */
/* ================================================================== */

struct s_test {
    const char *name;
    void      (*fn)(void);
};

static const struct s_test tests[] = {
    { "new_returns_non_null",                     test_new_returns_non_null },
    { "new_sets_sequence",                         test_new_sets_sequence },
    { "new_sets_next_to_null",                     test_new_sets_next_to_null },
    { "new_accepts_zero",                          test_new_accepts_zero },
    { "new_accepts_max_uint16",                    test_new_accepts_max_uint16 },

    { "has_on_null_history_returns_false",         test_has_on_null_history_returns_false },
    { "has_finds_the_only_node",                   test_has_finds_the_only_node },
    { "has_rejects_missing_value_in_single_node",  test_has_rejects_missing_value_in_single_node },
    { "has_finds_head_middle_and_tail",            test_has_finds_head_middle_and_tail },
    { "has_distinguishes_close_values",            test_has_distinguishes_close_values },
    { "has_handles_boundary_values_together",      test_has_handles_boundary_values_together },

    { "add_rejects_null_history_pointer",          test_add_rejects_null_history_pointer },
    { "add_on_empty_history_creates_head",         test_add_on_empty_history_creates_head },
    { "add_appends_after_existing_head",           test_add_appends_after_existing_head },
    { "add_preserves_insertion_order",             test_add_preserves_insertion_order },
    { "add_makes_new_entries_findable_via_has",    test_add_makes_new_entries_findable_via_has },
    { "add_allows_duplicate_sequences",            test_add_allows_duplicate_sequences },

    { "free_null_returns_false",                   test_free_null_returns_false },
    { "free_single_node_returns_true",             test_free_single_node_returns_true },
    { "free_multiple_nodes_returns_true",          test_free_multiple_nodes_returns_true },

    { "roundtrip_add_then_has_for_many_sequences", test_roundtrip_add_then_has_for_many_sequences }
};

#define TEST_COUNT (sizeof(tests) / sizeof(tests[0]))

static int run_forked(const struct s_test *test) {
    pid_t pid;
    int status = 0;

    printf("  %-45s", test->name);
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

    printf("ping (sequence_ping_history) : %zu tests\n\n", (size_t)TEST_COUNT);
    for (i = 0; i < TEST_COUNT; i++) {
        passed += (size_t)run_forked(&tests[i]);
    }
    printf("\n%zu/%zu\n", passed, (size_t)TEST_COUNT);
    return passed == TEST_COUNT ? 0 : 1;
}
