/* Second-pass category/target-state gate immediately after the region score.
 * The surrounding evaluator owns the frame; this interval only tests the
 * captured target flags and reuses the original generation-aware eligibility
 * helper before entering the category scan. */
#include <stdint.h>
#include "target.h"

/* The caller tests AL but carries the full EAX value into the continuation. */
typedef uint32_t (__attribute__((fastcall)) *eligible)(uint32_t,uint32_t);

static uint32_t word(uint32_t p)
{
    return *(volatile uint32_t *)(uintptr_t)p;
}

uint32_t bfv_artillery_second_category_gate_phase(uint32_t *registers)
{
    /* pushal layout: EDI, ESI, EBP, original ESP, EBX, EDX, ECX, EAX. */
    uint32_t frame=registers[3],target=registers[4];
    uint32_t shifted=word(target+0x10)>>3;
    if(!(shifted&1u)) {
        /* The native reject branch retains the shifted flag value in EAX. */
        registers[7]=shifted;
        return 0;
    }
    uint32_t descriptor=word(frame+0x1c);
    uint32_t argument=word(frame+0xa0);
    uint32_t object=word(descriptor);
    uint32_t result=((eligible)(uintptr_t)BFV_TARGET_HANDLE_ELIGIBLE)(object,argument);
    /* The retained continuation consumes EDI.  The original also reaches it
     * after placing the fastcall arguments in ECX/EDX and leaving EAX as the
     * helper's unnormalised result.  Write those caller-saved outcomes into
     * PUSHAD's packet before the bridge restores it. */
    registers[0]=descriptor;
    registers[5]=argument;
    registers[6]=object;
    registers[7]=result;
    return result;
}

__attribute__((naked)) void bfv_artillery_second_category_gate_bridge(void)
{
    __asm__ volatile(
        "pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_category_gate_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0;"
        "1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_SECOND_CATEGORY_ACCEPT),
            "i"(BFV_ARTILLERY_SECOND_CATEGORY_REJECT) : "memory");
}
