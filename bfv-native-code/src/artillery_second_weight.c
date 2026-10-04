/* Second-pass timestamp-dependent weight. Aiming and later scores remain native. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
#define SC __attribute__((stdcall))
typedef uint32_t (TC *lookup_timestamp)(void *,uint32_t,float *);
typedef long double (SC *clamp_value)(float,float,float);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
void bfv_artillery_second_weight_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],bot=registers[1];
    uint32_t iterator=word(frame+0x1c),table=word(bot),handle=word(iterator);
    uint32_t found=((lookup_timestamp)(uintptr_t)word(table+0x1a4))(
        (void *)(uintptr_t)bot,handle,(float *)(uintptr_t)(frame+0x10c));
    if(!(uint8_t)found) { store(frame+0x2c,0x3f800000);return; }
    float scaled;
    __asm__ volatile("flds 88(%[f]); fsubs 268(%[f]); fmuls %[scale]; fstps %[out]"
        : [out]"=m"(scaled) : [f]"r"(frame),
          [scale]"m"(*(volatile float *)BFV_ARTILLERY_SECOND_WEIGHT_SCALE) : "st","memory");
    long double clamped=((clamp_value)BFV_FLOAT_CLAMP)(0.0f,scaled,1.0f);
    __asm__ volatile("fldt %[value]; fmuls %[scale]; fsubrs %[one]; fstps 44(%[f])"
        : : [value]"m"(clamped),[f]"r"(frame),
          [scale]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_SCALE),
          [one]"m"(*(volatile float *)BFV_ARTILLERY_CATEGORY_SCALE) : "st","memory");
}
__attribute__((naked)) void bfv_artillery_second_weight_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_weight_phase;"
        "add $4,%%esp; popal; jmp %c0;"
        : : "i"(BFV_ARTILLERY_SECOND_WEIGHT_CONTINUE) : "memory");
}
