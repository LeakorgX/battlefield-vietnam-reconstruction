/* First-pass node advance and live list-boundary comparison. The next pass
 * remains native. This is an inline evaluator stage, not a complete function. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
uint32_t bfv_artillery_next_candidate_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],bot=registers[1];
    uint32_t node=word(word(frame+0x40)),table=word(bot);
    registers[0]=node;store(frame+0x40,node);
    uint32_t list=((get_word)(uintptr_t)word(table+0x84))((void *)(uintptr_t)bot);
    return node!=word(list+4);
}
__attribute__((naked)) void bfv_artillery_next_candidate_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_next_candidate_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_FILTER),"i"(BFV_ARTILLERY_AFTER_FIRST_PASS) : "memory");
}
