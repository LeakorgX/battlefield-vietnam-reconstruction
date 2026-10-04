/* First-pass candidate weight and aiming gate, following weapon selection.
 * The wider evaluator remains native while its other phases are recovered. */
#include <stdint.h>
#include "aim_geometry.h"
#include "target.h"
typedef void *(BFV_AIM_TC *get_object)(void *);
typedef uint8_t (BFV_AIM_TC *aim_direction)(void *,const float *);
static uint32_t word(uintptr_t p) { return *(volatile uint32_t *)p; }
static void store(uintptr_t p,uint32_t v) { *(volatile uint32_t *)p=v; }
static void *get(uintptr_t object,uint32_t slot)
{ return ((get_object)(uintptr_t)word(word(object)+slot))((void *)object); }

uint32_t bfv_artillery_aim_gate_phase(uint32_t *registers)
{
    uintptr_t frame=registers[3],record=word(frame+0x18);
    if (*(volatile uint8_t *)(record+0x14)) {
        /* weight = 1 / (1 + (reference - record_value) * 0.05).
         * The native code applies no clamp; units remain unestablished. */
        __asm__ volatile(
            "flds 88(%[f]); fsubs 24(%[r]); fmuls %[scale]; fadds %[one];"
            "fdivrs %[one]; fstps 116(%[f])"
            : : [f]"r"(frame),[r]"r"(record),
            [scale]"m"(*(volatile float *)BFV_ARTILLERY_WEIGHT_SCALE),
            [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");
        return 1;
    }
    uintptr_t driver=word(frame+0x1fc);
    store(frame+0x74,0x3f800000);
    if (driver) return 1;
    void *component=(void *)(uintptr_t)word(frame+0xf4);
    bfv_component_position(component,(float *)(frame+0x1dc));
    /* The two zero-argument getters leave already-pushed transform arguments
     * alone in the binary. They are separate typed calls here. */
    const float *point=(const float *)(uintptr_t)(word(frame+0x18)+4);
    const float *matrix=get(registers[2],0x24);
    bfv_transform_point(matrix,point,(float *)(frame+0x1b0));
    const float *origin=get(word(frame+0xec),0x38);
    const float *direction=bfv_vector_difference(origin,(float *)(frame+0x1c4),(const float *)(frame+0x1b0));
    store(frame+0x1a4,word((uintptr_t)direction));
    store(frame+0x1a8,word((uintptr_t)direction+4));
    store(frame+0x1ac,word((uintptr_t)direction+8));
    return ((aim_direction)BFV_AIM_DIRECTION)(component,(const float *)(frame+0x1a4));
}

__attribute__((naked)) void bfv_artillery_aim_gate_bridge(void)
{
    __asm__ volatile(
        "pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_aim_gate_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0;"
        "1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_AIM_GATE_ACCEPT),"i"(BFV_ARTILLERY_FILTER_REJECT) : "memory");
}
