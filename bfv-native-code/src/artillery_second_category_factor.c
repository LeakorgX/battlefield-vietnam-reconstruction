/* Re-enterable second-pass category factor. The retained scorer consumes the
 * factor through ST0, and native loop paths enter this bridge directly. */
#include <stdint.h>
#include "collision_events.h"
#include "list_search.h"
#include "target.h"

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

void bfv_artillery_second_category_factor_phase(uint32_t *registers)
{
    /* PUSHAD: EDI, ESI, EBP, incoming ESP, EBX, EDX, ECX, EAX. */
    uint32_t frame=registers[3],candidate=registers[4],bot=registers[1];
    uint32_t descriptor=registers[0];
    uint32_t component=word(candidate+0x30);
    store(frame+0x3c,component);
    uint32_t values=word(component+8),index=word(frame+0xd0);
    __asm__ volatile("flds 92(%[f]); fadds (%[v],%[i],4); fstps 92(%[f])"
        : : [f]"r"(frame),[v]"r"(values),[i]"r"(index) : "st","memory");
    uint32_t default_factor=1;
    if((uint8_t)get_category(bot,descriptor,0x70) && (uint8_t)get(bot,0x6c)) {
        uint32_t list=get_category(bot,descriptor,0x4c);
        if(word(frame+0x44)<word(list+8)) {
            uint32_t second=get_category(bot,descriptor,0x4c);
            uint32_t last=word(second+4);
            uint32_t third=get_category(bot,descriptor,0x4c);
            uint32_t first=word(word(third+4));
            uint32_t fourth=get_category(bot,descriptor,0x4c);
            store(frame+0x70,word(fourth+4));
            registers[2]=first;
            bfv_find_word_in_nodes((void *)(uintptr_t)(frame+0x15c),
                (const uint32_t *)(uintptr_t)(frame+0x24),first,last);
            if(word(frame+0x70)!=word(frame+0x15c)) {
                default_factor=0;
                store(frame+0x44,word(frame+0x44)+1);
                uint32_t factor=get(bot,0xe0);
                __asm__ volatile("flds 8(%0)" : : "r"(factor) : "st","memory");
            }
        }
    }
    registers[4]=word(frame+0x60);
    if(default_factor)
        __asm__ volatile("flds %0" : : "m"(*(volatile float *)BFV_MATH_ONE) : "st");
}

__attribute__((naked)) void bfv_artillery_second_category_factor_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_category_factor_phase;"
        "add $4,%%esp; popal; jmp %c0"
        : : "i"(BFV_ARTILLERY_SECOND_CATEGORY_FACTOR_CONTINUE) : "memory");
}
