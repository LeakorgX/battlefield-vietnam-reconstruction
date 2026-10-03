/* Reconstructed numerical helpers, using the original initialized lookup tables.
 * The table contents/initialization remain an engine dependency, not copied data.
 */
#include <stdint.h>
#include "target.h"
#include "numeric_curves.h"

/* Force the recovered coefficient to have its original float32 value before
 * promotion. An extended-evaluation C literal can otherwise become exact 0.01
 * instead of the native 0.00999999977648258209228515625. */
static const volatile float score_slope = 0.01f;

/* The original runtime conversion rounds to int64 with the active x87 mode,
 * then corrects toward zero using the float32 residual. A plain C integer cast
 * would introduce undefined behavior for NaNs and different invalid results.
 * Keep the sole hardware primitive explicit; the adjustment is readable C. */
static int32_t truncate_index(long double value)
{
    union { float value; uint32_t bits; } input, residual;
    volatile float input_float = value;
    input.value = input_float;
    int64_t rounded;
    __asm__ volatile ("fldt %1; fistpll %0" : "=m"(rounded) : "m"(value) : "st");
    uint32_t low = (uint32_t)rounded;
    uint32_t high = (uint32_t)((uint64_t)rounded >> 32);
    /* Also preserves the native masked-invalid int64 result's low word (zero). */
    if (low == 0 && (high & 0x7fffffffu) == 0) return (int32_t)low;
    volatile float difference = value - (long double)rounded;
    residual.value = difference;
    if (input.bits & 0x80000000u) {
        uint64_t sum = (uint64_t)(residual.bits ^ 0x80000000u) + 0x7fffffffu;
        low += (uint32_t)(sum >> 32);
    } else {
        uint64_t sum = (uint64_t)residual.bits + 0x7fffffffu;
        low -= (uint32_t)(sum >> 32);
    }
    return (int32_t)low;
}

static long double interpolate(float value, uintptr_t table_global)
{
    long double scaled = (long double)value * 100.0L;
    int32_t whole = truncate_index(scaled);
    long double fraction = scaled - (long double)whole;
    int32_t reverse_index = truncate_index((long double)value * -100.0L);
    uintptr_t base = *(volatile uint32_t *)table_global;
    uintptr_t address = base - (uint32_t)reverse_index * 4u;
    long double first = (1.0L - fraction) * *(volatile float *)address;
    long double second = fraction * *(volatile float *)(address + 4);
    return first + second;
}

long double __attribute__((stdcall)) bfv_bailout_curve(float value)
{
    if (value >= 1.0f) return 1.0L;
    if (value < 0.0f) return 0.0L;
    return interpolate(value, BFV_BAILOUT_CURVE_TABLE);
}

long double __attribute__((stdcall)) bfv_bailout_score(float value)
{
    if (value > 1.0f) return (long double)value * (long double)score_slope + 1.0L;
    if (value < 0.0f) return 0.0L;
    return interpolate(value, BFV_BAILOUT_SCORE_TABLE);
}
