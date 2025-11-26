/**
 * @file    test_bignum_div_u64.c
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    26.11.2025
 *
 * @brief   Детерминированные тесты для модуля bignum_div_u64.
 *
 * @details
 *   Проверяет основную функциональность, "счастливые пути" и обработку
 *   ошибок, включая новую проверку на длину входного числа.
 *
 *   Для компиляции:
 *   gcc -g -Wall -Wextra -Werror -std=c11 -I. -I include \
 *       src/bignum_div_u64_0.0.2.c test_bignum_div_u64_0.0.2.c \
 *       -o bin/test_bignum_div_u64_0.0.2
 *
 * @history
 *   - rev. 1 (08.08.2025) v0.0.1: Создание и доработка тестов для v0.0.1.
 *   - rev. 2 (08.08.2025) v0.0.1: Усиление покрытия по результатам ревью.
 *   - rev. 1 (08.08.2025) v0.0.2: Первая неполная версия.
 *   - rev. 2 (08.08.2025) v0.0.2: По результатам ревью:
 *                         - Артефакт представлен полностью.
 *                         - Восстановлена полная история ревизий.
 *                         - Добавлен тест `test_error_bad_length` для проверки
 *                           нового кода ошибки BIGNUM_DIV_U64_ERR_BAD_LENGTH.
 */

#include "bignum_div_u64.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h> // Для PRIx64
#include <limits.h>   // Для UINT64_MAX

// --- Улучшенная тестовая обвязка ---
static int tests_failed = 0;
#define RUN_TEST(test_func) \
    do { \
        printf("--- Running test: %s ---\n", #test_func); \
        test_func(); \
    } while (0)

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            printf("    [FAIL] %s\n", message); \
            tests_failed++; \
        } else { \
            printf("    [PASS] %s\n", message); \
        } \
    } while (0)

// --- Вспомогательные функции ---

static void bignum_from_u64(bignum_t *bn, uint64_t val) {
    memset(bn, 0, sizeof(*bn));
    if (val == 0) {
        bn->len = 0;
    } else {
        bn->len = 1;
        bn->words[0] = val;
    }
}

static bool bignum_are_equal(const bignum_t *a, const bignum_t *b) {
    if (a->len != b->len) return false;
    if (a->len == 0) return true;
    return memcmp(a->words, b->words, a->len * sizeof(uint64_t)) == 0;
}

// --- Тестовые случаи ---

void test_happy_path_division() {
    bignum_t n, q, q_expected;
    uint64_t d, r, r_expected;
    n.len = 2;
    n.words[0] = 0x0000000000000000;
    n.words[1] = 0x123456789ABCDEF1;
    d = 0xFFFFFFFFFFFFFFFF;
    q_expected.len = 1;
    q_expected.words[0] = 0x123456789ABCDEF1;
    r_expected = 0x123456789ABCDEF1;
    bignum_div_u64_status_t status = bignum_div_u64(&q, &n, d, &r);
    ASSERT_TRUE(status == BIGNUM_DIV_U64_OK, "Status is OK");
    ASSERT_TRUE(bignum_are_equal(&q, &q_expected), "Quotient is correct");
    ASSERT_TRUE(r == r_expected, "Remainder is correct");
}

void test_n_less_than_d() {
    bignum_t n, q, q_expected;
    uint64_t d, r, r_expected;
    bignum_from_u64(&n, 12345);
    d = 67890;
    bignum_from_u64(&q_expected, 0);
    r_expected = 12345;
    bignum_div_u64_status_t status = bignum_div_u64(&q, &n, d, &r);
    ASSERT_TRUE(status == BIGNUM_DIV_U64_OK, "Status is OK");
    ASSERT_TRUE(bignum_are_equal(&q, &q_expected), "Quotient is 0");
    ASSERT_TRUE(r == r_expected, "Remainder is N");
}

void test_division_by_one() {
    bignum_t n, q, q_expected;
    uint64_t d, r, r_expected;
    n.len = 2;
    n.words[0] = 0xAAAAAAAAAAAAAAAA;
    n.words[1] = 0x1111111111111111;
    d = 1;
    q_expected.len = n.len; // Safe copy
    memset(q_expected.words, 0, sizeof(q_expected.words));
    memcpy(q_expected.words, n.words, n.len * sizeof(uint64_t));
    r_expected = 0;
    bignum_div_u64_status_t status = bignum_div_u64(&q, &n, d, &r);
    ASSERT_TRUE(status == BIGNUM_DIV_U64_OK, "Status is OK");
    ASSERT_TRUE(bignum_are_equal(&q, &q_expected), "Quotient is N");
    ASSERT_TRUE(r == r_expected, "Remainder is 0");
}

void test_max_values() {
    bignum_t n, q, q_expected;
    uint64_t d, r, r_expected;
    d = 0xFFFFFFFFFFFFFFFF;
    n.len = 2;
    n.words[1] = 1;
    n.words[0] = 0; // N = 2^64
    // Эталон: Q = 1, R = 1
    q_expected.len = 1;
    q_expected.words[0] = 1;
    r_expected = 1;
    bignum_div_u64_status_t status = bignum_div_u64(&q, &n, d, &r);
    ASSERT_TRUE(status == BIGNUM_DIV_U64_OK, "Status is OK");
    ASSERT_TRUE(bignum_are_equal(&q, &q_expected), "Quotient is correct for max values");
    ASSERT_TRUE(r == r_expected, "Remainder is correct for max values");
}

void test_leading_zeros_in_dividend() {
    bignum_t n, q, q_expected;
    uint64_t d, r, r_expected;
    n.len = 3;
    n.words[2] = 0; // Ведущий ноль, который должен игнорироваться реализацией
    n.words[1] = 10;
    n.words[0] = 5;
    d = 10;
    // Эталон: Q = (2^64*10 + 5)/10 = 2^63/5 * 10...
    // N = 10 * 2^64 + 5.
    // Q = 1 * 2^64, R = 5.
    q_expected.len = 2;
    q_expected.words[1] = 1;
    q_expected.words[0] = 0;
    r_expected = 5;
    bignum_div_u64_status_t status = bignum_div_u64(&q, &n, d, &r);
    ASSERT_TRUE(status == BIGNUM_DIV_U64_OK, "Status is OK");
    ASSERT_TRUE(bignum_are_equal(&q, &q_expected), "Quotient is correct with leading zeros");
    ASSERT_TRUE(r == r_expected, "Remainder is correct with leading zeros");
}

// --- Тесты на обработку ошибок ---

void test_error_null_pointer() {
    bignum_t n, q;
    uint64_t d = 1, r;
    bignum_from_u64(&n, 10);
    ASSERT_TRUE(bignum_div_u64(NULL, &n, d, &r) == BIGNUM_DIV_U64_ERR_NULL_PTR, "Handles NULL quotient");
    ASSERT_TRUE(bignum_div_u64(&q, NULL, d, &r) == BIGNUM_DIV_U64_ERR_NULL_PTR, "Handles NULL dividend");
    ASSERT_TRUE(bignum_div_u64(&q, &n, d, NULL) == BIGNUM_DIV_U64_ERR_NULL_PTR, "Handles NULL remainder");
}

void test_error_division_by_zero() {
    bignum_t n, q;
    uint64_t d = 0, r;
    bignum_from_u64(&n, 10);
    ASSERT_TRUE(bignum_div_u64(&q, &n, d, &r) == BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO, "Handles division by zero");
}

void test_error_buffer_overlap() {
    bignum_t n;
    uint64_t d = 2, r;
    bignum_from_u64(&n, 10);
    // Передаем один и тот же bignum в качестве q и n
    ASSERT_TRUE(bignum_div_u64(&n, &n, d, &r) == BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP, "Handles buffer overlap");
}


void test_error_bad_length() {
    bignum_t n, q;
    uint64_t r;
    n.len = BIGNUM_CAPACITY + 1; // Заведомо неверная длина
    ASSERT_TRUE(bignum_div_u64(&q, &n, 123, &r) == BIGNUM_DIV_U64_ERR_BAD_LENGTH, "Handles bad length");
}

int main() {
    printf("=== Running Deterministic Tests for bignum_div_u64 ===\n");

    RUN_TEST(test_happy_path_division);
    RUN_TEST(test_n_less_than_d);
    RUN_TEST(test_division_by_one);
    RUN_TEST(test_max_values);
    RUN_TEST(test_leading_zeros_in_dividend);
    RUN_TEST(test_error_null_pointer);
    RUN_TEST(test_error_division_by_zero);
    RUN_TEST(test_error_buffer_overlap);
    RUN_TEST(test_error_bad_length);

    printf("----------------------------------------\n");
    if (tests_failed == 0) {
        printf("All tests passed!\n");
    } else {
        printf("%d test(s) failed.\n", tests_failed);
    }
    printf("========================================\n");

    return (tests_failed == 0) ? 0 : 1;
}