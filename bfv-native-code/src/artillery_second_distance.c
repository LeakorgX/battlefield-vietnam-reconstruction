/* Second-pass position difference, distance floor and normalized direction. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
#define SC __attribute__((stdcall))
long double TC bfv_vector_length(const float *);
long double SC bfv_float_maximum(float,float);
float *TC bfv_vector_divide(float *,float);
void bfv_artillery_second_distance_phase(uint32_t *registers)
{
    uint32_t frame=registers[3];
    /* All differences are computed before stores. The rounded z store at
     * +0x124 overwrites an origin field only after its old value was consumed.
     * Preserve the intervening word copies and x/y rounding order. */
    __asm__ volatile(
        "flds 196(%[f]); fsubs 296(%[f]);"
        "flds 200(%[f]); fsubs 300(%[f]);"
        "flds 204(%[f]); fsubs 304(%[f]); fstps 440(%[f]);"
        "movl 440(%[f]),%%eax; fxch; movl %%eax,292(%[f]);"
        "fstps 284(%[f]); movl 284(%[f]),%%edx; movl %%eax,128(%[f]);"
        "fstps 288(%[f]); movl 288(%[f]),%%eax;"
        "movl %%edx,120(%[f]); movl %%eax,124(%[f])"
        : : [f]"r"(frame) : "eax","edx","st","memory");
    long double length=bfv_vector_length((const float *)(uintptr_t)(frame+0x78));
    float rounded;
    __asm__ volatile("fldt %[length]; fstps %[out]"
        : [out]"=m"(rounded) : [length]"m"(length) : "st");
    long double distance=bfv_float_maximum(rounded,0.5f);
    __asm__ volatile("fldt %[distance]; fstps 40(%[f])"
        : : [distance]"m"(distance),[f]"r"(frame) : "st","memory");
    bfv_vector_divide((float *)(uintptr_t)(frame+0x78),*(volatile float *)(uintptr_t)(frame+0x28));
}
__attribute__((naked)) void bfv_artillery_second_distance_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_distance_phase;"
        "add $4,%%esp; popal; jmp %c0;"
        : : "i"(BFV_ARTILLERY_SECOND_DISTANCE_CONTINUE) : "memory");
}
