/* Second-pass routing and query-vector initialization. Later filtering,
 * scoring and vector cleanup remain native. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef void (TC *query)(void *,void *,void *,uint32_t);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
uint32_t bfv_artillery_second_setup_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],bot=registers[1],pattern=word(frame+0x50);
    uint32_t result=((get_word)(uintptr_t)word(word(pattern)+0x28))((void *)(uintptr_t)pattern);
    if((uint8_t)result) return 0;
    uint32_t manager=word(frame+0x24),output=frame+0xfc;
    store(frame+0x100,0);store(frame+0x104,0);store(frame+0x108,0);
    /* Native captures the receiver before initialization but reads its table
     * after the stores. Keep the raw argument bits without inferring units. */
    ((query)(uintptr_t)word(word(manager)+0x24))((void *)(uintptr_t)manager,
        (void *)(uintptr_t)output,(void *)(uintptr_t)bot,0x44548000u);
    uint32_t begin=word(frame+0x100);store(frame+0x1c,begin);
    /* The empty continuation tests/frees EAX without reloading the allocation. */
    registers[7]=begin;
    return begin==word(frame+0x104)?1:2;
}
__attribute__((naked)) void bfv_artillery_second_setup_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_setup_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; cmp $1,%%eax; je 2f;"
        "popal; jmp %c0; 1: popal; jmp %c1; 2: popal; jmp %c2;"
        : : "i"(BFV_ARTILLERY_SECOND_FILTER),"i"(BFV_ARTILLERY_SECOND_SKIP),
        "i"(BFV_ARTILLERY_SECOND_EMPTY) : "memory");
}
