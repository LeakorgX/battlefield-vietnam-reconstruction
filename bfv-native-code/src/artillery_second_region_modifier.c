/* Second-pass region modifier. The surrounding candidate iterator remains
 * native; this interval only refreshes the score and applies the region scale. */
#include <stdint.h>
#include "region_geometry.h"
#include "target.h"
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
void bfv_artillery_second_region_modifier_phase(uint32_t *registers)
{
    uint32_t frame=registers[3];
    registers[4]=word(frame+0x18); /* native EBX capture */
    *(volatile uint32_t *)(uintptr_t)(frame+0x98)=word(frame+0x9c);
    if(registers[0]) {
        uint32_t region=word(frame+0x134);
        if(region && !bfv_region_contains_point3((void *)(uintptr_t)region,
                         (const void *)(uintptr_t)(frame+0xc4))) {
            __asm__ volatile("flds 156(%[f]); fmuls %[scale]; fstps 152(%[f])"
                : : [f]"r"(frame),
                [scale]"m"(*(volatile float *)BFV_ARTILLERY_REGION_SCALE)
                : "st","memory");
        }
    }
}
__attribute__((naked)) void bfv_artillery_second_region_modifier_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_region_modifier_phase;"
        "add $4,%%esp; popal; jmp %c0"
        : : "i"(BFV_ARTILLERY_SECOND_REGION_CONTINUE) : "memory");
}
