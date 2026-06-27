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
;   - rev. 11 (27.06.2026): Попытка оптимизации: вынос q_len_idx из main_loop. РЕГРЕСС.
;   - rev. 12 (27.06.2026): Корректная реализация с early-exit через jnz.
;                            - Удалено ленивое обнуление хвоста через rep stosq.
;                            - Проверка d == 0 поднята выше проверки n->len.
;                            - rbx удалён из @clobbers.
;   - rev. 13 (27.06.2026): Фикс по результатам ревью:
;                           - [BUG FIX] Запись q->len теперь 64-битная.
;                             В LP64 size_t = 8 байт; r11d/mov dword
;                             оставляли мусор в верхних 4 байтах поля len
;                             при переиспользовании буфера q (latent bug,
;                             не ловился тестами потому что q инициализировался
;                             через memset). Заменено на mov [..], r11 / mov qword [..], 0.
;                           - [OPTIMIZATION] Нормализация O(N) → O(1).
;                             Цикл .normalize_scan удалён полностью.
;                             По теореме о делении нормализованного N на d>=1,
;                             длина частного либо L, либо L-1 (либо 0 при L=1
;                             и N[0]<d). Проверяется только Q[L-1]: != 0 → L,
;                             == 0 → L-1. Убраны .normalize_scan и .set_q_len
;                             как отдельные источники branch-misses (~29% BM
;                             в rev.12 профиле).
;                           - [CLEANUP] Валидация n->len упрощена до беззнаковой:
;                             mov r9, [..]; cmp r9, 32; ja .err_bad_length.
;                             size_t беззнаковый, "отрицательные" значения
;                             автоматически ловятся через ja как огромные
;                             положительные.
;   - rev. 14 (27.06.2026): 
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
;   2.  **Валидация входных данных (по убыванию дешевизны):**
;       - NULL-проверки указателей q, n, rem.
;       - Делитель d на ноль.
;       - Длина n->len: проверка n->len <= BIGNUM_CAPACITY беззнаково.
;       - Перекрытие буферов q и n по диапазонам [base, base+264).
;   3.  **Тривиальный случай n->len == 0:** полное обнуление q, rem=0, OK.
;   4.  **Основной цикл (main_loop):** только деление, от старшего к младшему.
;   5.  **Нормализация O(1):** проверка Q[n->len - 1].
;       - Q[n->len-1] != 0 → q->len = n->len
;       - Q[n->len-1] == 0 → q->len = n->len - 1 (или 0 если n->len == 1)
;       Обоснование: после main_loop для нормализованного N и d >= 1
;       частное Q ненулевое хотя бы в одной из позиций L-1 или L-2.
;   6.  **Финальный остаток:** записывается в *rem.
;   7.  **Эпилог:** восстановление callee-saved, возврат кода состояния.
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
; @clobbers   r8–r15, rcx, rdx
; =============================================================================
; --- Константы ---
%define BIGNUM_CAPACITY 32
%define BIGNUM_LEN_OFFSET (BIGNUM_CAPACITY * 8)
; sizeof(bignum_t) в C = 256 (words) + 8 (len на LP64) = 264.
; Корректно только для LP64 (x86-64 System V).
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

    ; 1. Валидация входных данных (по убыванию дешевизны)
    test    r12, r12
    jz      .err_null_ptr
    test    r13, r13
    jz      .err_null_ptr
    test    r15, r15
    jz      .err_null_ptr

    test    r14, r14
    jz      .err_div_by_zero

    ; Длина n->len: 64-битная беззнаковая проверка.
    ; size_t беззнаковый, поэтому "отрицательные" значения автоматически
    ; > BIGNUM_CAPACITY и ловятся через ja.
    mov     r9, [r13 + BIGNUM_LEN_OFFSET]
    cmp     r9, BIGNUM_CAPACITY
    ja      .err_bad_length

    ; Проверка перекрытия буферов q и n
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

    ; n->len == 0: полное обнуление q
    xor     rax, rax
    mov     ecx, BIGNUM_T_SIZE_ALIGNED / 8
    mov     rdi, r12
    rep     stosq
    mov     eax, BIGNUM_DIV_U64_OK
    jmp     .exit

.main_logic:
    xor     r8, r8              ; current_rem = 0
.main_loop:
    dec     r9
    mov     rax, [r13 + r9 * 8]
    mov     rdx, r8             
    div     r14
    mov     [r12 + r9 * 8], rax
    mov     r8, rdx             
    test    r9, r9
    jnz     .main_loop

    ; 4. Нормализация O(1).
    ; Для нормализованного N и d >= 1, после main_loop:
    ;   - Если N[L-1] >= d, то Q[L-1] >= 1, длина = L.
    ;   - Если N[L-1] < d, то Q[L-1] = 0, но Q[L-2] >= 1 (так как на
    ;     итерации L-2 делим 128-бит число N[L-1]:N[L-2] на d <= UINT64_MAX,
    ;     старшая часть N[L-1] >= 1 даёт частное >= 1).
    ;   - Исключение L=1: Q[0] = N[0]/d, длина 1 или 0.
    ; Итого: проверяем только Q[L-1]; если != 0 → длина L, иначе → L-1.
    ;
    ; r9 сейчас == 0 (выход из main_loop через test r9,r9; jnz).
    ; Перезагружаем n->len в rcx.
    mov     rcx, [r13 + BIGNUM_LEN_OFFSET]
    mov     rax, [r12 + rcx * 8 - 8]    ; rax = Q[n->len - 1]
    lea     r10, [rcx - 1]              ; r10 = n->len - 1 (вычисляем заранее)
    test    rax, rax
    cmovz   rcx, r10                    ; Если rax == 0, то rcx = r10
    mov     [r12 + BIGNUM_LEN_OFFSET], rcx

.finalize:
    ; 5. Финальный остаток
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
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    ret