/* Second-pass direction construction and aiming gate; later scoring is native. */
#include <stdint.h>
#include "aim_geometry.h"
#include "target.h"
typedef const float *(BFV_AIM_TC *position)(void *);
typedef uint32_t (BFV_AIM_TC *aim_direction)(void *,const float *);
uint32_t BFV_AIM_TC bfv_vector_normalize(float *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
uint32_t bfv_artillery_second_aim_phase(uint32_t *registers)
{
    uint32_t frame=registers[3];
    if(word(frame+0x1fc)) return 1;
    uint32_t component=word(frame+0xf4);registers[0]=component;
    bfv_component_position((void *)(uintptr_t)component,(float *)(uintptr_t)(frame+0x18c));
    uint32_t candidate=word(frame+0x18);
    const float *point=((position)(uintptr_t)word(word(candidate)+0x18))((void *)(uintptr_t)candidate);
    const float *direction=bfv_vector_difference((const float *)(uintptr_t)(frame+0x18c),
        (float *)(uintptr_t)(frame+0x1c4),point);
    store(frame+0xe0,word((uint32_t)(uintptr_t)direction));
    store(frame+0xe4,word((uint32_t)(uintptr_t)direction+4));
    store(frame+0xe8,word((uint32_t)(uintptr_t)direction+8));
    bfv_vector_normalize((float *)(uintptr_t)(frame+0xe0));
    /* The component is captured before position callbacks; the frame may change. */
    return (uint8_t)((aim_direction)BFV_AIM_DIRECTION)((void *)(uintptr_t)component,
        (const float *)(uintptr_t)(frame+0xe0));
}
__attribute__((naked)) void bfv_artillery_second_aim_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_aim_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_SECOND_AIM_ACCEPT),"i"(BFV_ARTILLERY_SECOND_REJECT) : "memory");
}
