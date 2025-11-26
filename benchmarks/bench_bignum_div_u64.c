/**
 * @file    bench_bignum_div_u64.c
 * @brief   Микробенчмарк для профилирования bignum_div_u64.
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    03.10.2025
 *
 * @details
 *   Вызывает функцию bignum_div_u64 на случайных
 *   больших числах многократно, чтобы perf успел
 *   собрать достаточное число сэмплов.
 *
 *   Для чистоты измерений все случайные данные (числа и сдвиги)
 *   генерируются заранее и помещаются в массив. Основной цикл,
 *   который профилируется, выполняет только копирование структуры
 *   и вызов целевой функции, исключая медленный вызов rand().
 *
 * @history
 *   - rev 1.0 (12.08.2025): Первоначальная версия.
 *   - rev 1.1 (13.08.2025): Реализована предварительная генерация данных.
 *   - rev 1.2 (13.08.2025): Добавлены локальные определения констант
 *                           BIGNUM_CAPACITY и BIGNUM_BITS для компиляции.
 *
 * # Сборка
 *  gcc -g -O2 -I include -no-pie -fno-omit-frame-pointer \
 *    benchmarks/bench_bignum_div_u64.c build/bignum_div_u64.o \
 *    -o bin/bench_bignum_div_u64
 *
 * # Запуск perf с записью стека через frame-pointer
 * /usr/local/bin/perf record -F 9999 -o benchmarks/reports/report_bench_bignum_div_u64 -g -- bin/bench_bignum_div_u64
 *
 * # Отчёт, отфильтрованный по символу
 * /usr/local/bin/perf report -i benchmarks/reports/report_bench_bignum_div_u64 --stdio --symbol-filter=bignum_div_u64
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <bignum.h>
#include "bignum_div_u64.h"

// --- Локальные определения для компиляции ---
// Эти константы должны быть синхронизированы с ассемблерным кодом.
#define BIGNUM_CAPACITY 32
#define BIGNUM_BITS (BIGNUM_CAPACITY * 64)

// Увеличиваем количество итераций для более надежных измерений
#define ITERATIONS (100000000u * 20)

// Количество предварительно сгенерированных наборов данных
#define PREGEN_DATA_COUNT 8192

// Максимальный сдвиг
#define MAX_SHIFT (BIGNUM_BITS - 1)

/** Заполняет bignum случайными словами и устанавливает len. */
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

int main(void) {
    // --- Фаза 1: Предварительная генерация данных ---
    printf("Pregenerating %u data sets...\n", PREGEN_DATA_COUNT);

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

    // --- Фаза 2: "Горячий" цикл для профилирования ---
    printf("Starting benchmark with %u iterations...\n", ITERATIONS);

    for (uint32_t i = 0; i < ITERATIONS; ++i) {
        // Используем предварительно сгенерированные данные, циклически обращаясь к ним
        unsigned data_idx = i % PREGEN_DATA_COUNT;
        
        // Копируем исходное число, чтобы не портить эталон
        bignum_t q_dst = q_sources[data_idx];
        bignum_t n_dst = n_sources[data_idx];
        uint64_t d_u64_dst = d_u64[data_idx];
        uint64_t rem_u64_dst = rem_u64[data_idx];
        
        // Вызываем целевую функцию (bignum_t *q, const bignum_t *n, const uint64_t d, uint64_t *rem);
        bignum_div_u64(&q_dst, &n_dst,  d_u64_dst, &rem_u64_dst);
        
        // Эта проверка не дает компилятору выбросить вызов функции
        if (n_dst.len == 0xDEADBEEF) {
            // Никогда не выполнится
            printf("Error marker hit.\n");
            return 1;
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
