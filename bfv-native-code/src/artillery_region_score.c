/* Following first-pass region score modifier; later flag/category phases are
 * still native. The supplied frame remains owned by the wider evaluator. */
#include <stdint.h>
#include "target.h"
#include "region_geometry.h"
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
void bfv_artillery_region_score_phase(uint32_t *registers)
{
    uint32_t frame=registers[3];
    *(volatile uint32_t *)(uintptr_t)(frame+0x98)=word(frame+0x9c);
    if(word(frame+0x1fc)) {
        uint32_t region=word(frame+0x134);
        if(region && !bfv_region_contains_point3((void *)(uintptr_t)region,
                                               (const void *)(uintptr_t)(frame+0xc4))) {
            /* Reread the source score after the predicate, preserving the
             * native rounded store and signaling/nonfinite behavior. */
            __asm__ volatile("flds 156(%[f]); fmuls %[scale]; fstps 152(%[f])"
                : : [f]"r"(frame),[scale]"m"(*(volatile float *)BFV_ARTILLERY_REGION_SCALE)
                : "st","memory");
        }
    }
}
__attribute__((naked)) void bfv_artillery_region_score_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_region_score_phase;"
        "add $4,%%esp; popal; jmp %c0" : : "i"(BFV_ARTILLERY_REGION_CONTINUE) : "memory");
}
