/**
 * @file    bench_bignum_div_u64.c
 * @brief   Микробенчмарк для профилирования bignum_div_u64 (ST).
 * @author  git@bayborodov.com
 * @version 1.2.0
 * @date    27.06.2026
 *
 * @details
 *   rev.12 — честный ST-бенчмарк после профиля rev.11.2:
 *   - Убрано копирование bignum_t q_dst/n_dst (264 байт × ITERATIONS копий
 *     доминировали в профиле как 81% cycles в main).
 *   - splitmix64 PRNG без блокировок (важно для MT-варианта).
 *   - d распределён по [1, UINT64_MAX] (раньше rand() % 2047 — катастрофически
 *     узкое распределение, давало огромное частное на каждой итерации).
 *   - Anti-DCE барьер через volatile чтение q->len после вызова.
 *   - ITERATIONS снижено до 100M (стабильный профиль, не раздувает CI).
 *
 * # Сборка
 *  gcc -O2 -I include -no-pie -fno-omit-frame-pointer \
 *    benchmarks/bench_bignum_div_u64.c build/bignum_div_u64.o \
 *    -o bin/bench_bignum_div_u64
 *
 * # Запуск perf с frame-pointer
 *  /usr/local/bin/perf record -F 9999 -o /tmp/rev12_st.perf -g -- \
 *    bin/bench_bignum_div_u64
 *  /usr/local/bin/perf report -i /tmp/rev12_st.perf --stdio \
 *    --symbol-filter='bignum_div_u64\.([a-z_]+)'
 *
 * @history
 *   - rev 1.0 (12.08.2025): Первоначальная версия.
 *   - rev 1.1 (13.08.2025): Реализована предварительная генерация данных.
 *   - rev 1.2 (13.08.2025): Добавлены локальные определения констант.
 *   - rev 1.2.0 (27.06.2026): Убрано копирование структур, splitmix64,
 *                             d ∈ [1, UINT64_MAX], anti-DCE барьер,
 *                             ITERATIONS=100M.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <bignum.h>
#include "bignum_div_u64.h"

// --- Локальные определения для компиляции ---
#define BIGNUM_CAPACITY 32
#define BIGNUM_BITS (BIGNUM_CAPACITY * 64)

#define ITERATIONS 100000000u
#define PREGEN_DATA_COUNT 8192

/* --- splitmix64: детерминированный, без блокировок, 64-bit состояние --- */
static uint64_t splitmix_state = 0x9E3779B97F4A7C15ULL;

static inline uint64_t splitmix64_next(void) {
    uint64_t z = (splitmix_state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Заполняет bignum случайными словами и устанавливает len ∈ [1, CAPACITY].
 * Ведущее слово гарантированно != 0 (нормализация по контракту bignum_t). */
static void init_random_bignum(bignum_t *num) {
    int used;
    /* splitmix64 даёт любые 64 бита; приведение в [1, CAPACITY] через
     * бит-маску — детерминированно и без % CAPACITY (который даёт смещение). */
    used = (int)((splitmix64_next() & 0x1F)) + 1;  /* [1, 32] */
    if (used > BIGNUM_CAPACITY) used = BIGNUM_CAPACITY;
    num->len = (size_t)used;
    for (int i = 0; i < used; ++i) {
        num->words[i] = splitmix64_next();
    }
    /* Старшее слово != 0 по контракту — гарантируем: */
    num->words[used - 1] |= 1ULL;
    /* Хвост можно не обнулять — функция читает только [0..n->len). */
}

/* Сборка: вынесена инициализация делителя в отдельный helper. */
static uint64_t init_random_d(void) {
    /* d ∈ [1, UINT64_MAX], splitmix64 даёт [0, UINT64_MAX] включительно 0. */
    uint64_t d = splitmix64_next();
    /* splitmix64 может вернуть 0; в этом случае заменяем на 1.
     * Вероятность 2^-64 — практически никогда, но проверка дешёвая. */
    return d == 0 ? 1 : d;
}

int main(void) {
    /* --- Фаза 1: Предварительная генерация данных --- */
    printf("Pregenerating %u data sets (splitmix64)...\n", PREGEN_DATA_COUNT);

    /* q: переиспользуем буфер между итерациями, обнуляем заново каждый раз
     * через сам bignum_div_u64 (он перезаписывает нужные слова).
     * n: предварительно сгенерированные нормализованные числа.
     * d: предварительно сгенерированные делители.
     * rem: один слот, переиспользуем. */
    bignum_t *n_sources = aligned_alloc(64, sizeof(bignum_t) * PREGEN_DATA_COUNT);
    uint64_t *d_u64 = aligned_alloc(64, sizeof(uint64_t) * PREGEN_DATA_COUNT);
    bignum_t  q_buf;       /* единственный буфер частного, без копий */
    uint64_t  rem_buf;     /* единственный слот остатка */

    if (!n_sources || !d_u64) {
        perror("Failed to allocate memory for test data");
        return 1;
    }

    /* seed splitmix64 на основе времени — разные прогоны неидентичны,
     * но в рамках одного прогона детерминированы (важно для диффов профилей). */
    splitmix_state = (uint64_t)time(NULL) ^ 0x9E3779B97F4A7C15ULL;

    for (unsigned i = 0; i < PREGEN_DATA_COUNT; ++i) {
        init_random_bignum(&n_sources[i]);
        d_u64[i] = init_random_d();
    }

    /* --- Фаза 2: "Горячий" цикл для профилирования --- */
    printf("Starting benchmark with %u iterations...\n", ITERATIONS);

    /* volatile-sink: компилятор не должен выкинуть вызов bignum_div_u64.
     * Если q->len становится == 0xDEADBEEF — ловим (никогда не сработает). */
    volatile size_t q_len_sink = 0;

    for (uint32_t i = 0; i < ITERATIONS; ++i) {
        unsigned data_idx = i & (PREGEN_DATA_COUNT - 1);  /* степень 2 — быстрее % */
        bignum_div_u64(&q_buf, &n_sources[data_idx], d_u64[data_idx], &rem_buf);
        q_len_sink = q_buf.len;
    }

    /* Anti-DCE: реально прочитаем q_len_sink, чтобы компилятор не удалил цикл.
     * Сам volatile read не считается за "наблюдение" значения, но мы ещё и
     * выводим его (через printf) — это полноценный наблюдаемый side-effect. */
    if (q_len_sink == 0xDEADBEEF) {
        printf("Error marker hit.\n");
        return 1;
    }

    printf("Benchmark finished. last q_len = %zu, rem = %lu\n",
           (size_t)q_len_sink, (unsigned long)rem_buf);

    free(n_sources);
    free(d_u64);
    return 0;
}