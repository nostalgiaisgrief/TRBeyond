#ifndef TOMB_FIXED_H
#define TOMB_FIXED_H
#include <stdint.h>
static inline int16_t tomb_word(int32_t x)
{
    uint32_t u = (uint32_t)x & UINT32_C(65535);
    return (int16_t)(u < 32768 ? (int32_t)u : (int32_t)u - 65536);
}
static inline int32_t tomb_long(int64_t x)
{
    uint32_t u = (uint32_t)((uint64_t)x & UINT64_C(4294967295));
    return (int32_t)(u <= INT32_MAX ? (int64_t)u : (int64_t)u - INT64_C(4294967296));
}
/* Defined arithmetic right shift, including negative values. */
static inline int32_t tomb_asr(int32_t x, unsigned bits)
{
    int64_t divisor = INT64_C(1) << bits;
    return (int32_t)(x >= 0 ? x / divisor : -((-(int64_t)x + divisor - 1) / divisor));
}
#endif
