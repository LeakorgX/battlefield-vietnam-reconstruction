/* Alternate first-pass eligibility and nested-score initialization.
 * The loop beginning at the accept continuation remains native. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
#define FC __attribute__((fastcall))
typedef uint32_t (TC *get_word)(void *);
typedef uint32_t (TC *get_argument)(void *,uint32_t);
typedef uint8_t (FC *eligible)(uint32_t,uint32_t);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
uint32_t bfv_artillery_alternate_gate_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],target=registers[2];
    if(!(word(target+0x10)&8)) return 0;
    uint32_t record=word(frame+0x18),argument=word(frame+0xa0),handle=word(record);
    if(!((eligible)BFV_TARGET_HANDLE_ELIGIBLE)(handle,argument)) return 0;
    record=word(frame+0x18);handle=word(record);
    uint32_t receiver=word(target+0x20),table=word(receiver);
    store(frame+0x5c,0);store(frame+0x54,handle);registers[0]=target;store(frame+0x14,0);
    uint32_t result=((get_word)(uintptr_t)word(table+0x90))((void *)(uintptr_t)receiver);
    store(frame+0x70,result);
    uint32_t interface=((get_argument)BFV_COLLISION_EVENT_INTERFACE)((void *)(uintptr_t)target,0);
    result=((get_word)(uintptr_t)word(word(interface)+0x3c))((void *)(uintptr_t)interface);
    store(frame+0x94,result);
    return 1;
}
__attribute__((naked)) void bfv_artillery_alternate_gate_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_alternate_gate_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_ALTERNATE_ACCEPT),"i"(BFV_ARTILLERY_FILTER_REJECT) : "memory");
}
