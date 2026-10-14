/* Remaining short evaluator continuations. Keep distinct entries because the
 * heavy movement path and empty query enter inside the adjacent sequences. */
#include <stdint.h>
#include "target.h"
typedef void (__attribute__((cdecl)) *release)(void *);
void bfv_artillery_continuation_phase(uint32_t *registers,uint32_t operation)
{
    uint32_t frame=registers[3];
    if(operation==0)
        *(volatile uint32_t *)(uintptr_t)(frame+0x34)=0x3f800000u;
    else if(operation==1)
        registers[0]=*(volatile uint32_t *)(uintptr_t)(frame+0x1fc);
    else if(operation==2)
        registers[7]=*(volatile uint32_t *)(uintptr_t)(frame+0x100);
    else if(registers[7])
        ((release)(uintptr_t)BFV_VECTOR_FREE)((void *)(uintptr_t)registers[7]);
}
#define CONTINUATION(name,operation,next) \
__attribute__((naked)) void name(void) { \
    __asm__ volatile("pushal; mov %%esp,%%eax; push $%c0; push %%eax;" \
        "call _bfv_artillery_continuation_phase; add $8,%%esp; popal; jmp %c1" \
        : : "i"(operation),"i"(next) : "memory"); \
}
CONTINUATION(bfv_artillery_unit_score_bridge,0,BFV_ARTILLERY_DRIVER_RELOAD)
CONTINUATION(bfv_artillery_driver_reload_bridge,1,BFV_ARTILLERY_SECOND_DRIVER_GATE)
CONTINUATION(bfv_artillery_allocation_reload_bridge,2,BFV_ARTILLERY_SECOND_EMPTY)
CONTINUATION(bfv_artillery_query_cleanup_bridge,3,BFV_ARTILLERY_FINAL_WEIGHTS)
