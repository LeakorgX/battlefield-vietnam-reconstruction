/* Post-query artillery score modifiers and the second-pass iterator.
 * The parameter ids are recovered numbers; their gameplay names are not yet
 * established. Object index callbacks and allocation cleanup remain services.
 * Final weighting also consumes the evaluator's saved EBP stack word. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_index)(void *);

static uint32_t word(uint32_t address)
{ return *(volatile uint32_t *)(uintptr_t)address; }

static uint32_t parameter_row(uint32_t manager,uint32_t index)
{ return word(word(manager+4)+index*4); }

static uint32_t index_of(uint32_t bot,uint32_t table)
{
    return ((get_index)(uintptr_t)word(table+0xdc))((void *)(uintptr_t)bot);
}

uint32_t bfv_artillery_second_iterator_phase(uint32_t *registers)
{
    uint32_t frame=registers[3];
    uint32_t next=word(frame+0x1c)+4u,end=word(frame+0x104);
    *(volatile uint32_t *)(uintptr_t)(frame+0x1c)=next;
    registers[7]=next;
    registers[6]=end;
    return next!=end;
}

void bfv_artillery_final_weights_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],bot=registers[1];
    /* Capture the global receiver BEFORE each index callback. The callback can
     * change the global, row pointers, bot table and the score spill itself. */
    uint32_t table=word(bot),manager=word(BFV_ARTILLERY_PARAMETER_MANAGER);
    uint32_t index=index_of(bot,table),row=parameter_row(manager,index);
    __asm__ volatile("flds 332(%[row]); fadds %[one]"
        : : [row]"r"(row),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");

    table=word(bot);
    manager=word(BFV_ARTILLERY_PARAMETER_MANAGER);
    __asm__ volatile("fstps 44(%[frame])" : : [frame]"r"(frame) : "st","memory");
    index=index_of(bot,table);row=parameter_row(manager,index);
    __asm__ volatile("flds 348(%[row]); fadds %[one]"
        : : [row]"r"(row),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");

    table=word(bot);
    manager=word(BFV_ARTILLERY_PARAMETER_MANAGER);
    __asm__ volatile("fmuls 44(%[frame]); fstps 44(%[frame])"
        : : [frame]"r"(frame) : "st","memory");
    index=index_of(bot,table);row=parameter_row(manager,index);
    __asm__ volatile("flds 336(%[row]); fsubrs %[one];"
        "fmuls 44(%[frame]); fstps 44(%[frame])"
        : : [row]"r"(row),[frame]"r"(frame),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");

    /* The original parameter getter leaves these caller-scratch values.
     * The bridge reproduces its integer state and the following POP EBP. */
    registers[0]=manager;
    registers[5]=row;
    registers[6]=index;
    registers[7]=0x54;
}

__attribute__((naked)) void bfv_artillery_second_iterator_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax;"
        "call _bfv_artillery_second_iterator_phase; add $4,%%esp;"
        "test %%eax,%%eax; jz 1f; popal; jmp %c0;"
        "1: popal; jmp %c1"
        : : "i"(BFV_ARTILLERY_SECOND_FILTER),
            "i"(BFV_ARTILLERY_SECOND_ITERATOR_DONE) : "memory");
}

__attribute__((naked)) void bfv_artillery_final_weights_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax;"
        "call _bfv_artillery_final_weights_phase; add $4,%%esp;"
        "popal; pop %%ebp; jmp %c0"
        : : "i"(BFV_ARTILLERY_FINAL_WEIGHTS_CONTINUE) : "memory");
}
