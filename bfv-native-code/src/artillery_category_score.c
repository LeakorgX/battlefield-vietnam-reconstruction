/* First-pass category-list factor and best-candidate score. The alternate
 * target-flag path and remaining evaluator phases stay native. */
#include <stdint.h>
#include "target.h"
#include "collision_events.h"
#include "list_search.h"
#include "scalar_vector_math.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef uint32_t (TC *get_argument)(void *,uint32_t);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }
static uint32_t get_category(uint32_t bot,uint32_t descriptor,uint32_t slot)
{
    uint32_t table=word(bot),category=*(volatile uint8_t *)(uintptr_t)(descriptor+4);
    return ((get_argument)(uintptr_t)word(table+slot))((void *)(uintptr_t)bot,category);
}
static uint16_t score(uint32_t frame,uint32_t target)
{
    float ratio;
    __asm__ volatile("flds 40(%[f]); fdivs 28(%[f]); fmuls %[scale]; fstps %[r]"
        : [r]"=m"(ratio) : [f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_CATEGORY_SCALE) : "st","memory");
    /* Each selector takes a rounded float32 argument but returns native ST0.
     * Preserve FSUBR operand order before the candidate factors are loaded. */
    float lower=(float)bfv_float_maximum(0.1f,ratio);
    long double attenuation=bfv_float_minimum(1.0f,lower);
    __asm__ volatile("fsubrs %[one]" : "=t"(attenuation) : "0"(attenuation),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "memory");
    uint32_t index=word(frame+0xbc);
    uint32_t rating=word(word(frame+0xd8)+8)+index*4;
    uint32_t weapon=word(frame+0xf0)+index*4;
    uint16_t status;
    __asm__ volatile(
        "flds (%[r]); fmuls (%[w]); fmuls 20(%[t]); fmuls 60(%[f]);"
        "fmuls 152(%[f]); fadds 20(%[f]); fmulp; fmuls 32(%[f]);"
        "fmuls 116(%[f]); fmuls 52(%[f]); fmuls 44(%[f]);"
        "fsts 20(%[f]); fcomps 72(%[f]); fnstsw %%ax"
        : "=a"(status) : "t"(attenuation),[r]"r"(rating),[w]"r"(weapon),
        [t]"r"(target),[f]"r"(frame) : "st","memory");
    return status;
}
uint32_t bfv_artillery_category_score_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],target=registers[2],bot=registers[1],descriptor=registers[4];
    if(!(*(volatile uint8_t *)(uintptr_t)(target+7)&1)) return 0;
    uint32_t interface=bfv_event_interface((void *)(uintptr_t)target,0);
    store(frame+0xbc,get(interface,0x3c));
    uint32_t use_default=1;
    if((uint8_t)get_category(bot,descriptor,0x70) && (uint8_t)get(bot,0x6c)) {
        uint32_t list=get_category(bot,descriptor,0x4c);
        if(word(frame+0x44)<word(list+8)) {
            list=get_category(bot,descriptor,0x4c);
            uint32_t last=word(list+4);registers[0]=last;
            list=get_category(bot,descriptor,0x4c);
            store(frame+0x30,word(word(list+4)));
            list=get_category(bot,descriptor,0x4c);
            store(frame+0xac,word(list+4));
            bfv_find_word_in_nodes((void *)(uintptr_t)(frame+0x1c0),
                (const uint32_t *)(uintptr_t)word(frame+0x18),word(frame+0x30),last);
            if(word(frame+0xac)!=word(frame+0x1c0)) use_default=0;
        }
    }
    if(use_default) store(frame+0x3c,0x3f800000);
    else {
        uint32_t index=word(frame+0x44),table=word(bot);
        store(frame+0x44,index+1);
        uint32_t value=((get_word)(uintptr_t)word(table+0xe0))((void *)(uintptr_t)bot);
        store(frame+0x3c,word(value+8));
    }
    if(!(score(frame,target)&0x4100)) {
        uint32_t record=word(frame+0x40),rounded=word(frame+0x14);
        uint32_t data=word(record+8),index=word(frame+0x60);
        store(frame+0x48,rounded);store(frame+0xdc,word(data));store(frame+0x4c,index);
    }
    return 1;
}
__attribute__((naked)) void bfv_artillery_category_score_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_category_score_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_FILTER_REJECT),"i"(BFV_ARTILLERY_CATEGORY_ALTERNATE) : "memory");
}
