/**
 * @file    bignum_div_u64.h
 * @author  git@bayborodov.com
 * @version 1.0.0
 * @date    26.11.2025
 *
 * @brief   Публичный заголовочный файл для модуля деления большого числа на uint64_t.
 *
 * @details
 *   Определяет API для функции bignum_div_u64, включая типы данных,
 *   коды состояния и прототипы функций.
 *
 * @see     bignum.h
 * @since   1.0.0
 *
 * @history
 *   - rev. 1 (02.08.2025): Первоначальное создание API.
 *   - rev. 2 (02.08.2025): По результатам ревью:
 *                         - Добавлен новый код ошибки BIGNUM_DIV_U64_ERR_BAD_LENGTH.
 *                         - Обновлена документация для отражения проверки n->len.
 *   - rev. 3 (26.11.2025): Removed version control functions.
 */

#ifndef BIGNUM_DIV_U64_H
#define BIGNUM_DIV_U64_H

#include <bignum.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Проверка на наличие определения BIGNUM_CAPACITY из общего заголовка
#ifndef BIGNUM_CAPACITY
#  error "bignum.h must define BIGNUM_CAPACITY"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Коды состояния для функции bignum_div_u64.
 */
typedef enum {
    BIGNUM_DIV_U64_OK                    =  0,
    BIGNUM_DIV_U64_ERR_NULL_PTR          = -1,
    BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO  = -2,
    BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP    = -3,
    /** @brief Ошибка: длина входного числа n->len превышает BIGNUM_CAPACITY. */
    BIGNUM_DIV_U64_ERR_BAD_LENGTH        = -4
} bignum_div_u64_status_t;

/**
 * @brief Выполняет деление большого беззнакового целого числа на 64-битное число.
 *
 * @details
 *   ### Алгоритм
 *   1.  **Валидация:** Проверяются входные указатели на `NULL`, делитель на ноль,
 *       а также буферы `q` и `n` на перекрытие.
 *   2.  **Проверка длины:** Проверяется, что `n->len` не превышает `BIGNUM_CAPACITY`.
 *   3.  **Инициализация:** Буфер результата `q` и остаток `rem` обнуляются.
 *   4.  **Длинное деление:** Выполняется пословное деление, начиная со старшего
 *       слова `n`, с использованием 128/64-битной эмуляции для каждого шага.
 *       Остаток от каждой итерации переносится в следующую.
 *   5.  **Нормализация:** Длина результата `q->len` устанавливается корректно,
 *       удаляя ведущие нули.
 *
 * @param[out] q      Указатель на структуру `bignum_t` для записи частного.
 * @param[in]  n      Указатель на `bignum_t`, представляющую делимое.
 * @param[in]  d      64-битный делитель.
 * @param[out] rem    Указатель на `uint64_t` для записи остатка.
 *
 * @return bignum_div_status_t Код состояния операции.
 * @retval BIGNUM_DIV_U64_OK                    Успешное выполнение.
 * @retval BIGNUM_DIV_U64_ERR_NULL_PTR          Один из входных указателей `NULL`.
 * @retval BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO  Делитель `d` равен нулю.
 * @retval BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP    Обнаружено перекрытие буферов `q` и `n`.
 * @retval BIGNUM_DIV_U64_ERR_BAD_LENGTH        Длина `n->len` превышает `BIGNUM_CAPACITY`.
 */
bignum_div_u64_status_t bignum_div_u64(bignum_t *q, const bignum_t *n, const uint64_t d, uint64_t *rem);

// --- API для отладки

/**
 * @brief Преобразует код состояния bignum_div_u64 в строку.
 *
 * @param[in] status Код состояния для преобразования.
 * @return const char* Строковое представление кода состояния.
 */
const char* bignum_div_status_to_string(bignum_div_u64_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* BIGNUM_DIV_U64_H */
