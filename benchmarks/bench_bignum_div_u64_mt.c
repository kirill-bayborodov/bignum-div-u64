/**
 * @file    bench_bignum_div_u64_mt.c
 * @brief   Микробенчмарк для профилирования bignum_div_u64 (MT).
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    27.06.2026
 *
 * @details
 *   MT-вариант для taskset --cpu-list 1-N. Каждый поток работает
 *   со своей копией данных (потокобезопасность уже доказана тестом
 *   test_bignum_div_u64_mt, тут мы её не проверяем, а только грузим CPU).
 *
 *   БЕЗ блокировок: splitmix64 не используется в горячем цикле —
 *   каждый поток получает свой pre-baked срез данных.
 *
 * # Сборка
 *  gcc -O2 -I include -no-pie -fno-omit-frame-pointer -pthread \
 *    benchmarks/bench_bignum_div_u64_mt.c build/bignum_div_u64.o \
 *    -o bin/bench_bignum_div_u64_mt
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <bignum.h>
#include "bignum_div_u64.h"

#define BIGNUM_CAPACITY 32
#define NUM_THREADS 8               /* совпадает с NP обычно */
#define ITERATIONS_PER_THREAD 25000000u
#define PREGEN_DATA_COUNT 8192

/* --- splitmix64 --- */
static uint64_t splitmix_state = 0x9E3779B97F4A7C15ULL;

static inline uint64_t splitmix64_next(void) {
    uint64_t z = (splitmix_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void init_random_bignum(bignum_t *num) {
    int used = (int)((splitmix64_next() & 0x1F)) + 1;
    if (used > BIGNUM_CAPACITY) used = BIGNUM_CAPACITY;
    num->len = (size_t)used;
    for (int i = 0; i < used; ++i) {
        num->words[i] = splitmix64_next();
    }
    num->words[used - 1] |= 1ULL;
}

/* Per-thread контекст: указатели на shared данные (read-only) +
 * локальные q/buf для записи (каждый поток — свои). */
typedef struct {
    int                   tid;
    const bignum_t       *n_sources;
    const uint64_t       *d_u64;
    unsigned              pregen_count;
    bignum_t              q_local;
    uint64_t              rem_local;
    volatile size_t       q_len_sink;
} thread_ctx_t;

static void *thread_func(void *arg) {
    thread_ctx_t *ctx = (thread_ctx_t *)arg;
    unsigned mask = ctx->pregen_count - 1;

    for (uint32_t i = 0; i < ITERATIONS_PER_THREAD; ++i) {
        unsigned idx = i & mask;
        bignum_div_u64(&ctx->q_local, &ctx->n_sources[idx],
                       ctx->d_u64[idx], &ctx->rem_local);
        ctx->q_len_sink = ctx->q_local.len;
    }

    /* Anti-DCE — отдать значение обратно через volatile. */
    if (ctx->q_len_sink == 0xDEADBEEF) {
        fprintf(stderr, "Thread %d: error marker hit\n", ctx->tid);
    }
    return NULL;
}

int main(void) {
    printf("Pregenerating %u data sets for %d threads...\n",
           PREGEN_DATA_COUNT, NUM_THREADS);

    /* Shared read-only данные — splitmix64 тут используется ДО fork'а
     * потоков, так что гонок нет. */
    bignum_t *n_sources = aligned_alloc(64, sizeof(bignum_t) * PREGEN_DATA_COUNT);
    uint64_t *d_u64     = aligned_alloc(64, sizeof(uint64_t) * PREGEN_DATA_COUNT);

    if (!n_sources || !d_u64) {
        perror("Failed to allocate memory");
        return 1;
    }

    splitmix_state = (uint64_t)time(NULL) ^ 0x9E3779B97F4A7C15ULL;
    for (unsigned i = 0; i < PREGEN_DATA_COUNT; ++i) {
        init_random_bignum(&n_sources[i]);
        uint64_t d = splitmix64_next();
        d_u64[i] = (d == 0) ? 1 : d;
    }

    /* Запуск потоков. */
    pthread_t       threads[NUM_THREADS];
    thread_ctx_t    ctxs[NUM_THREADS];

    printf("Starting MT benchmark: %d threads × %u iterations...\n",
           NUM_THREADS, ITERATIONS_PER_THREAD);

    for (int t = 0; t < NUM_THREADS; ++t) {
        ctxs[t] = (thread_ctx_t){
            .tid          = t,
            .n_sources    = n_sources,
            .d_u64        = d_u64,
            .pregen_count = PREGEN_DATA_COUNT,
            .q_len_sink   = 0,
        };
        if (pthread_create(&threads[t], NULL, thread_func, &ctxs[t]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    for (int t = 0; t < NUM_THREADS; ++t) {
        pthread_join(threads[t], NULL);
    }

    /* Суммируем q_len_sink — это наблюдаемый side-effect,
     * который не даст компилятору выкинуть горячий цикл. */
    size_t total_len = 0;
    for (int t = 0; t < NUM_THREADS; ++t) {
        total_len += ctxs[t].q_len_sink;
    }
    printf("MT benchmark finished. total q_len across threads = %zu\n", total_len);

    free(n_sources);
    free(d_u64);
    return 0;
}