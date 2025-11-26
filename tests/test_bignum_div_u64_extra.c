/**
 * @file    test_bignum_div_u64_extra.c
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    26.11.2025
 *
 * @brief   Экстра-тесты (на робастность) для модуля bignum_div_u64.
 *
 * @details
 *   Проверяет поведение функции на граничных и некорректных входных
 *   данных, таких как NULL-указатели, деление на ноль и перекрытие буферов.
 *
 *   Для компиляции:
 *   gcc -g -Wall -Wextra -Werror -std=c11 -I. -I include \
 *       src/bignum_div_u64_0.0.2.c test_bignum_div_u64_extra_0.0.2.c \
 *       -o bin/test_bignum_div_u64_extra_0.0.2
 *
 * @history
 *   - rev. 1 (08.08.2025) v0.0.1: Создание тестов на робастность.
 *   - rev. 2 (08.08.2025) v0.0.1: Усиление покрытия по результатам ревью.
 *   - rev. 1 (08.08.2025) v0.0.2: Адаптирован для тестирования версии 0.0.2.
 */

#include "bignum_div_u64.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

// --- Макрос для ассертов ---
#define ASSERT_TRUE(condition, message) \
    do { \
        if (condition) { \
            printf("    [PASS] %s\n", message); \
        } else { \
            printf("    [FAIL] %s\n", message); \
            tests_failed++; \
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

// --- Тестовые сценарии ---

static int tests_failed = 0;

void test_robustness_null_pointers() {
    printf("--- Running test: test_robustness_null_pointers ---\n");
    bignum_t n, q;
    uint64_t r;
    bignum_from_u64(&n, 1);
    ASSERT_TRUE(bignum_div_u64(NULL, &n, 1, &r) == BIGNUM_DIV_U64_ERR_NULL_PTR, "Handles NULL quotient");
    ASSERT_TRUE(bignum_div_u64(&q, NULL, 1, &r) == BIGNUM_DIV_U64_ERR_NULL_PTR, "Handles NULL dividend");
    ASSERT_TRUE(bignum_div_u64(&q, &n, 1, NULL) == BIGNUM_DIV_U64_ERR_NULL_PTR, "Handles NULL remainder");
}

void test_robustness_division_by_zero() {
    printf("--- Running test: test_robustness_division_by_zero ---\n");
    bignum_t n, q;
    uint64_t r;
    bignum_from_u64(&n, 1);
    ASSERT_TRUE(bignum_div_u64(&q, &n, 0, &r) == BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO, "Handles division by zero");
}

void test_robustness_full_buffer_overlap() {
    printf("--- Running test: test_robustness_full_buffer_overlap ---\n");
    bignum_t n;
    uint64_t r;
    bignum_from_u64(&n, 123);
    ASSERT_TRUE(bignum_div_u64(&n, &n, 1, &r) == BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP, "Handles full buffer overlap");
}

void test_robustness_partial_buffer_overlap() {
    printf("--- Running test: test_robustness_partial_buffer_overlap ---\n");
    bignum_t n;
    uint64_t r;
    bignum_from_u64(&n, 123);
    // q указывает на середину n
    bignum_t *q_alias = (bignum_t*)&n.words[2];
    ASSERT_TRUE(bignum_div_u64(q_alias, &n, 1, &r) == BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP, "Handles partial buffer overlap");
}

void test_robustness_no_overlap() {
    printf("--- Running test: test_robustness_no_overlap ---\n");
    bignum_t n, q;
    uint64_t r;
    bignum_from_u64(&n, 123);
    ASSERT_TRUE(bignum_div_u64(&q, &n, 10, &r) == BIGNUM_DIV_U64_OK, "Handles non-overlapping buffers correctly");
}

void test_robustness_adjacent_buffers() {
    printf("--- Running test: test_robustness_adjacent_buffers ---\n");
    bignum_t buffers[2];
    bignum_t *q = &buffers[0];
    bignum_t *n = &buffers[1];
    uint64_t r;
    bignum_from_u64(n, 123);
    ASSERT_TRUE(bignum_div_u64(q, n, 10, &r) == BIGNUM_DIV_U64_OK, "Handles adjacent (non-overlapping) buffers");
}

int main() {
    printf("\n=== Running Extra (Robustness) Tests for bignum_div_u64===\n");

    test_robustness_null_pointers();
    test_robustness_division_by_zero();
    test_robustness_full_buffer_overlap();
    test_robustness_partial_buffer_overlap();
    test_robustness_no_overlap();
    test_robustness_adjacent_buffers();

    printf("----------------------------------------\n");
    if (tests_failed == 0) {
        printf("All robustness tests passed!\n");
    } else {
        printf("%d robustness test(s) failed.\n", tests_failed);
    }
    printf("========================================\n");

    return (tests_failed > 0);
}
