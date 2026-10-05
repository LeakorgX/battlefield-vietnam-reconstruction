/* Second-pass category scorer.  The factor is a separately reconstructed stage at the
 * loop back edge; this body owns only the 420-byte scorer interval.  Floating
 * operations below intentionally mirror the original x87 instruction order. */
#include <stdint.h>
#include "target.h"

#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef uint32_t (TC *get_argument_word)(void *,uint32_t);

static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }
static uint32_t call_arg(uint32_t object,uint32_t slot,uint32_t argument)
{ return ((get_argument_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object,argument); }

static uint32_t resolve(uint32_t handle)
{
    uint32_t index=handle&0xffffu;
    if(!index) return 0;
    uint32_t record=word(word(BFV_OBJECT_POOL))+index*8-8;
    return *(volatile uint16_t *)(uintptr_t)(record+6)==(uint16_t)(handle>>16)
        ? word(record) : 0;
}

static void accumulate(uint32_t frame,uint32_t candidate)
{
    uint32_t category=word(frame+0x30);
    uint32_t class_rating=word(word(frame+0xd8)+8)+category*4;
    uint32_t component_rating=word(word(frame+0x3c)+8)+word(frame+0xd0)*4;
    uint32_t weapon=word(frame+0xf0)+category*4;
    __asm__ volatile(
        "flds (%[class]); fadds (%[component]); fadds 20(%[candidate]);"
        "fmuls (%[weapon]); fmul %%st(1),%%st; fmuls 152(%[frame]);"
        "fadds 20(%[frame]); fstps 20(%[frame]); fstp %%st(0)"
        : : [class]"r"(class_rating),[component]"r"(component_rating),
            [candidate]"r"(candidate),[weapon]"r"(weapon),[frame]"r"(frame)
        : "st","memory");
}

static void scale_score(uint32_t frame)
{
    /* This is the original stack-spill/callee sequence.  The two selector
     * bodies are inlined because their original entry points are detoured by
     * the shared scalar reconstruction in the edited image. */
    __asm__ volatile(
        "flds 32(%[f]); pushl %%ecx; fmuls %[category];"
        "fdivrs 40(%[f]); fstps (%%esp);"
        "pushl $0x3dcccccd;"
        "flds (%%esp); fcomps 4(%%esp); fnstsw %%ax; testb $5,%%ah;"
        "jp 1f; flds 4(%%esp); jmp 2f;"
        "1: flds (%%esp); 2: addl $8,%%esp;"
        "pushl %%ecx; fstps (%%esp); pushl $0x3f800000;"
        "flds (%%esp); fcomps 4(%%esp); fnstsw %%ax; testb $5,%%ah;"
        "jp 3f; flds (%%esp); jmp 4f;"
        "3: flds 4(%%esp); 4: addl $8,%%esp; fsubrs %[one];"
        "fmuls 52(%[f]); fmuls 64(%[f]); fmuls 20(%[f]);"
        "fmuls 44(%[f]); fmuls %[region]; fstps 20(%[f])"
        : : [f]"r"(frame),[category]"m"(*(volatile float *)BFV_ARTILLERY_CATEGORY_SCALE),
          [one]"m"(*(volatile float *)BFV_MATH_ONE),
          [region]"m"(*(volatile float *)BFV_ARTILLERY_REGION_SCALE)
        : "eax","st","memory");
}

static uint16_t compare_limit(uint32_t bot,uint32_t frame)
{
    uint32_t value=((get_word)(uintptr_t)BFV_ARTILLERY_SCORE_LIMIT)((void *)(uintptr_t)bot);
    uint16_t status;
    __asm__ volatile("fildl %[value]; fcomps 92(%[frame]); fnstsw %%ax"
        : "=a"(status) : [value]"m"(value),[frame]"r"(frame) : "st","memory");
    store(frame+0x70,value);
    return status;
}

static uint16_t compare_score(uint32_t frame,uint32_t offset)
{
    uint16_t status;
    __asm__ volatile("flds 20(%[f]); fcomps (%[other]); fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame),[other]"r"(frame+offset) : "st","memory");
    return status;
}

uint32_t bfv_artillery_second_category_score_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],candidate=registers[4],bot=registers[1];
    accumulate(frame,candidate);
    uint32_t receiver=word(word(frame+0x18)+0x20);
    uint32_t queried=call_arg(receiver,0x84,frame+0xec);
    uint32_t handle=queried?call_arg(queried,0x5c,word(frame+0xa0)):UINT32_MAX;
    store(frame+0x24,handle);
    uint32_t next=resolve(handle);
    store(frame+0x60,next);
    registers[4]=next;
    if(next) {
        uint32_t index=handle&0xffffu;
        uint32_t record=word(word(BFV_OBJECT_POOL))+index*8-8;
        registers[6]=record;
        registers[7]=handle>>16;
        return 1;
    }

    scale_score(frame);
    uint16_t status=compare_limit(bot,frame);
    registers[6]=bot;
    if(!__builtin_parity((status>>8)&5)) {
        uint16_t best_status=compare_score(frame,0x48);
        registers[7]=(word(frame+0x70)&0xffff0000u)|best_status;
        if(!(best_status&0x4100)) {
            uint32_t iterator=word(frame+0x1c);
            registers[7]=word(frame+0x74);
            registers[6]=iterator;
            store(frame+0x48,word(frame+0x14));
            store(frame+0xdc,word(iterator));
            store(frame+0x4c,word(frame+0x74));
            *(volatile uint8_t *)(uintptr_t)(frame+0x14c)=1;
        }
        return 0;
    }

    uint32_t iterator=word(frame+0x1c),record=word(iterator);
    registers[4]=record;
    if(record==word(frame+0x148)) {
        *(volatile uint8_t *)(uintptr_t)(frame+0x13)=1;
        store(frame+0x144,word(frame+0x14));
        registers[6]=word(frame+0x14);
    }
    uint16_t best_status=compare_score(frame,0x84);
    registers[7]=(iterator&0xffff0000u)|best_status;
    if(!(best_status&0x4100)) {
        store(frame+0x84,word(frame+0x14));
        uint32_t index=get(bot,0xdc),targets=word(registers[0]+8);
        registers[7]=index;
        registers[4]=record;
        registers[6]=targets;
        store(targets+index*4,record);
        store(frame+0xd4,word(frame+0x74));
        *(volatile uint8_t *)(uintptr_t)(frame+0x140)=1;
    }
    return 0;
}

__attribute__((naked)) void bfv_artillery_second_category_score_bridge(void)
{
    __asm__ volatile(
        "pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_category_score_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0;"
        "1: popal; jmp %c1"
        : : "i"(BFV_ARTILLERY_SECOND_CATEGORY_FACTOR),
            "i"(BFV_ARTILLERY_SECOND_CATEGORY_SCORE_CONTINUE) : "memory");
}
