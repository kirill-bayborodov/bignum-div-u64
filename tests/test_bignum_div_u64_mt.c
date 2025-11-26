/**
 * @file    test_bignum_div_u64_mt.c
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    26.11.2025
 *
 * @brief   Тест на потокобезопасность для модуля bignum_div_u64.
 *
 * @details
 *   Создает несколько потоков, каждый из которых выполняет деление
 *   с уникальными, неразделяемыми данными. Успешное прохождение теста
 *   демонстрирует, что функция не использует глобальное или статическое
 *   состояние и является потокобезопасной.
 *
 *   Для компиляции:
 *   gcc -g -Wall -Wextra -Werror -std=c11 -I. -I include \
 *       src/bignum_div_u64_0.0.2.c test_bignum_div_u64_mt_0.0.2.c \
 *       -o bin/test_bignum_div_u64_mt_0.0.2 -pthread
 *
 * @history
 *   - rev. 1 (08.08.2025) v0.0.1: Создание теста.
 *   - rev. 2 (08.08.2025) v0.0.1: Усиление покрытия по результатам ревью.
 *   - rev. 1 (08.08.2025) v0.0.2: Адаптирован для тестирования версии 0.0.2.
 */

#include "bignum_div_u64.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <limits.h>

#define NUM_THREADS 12

typedef struct {
    int thread_id;
    bignum_t n;
    uint64_t d;
    bignum_t q_expected;
    uint64_t r_expected;
    bool success;
} test_data_t;

static void bignum_from_u64_array(bignum_t *bn, const uint64_t *words, size_t len) {
    memset(bn, 0, sizeof(*bn));
    assert(len <= BIGNUM_CAPACITY);
    bn->len = (int)len;
    if (len > 0) {
        memcpy(bn->words, words, len * sizeof(uint64_t));
    }
}

static bool bignum_are_equal(const bignum_t *a, const bignum_t *b) {
    if (a->len != b->len) return false;
    if (a->len == 0) return true;
    return memcmp(a->words, b->words, a->len * sizeof(uint64_t)) == 0;
}

void* thread_func(void* arg) {
    test_data_t* data = (test_data_t*)arg;
    bignum_t q_actual;
    uint64_t r_actual;

    bignum_div_u64_status_t status = bignum_div_u64(&q_actual, &data->n, data->d, &r_actual);

    if (status == BIGNUM_DIV_U64_OK &&
        bignum_are_equal(&q_actual, &data->q_expected) &&
        r_actual == data->r_expected) {
        data->success = true;
    } else {
        data->success = false;
    }

    return NULL;
}

int main() {
    printf("\n=== Running Enhanced Thread-Safety Test for bignum_div_u64 ===\n");
    pthread_t threads[NUM_THREADS];
    test_data_t test_data[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; ++i) {
        test_data[i].thread_id = i;
        test_data[i].success = false;

        uint64_t n_val = (uint64_t)i * 1000 + 12345;
        uint64_t d_val = (uint64_t)i + 2;
        const uint64_t n_words[] = {n_val, (uint64_t)i + 1};

        bignum_from_u64_array(&test_data[i].n, n_words, 2);
        test_data[i].d = d_val;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        // Эталонное вычисление
        unsigned __int128 full_n = ((unsigned __int128)n_words[1] << 64) | n_words[0];
        unsigned __int128 q_val   = full_n / d_val;

#pragma GCC diagnostic pop

        test_data[i].r_expected = full_n % d_val;
        const uint64_t q_exp_words[] = {(uint64_t)q_val, (uint64_t)(q_val >> 64)};
        size_t q_len = (q_val > UINT64_MAX) ? 2 : ((q_val > 0) ? 1 : 0);
        bignum_from_u64_array(&test_data[i].q_expected, q_exp_words, q_len);

        pthread_create(&threads[i], NULL, thread_func, &test_data[i]);
    }

    int overall_success = 1;
    printf("--- Verifying results ---\n");
    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
        if (test_data[i].success) {
            printf("    [OVERALL PASS] Thread %d completed successfully.\n", i);
        } else {
            printf("    [OVERALL FAIL] Thread %d failed.\n", i);
            overall_success = 0;
        }
    }

    printf("----------------------------------------\n");
    if (overall_success) {
        printf("Thread-safety test passed! The function is thread-safe.\n");
    } else {
        printf("Thread-safety test FAILED.\n");
    }
    printf("========================================\n");

    return !overall_success;
}