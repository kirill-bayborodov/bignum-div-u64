; -----------------------------------------------------------------------------
; @file    bignum_div_u64.asm
; @author  git@bayborodov.com
; @version 1.0.0
; @date    26.11.2025
;
; @brief   Низкоуровневая реализация деления большого числа на uint64_t.
;
; @details
;   Реализует функцию bignum_div_u64 на ассемблере x86-64 (синтаксис YASM)
;   в соответствии с System V AMD64 ABI.
;
;
; @history
;   - rev. 1 (08.08.2025): Первоначальная реализация на ассемблере.
;   - rev. 2 (08.08.2025): Улучшения по ревью (убраны "магические числа").
;   - rev. 3 (08.08.2025): Неудачная попытка рефакторинга, приведшая к провалу тестов.
;   - rev. 4 (08.08.2025): Исправление логики проверки перекрытия и условия завершения цикла.
;   - rev. 5 (08.08.2025): Базовая оптимизация (объединение циклов деления и нормализации).
;   - rev. 6 (08.08.2025): Неудачная попытка продвинутой оптимизации (ошибка в логике `cmovnz`).
;   - rev. 7 (08.08.2025): Критическое исправление логики определения длины (`cmp r10, -1`).
;   - rev. 8 (09.08.2025): Финальная полировка: добавлена проверка на `n->len < 0` и исправлено обнуление `q` при `n->len == 0`.
;   - rev. 9 (09.08.2025): Финальная доработка документации в соответствии с QG. Восстановлена полная история, добавлены разделы "Алгоритм" и "Протокол вызова (ABI)".
;   - rev. 10 (26.11.2025): Removed version control functions and .data section
; -----------------------------------------------------------------------------

section .text

; =============================================================================
; @brief      Выполняет деление большого числа на uint64_t.
;
; @details
;   ### Протокол вызова (ABI)
;   - `rdi`: bignum_t *q        (Указатель на структуру для частного)
;   - `rsi`: const bignum_t *n  (Указатель на структуру делимого)
;   - `rdx`: uint64_t d         (64-битный делитель)
;   - `rcx`: uint64_t *rem      (Указатель на 64-битный остаток)
;   - `rax`: bignum_div_u64_status_t (Возвращаемый код состояния)
;
;   ### Алгоритм
;   1.  **Пролог и сохранение аргументов:** Сохраняются callee-saved регистры
;       (r12-r15), аргументы из rdi, rsi, rdx, rcx копируются в них.
;   2.  **Валидация входных данных:**
;       - Проверяются на NULL указатели `q`, `n`, `rem`.
;       - Проверяется длина `n->len` на отрицательные значения и превышение `BIGNUM_CAPACITY`.
;       - Проверяется делитель `d` на равенство нулю.
;       - Проверяется перекрытие памяти между буферами `q` и `n`.
;   3.  **Обработка тривиального случая:** Если `n->len` равен 0, буфер `q`
;       полностью обнуляется, `rem` устанавливается в 0, и функция успешно завершается.
;   4.  **Основной цикл:**
;       - Цикл выполняется от старшего слова делимого (`n->words[n->len - 1]`) к младшему.
;       - На каждой итерации 128-битное число, составленное из остатка
;         предыдущей итерации (`current_rem`) и текущего слова `n`, делится
;         на `d` с помощью аппаратной инструкции `div`.
;       - Полученная часть частного записывается в `q->words`.
;       - Индекс старшего ненулевого слова частного определяется "на лету"
;         с помощью инструкции `cmovnz` для избежания ветвлений.
;   5.  **Установка длины частного:** Длина `q->len` вычисляется без ветвления
;       на основе найденного индекса с помощью инструкции `lea`.
;   6.  **Ленивое обнуление:** Обнуляется только неиспользуемый "хвост"
;       буфера `q`, начиная с `q->words[q->len]`.
;   7.  **Запись остатка:** Финальный остаток из `current_rem` записывается в `*rem`.
;   8.  **Эпилог:** Восстанавливаются callee-saved регистры, возвращается код состояния.
;
; @abi        System V AMD64 ABI
; @param[in]  rdi: bignum_t *q        (Указатель на структуру для частного)
; @param[in]  rsi: const bignum_t *n  (Указатель на структуру делимого)
; @param[in]  rdx: uint64_t d         (64-битный делитель)
; @param[in]  rcx: uint64_t *rem      (Указатель на 64-битный остаток)
;
; @return     rax: bignum_div_u64_status_t (0, -1, -2, -3, -4)
; @retval  0 – success
; @retval -1 – null pointer
; @retval -2 – division by zero
; @retval -3 – buffer overlap
; @retval -4 – bad length 
; @clobbers   rbx, r8–r15, rcx, rdx
; =============================================================================
; --- Константы ---
%define BIGNUM_CAPACITY 32
%define BIGNUM_LEN_OFFSET (BIGNUM_CAPACITY * 8)
; sizeof(bignum_t) в C = 256 (words) + 4 (len) + 4 (padding) = 264
%define BIGNUM_T_SIZE_ALIGNED 264

; --- Коды состояния ---
%define BIGNUM_DIV_U64_OK                    0
%define BIGNUM_DIV_U64_ERR_NULL_PTR          -1
%define BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO  -2
%define BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP    -3
%define BIGNUM_DIV_U64_ERR_BAD_LENGTH        -4

section .text
align 16
global bignum_div_u64

bignum_div_u64:
    ; --- Пролог ---
    push    r12
    push    r13
    push    r14
    push    r15

    ; --- Сохранение аргументов ---
    mov     r12, rdi    ; q
    mov     r13, rsi    ; n
    mov     r14, rdx    ; d
    mov     r15, rcx    ; rem

    ; 1. Валидация входных данных
    test    r12, r12
    jz      .err_null_ptr
    test    r13, r13
    jz      .err_null_ptr
    test    r15, r15
    jz      .err_null_ptr

    mov     r9d, [r13 + BIGNUM_LEN_OFFSET] ; r9d = n->len
    test    r9d, r9d    ; Проверка на отрицательную длину
    js      .err_bad_length
    cmp     r9d, BIGNUM_CAPACITY
    jg      .err_bad_length

    test    r14, r14 ; d == 0?
    jz      .err_div_by_zero

    ; Проверка перекрытия буферов
    mov     rax, r12
    lea     rcx, [r13 + BIGNUM_T_SIZE_ALIGNED]
    cmp     rax, rcx
    jge     .no_overlap

    mov     rax, r13
    lea     rcx, [r12 + BIGNUM_T_SIZE_ALIGNED]
    cmp     rax, rcx
    jge     .no_overlap
    mov     eax, BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP
    jmp     .exit
.no_overlap:

    ; 2. Инициализация rem и проверка тривиального случая
    mov     qword [r15], 0
    test    r9, r9
    jnz     .main_logic

    ; Обработка n->len == 0: полное обнуление q
    xor     rax, rax
    mov     ecx, BIGNUM_T_SIZE_ALIGNED / 8
    mov     rdi, r12
    rep     stosq
    mov     eax, BIGNUM_DIV_U64_OK
    jmp     .exit

.main_logic:
    ; 4. Оптимизированный цикл (деление + нормализация)
    xor     r8, r8      ; current_rem = 0
    mov     r10, -1     ; q_len_idx = -1
.main_loop:
    dec     r9
    mov     rax, [r13 + r9 * 8]
    mov     rdx, r8
    div     r14
    mov     [r12 + r9 * 8], rax
    mov     r8, rdx

    ; Обновление q_len_idx без ветвления
    mov     rcx, r9
    cmp     r10, -1
    jne     .no_len_update
    test    rax, rax
    cmovnz  r10, rcx
.no_len_update:
    test    r9, r9
    jnz     .main_loop

    ; 5. Установка финальной длины q->len без ветвления
    lea     r11d, [r10 + 1]
    mov     [r12 + BIGNUM_LEN_OFFSET], r11d

    ; 6. Ленивое обнуление "хвоста" буфера q
    mov     rcx, BIGNUM_CAPACITY
    sub     rcx, r11
    jz      .finalize
    lea     rdi, [r12 + r11 * 8]
    xor     rax, rax
    rep     stosq

.finalize:
    ; 7. Финальный остаток
    mov     [r15], r8
    mov     eax, BIGNUM_DIV_U64_OK
    jmp     .exit

.err_null_ptr:
    mov     eax, BIGNUM_DIV_U64_ERR_NULL_PTR
    jmp     .exit

.err_bad_length:
    mov     eax, BIGNUM_DIV_U64_ERR_BAD_LENGTH
    jmp     .exit

.err_div_by_zero:
    mov     eax, BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO
    jmp     .exit

.exit:
    ; --- Эпилог ---
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    ret