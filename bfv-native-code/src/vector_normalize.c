/* Native tolerance-aware normalization, including the tiny-vector error return. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
static uint32_t less(uint16_t status)
{
    status&=0x0500;
    return status==0x0100||status==0x0400;
}
uint32_t TC bfv_vector_normalize_impl(float *vector,uint32_t *squared_out)
{
    union { float value;uint32_t bits; } squared;
    uint16_t status;
    /* Load all coordinates before computing; round squared length to float
     * before either tolerance test. Do not use the extended length helper. */
    __asm__ volatile(
        "flds 8(%[v]); flds 4(%[v]); flds (%[v]);"
        "fld %%st(0); fmul %%st(1),%%st; fld %%st(2); fmul %%st(3),%%st; faddp;"
        "fld %%st(3); fmul %%st(4),%%st; faddp; fstps %[out];"
        "fstp %%st(0); fstp %%st(0); fstp %%st(0)"
        : [out]"=m"(squared.value) : [v]"r"(vector) : "st","memory");
    *squared_out=squared.bits;
    __asm__ volatile("flds %[sq]; fsubs %[one]; fabs; fcomps %[tol]; fnstsw %%ax"
        : "=a"(status) : [sq]"m"(squared.value),[one]"m"(*(volatile float *)BFV_MATH_ONE),
          [tol]"m"(*(volatile float *)BFV_NORMALIZE_TOLERANCE) : "st");
    if(less(status)) return 0;
    __asm__ volatile("flds %[sq]; fabs; fcomps %[tol]; fnstsw %%ax"
        : "=a"(status) : [sq]"m"(squared.value),
          [tol]"m"(*(volatile float *)BFV_NORMALIZE_TOLERANCE) : "st");
    if(less(status)) {
        *(volatile uint32_t *)(vector+0)=0;
        *(volatile uint32_t *)(vector+1)=0;
        *(volatile uint32_t *)(vector+2)=0;
        return 0xffffb1df;
    }
    /* Unordered comparisons reach this math; there is no nonfinite guard. */
    __asm__ volatile("flds %[sq]; fsqrt; fdivrs %[one];"
        "fld %%st(0); fmuls (%[v]); fstps (%[v]);"
        "fld %%st(0); fmuls 4(%[v]); fstps 4(%[v]);"
        "fmuls 8(%[v]); fstps 8(%[v])"
        : : [sq]"m"(squared.value),[one]"m"(*(volatile float *)BFV_MATH_ONE),
          [v]"r"(vector) : "st","memory");
    return 0;
}
/* The native stack scratch replaces its saved ECX with squared-length bits.
 * POP ECX therefore returns those bits, including both early exits. */
__attribute__((naked,thiscall)) uint32_t bfv_vector_normalize(float *vector __attribute__((unused)))
{
    __asm__ volatile("push %%ecx; push %%esp; call _bfv_vector_normalize_impl; pop %%ecx; ret"
        : : : "memory");
}
