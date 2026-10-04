/* Second-pass candidate validation. Scoring and vector cleanup remain native. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef long double (TC *get_metric)(void *);
typedef float *(TC *history_lookup)(void *,uint32_t);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint16_t compare_metric(long double value)
{
    uint16_t status;
    __asm__ volatile("fldt %[v]; fcomps %[zero]; fnstsw %%ax"
        : "=a"(status) : [v]"m"(value),
          [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "st");
    return status;
}
static uint16_t compare_history(float reference,const float *timestamp)
{
    uint16_t status;
    __asm__ volatile("flds %[ref]; fsubs %[value]; fcomps %[limit]; fnstsw %%ax"
        : "=a"(status) : [ref]"m"(reference),[value]"m"(*timestamp),
          [limit]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_HISTORY_LIMIT) : "st");
    return status;
}
uint32_t bfv_artillery_second_filter_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],handle=word(word(frame+0x1c)),index=handle&0xffff;
    if(!index) return 0;
    uint32_t entry=word(word(BFV_OBJECT_POOL))+index*8-8;
    if(*(volatile uint16_t *)(uintptr_t)(entry+6)!=(uint16_t)(handle>>16)) return 0;
    uint32_t candidate=word(entry);store(frame+0x18,candidate);
    if(!candidate) return 0;
    uint32_t identity=((get_word)(uintptr_t)word(word(candidate)+0x2c))((void *)(uintptr_t)candidate);
    if(identity==word(frame+0xa0)) return 0;
    /* Identity may replace the candidate pointer in the evaluator frame. */
    uint32_t component_address=word(frame+0x18)+0x24;
    if(!component_address) return 0;
    uint32_t component=word(component_address);store(frame+0x94,component);
    if(!component) return 0;
    if(word(word(frame+0x18)+4)&((1u<<13)|(1u<<16)|(1u<<19))) return 0;
    component=word(frame+0x94);
    long double metric=((get_metric)(uintptr_t)word(word(component)+0x10))((void *)(uintptr_t)component);
    uint16_t status=compare_metric(metric)&0x4100;
    if(status==0x0100||status==0x4000) return 0;
    /* Iterator and history receiver are both read after the metric callback. */
    handle=word(word(frame+0x1c));
    uint32_t behavior=word(frame+0xa4);
    const float *timestamp=((history_lookup)BFV_TARGET_HISTORY)((void *)(uintptr_t)behavior,handle);
    status=compare_history(*(volatile float *)(uintptr_t)(frame+0x58),timestamp)&0x0500;
    return status!=0x0100&&status!=0x0400;
}
__attribute__((naked)) void bfv_artillery_second_filter_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_filter_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_SECOND_ACCEPT),"i"(BFV_ARTILLERY_SECOND_REJECT) : "memory");
}
