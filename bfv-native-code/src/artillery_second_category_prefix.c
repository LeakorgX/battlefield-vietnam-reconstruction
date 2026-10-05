/* One-time prefix before the re-enterable second-pass category factor. */
#include <stdint.h>
#include "collision_events.h"
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }
void bfv_artillery_second_category_prefix_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],candidate=registers[4],descriptor=registers[0];
    store(frame+0x24,word(descriptor));
    store(frame+0x5c,0);store(frame+0x60,candidate);store(frame+0x14,0);
    /* Original ESP is four bytes lower while the zero event argument is pushed. */
    store(frame+0xec,get(word(candidate+0x20),0x90));
    uint32_t interface=bfv_event_interface((void *)(uintptr_t)candidate,0);
    store(frame+0x30,get(interface,0x3c));
    registers[0]=word(frame+0xa4);
}
__attribute__((naked)) void bfv_artillery_second_category_prefix_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_category_prefix_phase;"
        "add $4,%%esp; popal; jmp %c0" : : "i"(BFV_ARTILLERY_SECOND_CATEGORY_FACTOR) : "memory");
}
