/**
 * @file bignum_div_u64.c
 * @brief C11 reference implementation for bignum_div_u64.
 * @details Validates inputs, computes into a stack-local temporary, normalizes
 * the result, and publishes it only after successful completion. The function
 * is deterministic, allocation-free, and safe for independent concurrent calls.
 */
#include "bignum_div_u64.h"
#include <string.h>

static int overlaps(const void *a, const void *b, size_t size)
{
    const unsigned char *pa = a, *pb = b;
    return pa < pb + size && pb < pa + size;
}

bignum_div_u64_status_t bignum_div_u64(bignum_t *q, const bignum_t *n, uint64_t d, uint64_t *rem)
{
    bignum_t tmp = {0};
    uint64_t remainder = 0U;
    if (q == NULL || n == NULL || rem == NULL) return BIGNUM_DIV_U64_ERR_NULL_PTR;
    if (d == 0U) return BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO;
    if (n->len > BIGNUM_CAPACITY) return BIGNUM_DIV_U64_ERR_BAD_LENGTH;
    if (overlaps(q, n, sizeof(*q))) return BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP;
    for (size_t i = n->len; i > 0U; --i) {
        __uint128_t value = ((__uint128_t)remainder << 64U) | n->words[i - 1U];
        tmp.words[i - 1U] = (uint64_t)(value / d);
        remainder = (uint64_t)(value % d);
    }
    tmp.len = n->len;
    while (tmp.len > 0U && tmp.words[tmp.len - 1U] == 0U) --tmp.len;
    *q = tmp;
    *rem = remainder;
    return BIGNUM_DIV_U64_OK;
}

const char *bignum_div_status_to_string(bignum_div_u64_status_t status)
{
    switch (status) {
    case BIGNUM_DIV_U64_OK: return "ok";
    case BIGNUM_DIV_U64_ERR_NULL_PTR: return "null pointer";
    case BIGNUM_DIV_U64_ERR_DIVISION_BY_ZERO: return "division by zero";
    case BIGNUM_DIV_U64_ERR_BUFFER_OVERLAP: return "buffer overlap";
    case BIGNUM_DIV_U64_ERR_BAD_LENGTH: return "bad length";
    default: return "unknown";
    }
}
