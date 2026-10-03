/* Shared float selectors and three-dimensional vector math.
 * Float arguments retain their original 32-bit storage; results use native ST0.
 * Small x87 primitives preserve NaN selection, precision and exception status. */
#include <stdint.h>
#define SC __attribute__((stdcall))
#define TC __attribute__((thiscall))

static uint16_t compare(float left, float right)
{
    uint16_t status;
    __asm__ volatile("flds %[left]; fcomps %[right]; fnstsw %%ax"
        : "=a"(status) : [left]"m"(left), [right]"m"(right) : "st");
    return status;
}
static uint32_t less(uint16_t status)
{
    /* Native TEST AH,5 / parity branch: unordered selects the other branch. */
    status &= 0x0500;
    return status == 0x0100 || status == 0x0400;
}

long double SC bfv_float_minimum(float left, float right)
{ return less(compare(left, right)) ? left : right; }

long double SC bfv_float_maximum(float left, float right)
{ return less(compare(left, right)) ? right : left; }

long double SC bfv_float_clamp(float lower, float value, float upper)
{
    /* Lower is tested first, even for reversed bounds. Unordered lower/value
     * comparisons continue to the upper test; unordered upper/value uses value. */
    if (!(compare(lower, value) & 0x4100)) return lower;
    return less(compare(upper, value)) ? upper : value;
}

long double TC bfv_vector_length(const float *vector)
{
    long double result;
    /* sqrt((x*x + y*y) + z*z), retaining native extended intermediates.
     * Load z,y,x before arithmetic to retain original NaN operand precedence. */
    __asm__ volatile(
        "flds 8(%[v]); flds 4(%[v]); flds (%[v]);"
        "fld %%st(0); fmul %%st(1),%%st;"
        "fld %%st(2); fmul %%st(3),%%st; faddp;"
        "fld %%st(3); fmul %%st(4),%%st; faddp; fsqrt;"
        "fstp %%st(3); fstp %%st(0); fstp %%st(0); fstpt %[out]"
        : [out]"=m"(result) : [v]"r"(vector) : "st", "memory");
    return result;
}

float *TC bfv_vector_divide(float *vector, float divisor)
{
    const float one = 1.0f;
    /* One extended reciprocal, then three individually rounded float stores.
     * There is intentionally no zero-length or nonfinite-value guard. */
    __asm__ volatile(
        "flds %[one]; fdivs %[d];"
        "fld %%st(0); fmuls (%[v]); fstps (%[v]);"
        "fld %%st(0); fmuls 4(%[v]); fstps 4(%[v]);"
        "fmuls 8(%[v]); fstps 8(%[v])"
        : : [one]"m"(one), [d]"m"(divisor), [v]"r"(vector) : "st", "memory");
    return vector;
}
