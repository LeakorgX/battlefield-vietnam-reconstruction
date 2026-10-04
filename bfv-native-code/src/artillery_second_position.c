/* Second-pass event-interface position with sequential fallback point stores. */
#include <stdint.h>
#include "collision_events.h"
#include "target.h"
#define TC __attribute__((thiscall))
typedef void (TC *write_position)(void *,float *);
typedef const float *(TC *get_position)(void *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
void bfv_artillery_second_position_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],candidate=word(frame+0x18);registers[0]=candidate;
    uint32_t interface=bfv_event_interface((void *)(uintptr_t)candidate,2);
    store(frame+0x24,interface);
    if(interface) {
        ((write_position)(uintptr_t)word(word(interface)+0x2c))((void *)(uintptr_t)interface,
            (float *)(uintptr_t)(frame+0xc4));
    } else {
        const float *point=((get_position)(uintptr_t)word(word(candidate)+0x18))((void *)(uintptr_t)candidate);
        uint32_t p=(uint32_t)(uintptr_t)point;
        /* The native loads and stores each coordinate in sequence. Overlapping
         * fallback buffers therefore affect subsequent coordinate reads. */
        store(frame+0xc4,word(p));store(frame+0xc8,word(p+4));store(frame+0xcc,word(p+8));
    }
}
__attribute__((naked)) void bfv_artillery_second_position_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_position_phase;"
        "add $4,%%esp; popal; jmp %c0;"
        : : "i"(BFV_ARTILLERY_SECOND_POSITION_CONTINUE) : "memory");
}
