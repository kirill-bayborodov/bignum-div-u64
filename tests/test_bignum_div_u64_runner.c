/**
 * @file    test_bignum_div_u64_runner.c
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    03.12.2025
 *
 * @brief Интеграционный тест‑раннер для библиотеки libbignum_div_u64.a.
 * @details Применяется для проверки достаточности сигнатур 
 *          в файле заголовка (header) при сборке и линковке
 *          статической библиотеки libbignum_div_u64.a
 *
 * @history
 *   - rev. 1 (03.12.2025): Создание теста
 */  
#include "bignum_div_u64.h"
#include <assert.h>
#include <stdio.h>  
int main() {
 printf("Running test: test_bignum_div_u64_runner... "); 
 bignum_t q_dst = {0};
 bignum_t n_dst;
 n_dst.words[0] = 10;
 n_dst.len=1;
 uint64_t d_u64_dst = {2};
 uint64_t rem_u64_dst = {0};
 bignum_div_u64(&q_dst, &n_dst,  d_u64_dst, &rem_u64_dst); 
 assert(1);
 printf("PASSED\n");   
 return 0;  
}