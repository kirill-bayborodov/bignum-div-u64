/**
 * @file    bench_bignum_div_u64_mt.c
 * @brief   Многопоточный микробенчмарк для профилирования bignum_div_u64.
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    26.11.2025
 *
 * @details
 *   Для чистоты измерений все случайные данные генерируются заранее
 *   в основном потоке и передаются в рабочие потоки. Каждый поток
 *   выполняет свой набор вызовов bignum_div_u64, используя
 *   общий пул предварительно сгенерированных данных.
 *
 * @history
 *   - rev 1.0 (12.08.2025): Первоначальная версия.
 *   - rev 1.1 (13.08.2025): Реализована предварительная генерация данных
 *                           в main для исключения rand() из потоков.
 *
 * # Сборка
 * gcc -g -O2 -I include -no-pie -fno-omit-frame-pointer -pthread \
 *   benchmarks/bench_bignum_div_u64_mt.c build/bignum_div_u64.o \
 *   -o bin/bench_bignum_div_u64_mt
 *
 * # Запуск perf
 * /usr/local/bin/perf record -F 9999 -o benchmarks/reports/report_bench_bignum_div_u64_mt -g -- \
 *   bin/bench_bignum_div_u64_mt
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <bignum.h>
#include "bignum_div_u64.h"

// --- Локальные определения для компиляции ---
#define BIGNUM_CAPACITY 32
#define BIGNUM_BITS (BIGNUM_CAPACITY * 64)

#ifndef ITER_PER_THREAD
#  define ITER_PER_THREAD (20000000u * 20)
#endif

#ifndef THREAD_COUNT
#  define THREAD_COUNT 4
#endif

#define PREGEN_DATA_COUNT 8192
#define MAX_SHIFT (BIGNUM_BITS - 1)

// Структура для передачи данных в поток
typedef struct {
    unsigned thread_id;
    unsigned iters;
    const bignum_t* q_sources;// Указатель на общий пул частного
    const bignum_t* n_sources;// Указатель на общий пул исходных чисел делимого
    const uint64_t* d_u64;    // Указатель на общий пул делителя
    const uint64_t* rem_u64;  // Указатель на общий пул отстатка
    unsigned data_count;      // Размер пула
} thread_arg_t;

/** Инициализация случайного bignum_t */
static void init_random_bignum(bignum_t *num) {
    int used = (rand() % BIGNUM_CAPACITY) + 1;
    num->len = used;
    for (int i = 0; i < used; ++i) {
        num->words[i] = ((uint64_t)rand() << 32) | rand();
    }
    for (int i = used; i < BIGNUM_CAPACITY; ++i) {
        num->words[i] = 0;
    }
}

/** Функция, исполняемая каждым потоком */
static void* thread_func(void *arg) {
    const thread_arg_t *t = arg;

    for (unsigned i = 0; i < t->iters; ++i) {
        // Используем общий пул данных, циклически
        // Смещаем индекс на thread_id, чтобы потоки реже работали с одними и теми же данными
        unsigned data_idx = (i + t->thread_id) % t->data_count;

        bignum_t q_dst = t->q_sources[data_idx];
        bignum_t n_dst = t->n_sources[data_idx];
        uint64_t d_u64_dst = t->d_u64[data_idx];
        uint64_t rem_u64_dst = t->rem_u64[data_idx];

        bignum_div_u64(&q_dst, &n_dst,  d_u64_dst, &rem_u64_dst);

        if (n_dst.len == 0xDEADBEEF) {
            return (void*)1;
        }
    }
    return NULL;
}

int main(void) {
    // --- Фаза 1: Предварительная генерация данных в основном потоке ---
    printf("Pregenerating %u data sets for %u threads...\n", PREGEN_DATA_COUNT, THREAD_COUNT);
    bignum_t* q_sources = malloc(sizeof(bignum_t) * PREGEN_DATA_COUNT);
    bignum_t* n_sources = malloc(sizeof(bignum_t) * PREGEN_DATA_COUNT);
    uint64_t* d_u64 = malloc(sizeof(uint64_t) * PREGEN_DATA_COUNT);
    uint64_t* rem_u64 = malloc(sizeof(uint64_t) * PREGEN_DATA_COUNT);

    if (!q_sources || !n_sources || !rem_u64 ||!d_u64) {
        perror("Failed to allocate memory for test data");
        return 1;
    }

    srand((unsigned)time(NULL));
    for (unsigned i = 0; i < PREGEN_DATA_COUNT; ++i) {
        //init_random_bignum(&q_sources[i]);
        init_random_bignum(&n_sources[i]);
        d_u64[i] = (uint64_t)(rand() % MAX_SHIFT);
        //rem_u64[i] = (uint64_t)(rand() % MAX_SHIFT);
    }

    // --- Фаза 2: Запуск потоков и профилирование ---
    printf("Starting benchmark with %u threads, %u iterations each...\n", THREAD_COUNT, ITER_PER_THREAD);
    pthread_t threads[THREAD_COUNT];
    thread_arg_t args[THREAD_COUNT];

    for (unsigned i = 0; i < THREAD_COUNT; ++i) {
        args[i].thread_id = i;
        args[i].iters     = ITER_PER_THREAD;
        args[i].q_sources   = q_sources;
        args[i].n_sources   = n_sources;
        args[i].d_u64    = d_u64;
        args[i].rem_u64    = rem_u64;
        args[i].data_count = PREGEN_DATA_COUNT;
        if (pthread_create(&threads[i], NULL, thread_func, &args[i]) != 0) {
            perror("pthread_create");
            free(q_sources);
            free(n_sources);
            free(d_u64);
            free(rem_u64);
            return 1;
        }
    }

    for (unsigned i = 0; i < THREAD_COUNT; ++i) {
        void *res;
        pthread_join(threads[i], &res);
        if (res != NULL) {
            fprintf(stderr, "Error in thread %u\n", i);
        }
    }
    
    printf("Benchmark finished.\n");

    // --- Фаза 3: Очистка ---
    free(q_sources);
    free(n_sources);
    free(d_u64);
    free(rem_u64);

    return 0;
}
