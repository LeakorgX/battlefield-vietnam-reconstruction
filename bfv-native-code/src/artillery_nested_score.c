/* Alternate first-pass linked-object accumulation. Final score scaling stays
 * native. Frame offsets describe the observed evaluator, not inferred units. */
#include <stdint.h>
#include "target.h"
#include "list_search.h"
#include "target_traversal.h"
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
void bfv_artillery_nested_score_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],target=registers[0],base=registers[2];
    uint32_t bot=registers[1],descriptor=registers[4];
    do {
        uint32_t component=target+0x24?word(target+0x30):0;
        store(frame+0x38,component);
        uint32_t rating=word(component+8)+word(frame+0xd0)*4;
        /* Native captures this table and category before the rounded sum store;
         * either input can alias that destination. The method slot is read later. */
        uint32_t table=word(bot),category=*(volatile uint8_t *)(uintptr_t)(descriptor+4);
        __asm__ volatile("flds 92(%[f]); fadds (%[r]); fstps 92(%[f])"
            : : [f]"r"(frame),[r]"r"(rating) : "st","memory");
        uint32_t factor=BFV_MATH_ONE,use_default=1;
        if((uint8_t)((get_argument)(uintptr_t)word(table+0x70))((void *)(uintptr_t)bot,category)
                && (uint8_t)get(bot,0x6c)) {
            uint32_t list=get_category(bot,descriptor,0x4c);
            if(word(frame+0x44)<word(list+8)) {
                list=get_category(bot,descriptor,0x4c);store(frame+0x30,word(list+4));
                list=get_category(bot,descriptor,0x4c);store(frame+0xac,word(word(list+4)));
                list=get_category(bot,descriptor,0x4c);store(frame+0xbc,word(list+4));
                bfv_find_word_in_nodes((void *)(uintptr_t)(frame+0x15c),
                    (const uint32_t *)(uintptr_t)(frame+0x54),word(frame+0xac),word(frame+0x30));
                if(word(frame+0xbc)!=word(frame+0x15c)) use_default=0;
            }
        }
        if(!use_default) {
            uint32_t index=word(frame+0x44),table=word(bot);
            store(frame+0x44,index+1);
            factor=((get_word)(uintptr_t)word(table+0xe0))((void *)(uintptr_t)bot)+8;
        }
        uint32_t classification=word(frame+0x94);
        uint32_t class_rating=word(word(frame+0xd8)+8)+classification*4;
        rating=word(word(frame+0x38)+8)+word(frame+0xd0)*4;
        uint32_t weapon=word(frame+0xf0)+classification*4,argument=word(frame+0xa0);
        __asm__ volatile(
            "flds (%[factor]); flds (%[class]); fadds (%[rating]); fadds 20(%[t]);"
            "fmul %%st(1),%%st; fmuls 152(%[f]); fmuls (%[weapon]);"
            "fadds 20(%[f]); fstps 20(%[f]); fstp %%st(0)"
            : : [factor]"r"(factor),[class]"r"(class_rating),[rating]"r"(rating),
            [t]"r"(target),[f]"r"(frame),[weapon]"r"(weapon) : "st","memory");
        uint32_t handle=bfv_next_target_handle(
            (void *)(uintptr_t)base,(void *)(uintptr_t)(frame+0x70),argument);
        uint32_t pool=word(BFV_OBJECT_POOL);store(frame+0x54,handle);
        target=((get_argument)BFV_ARTILLERY_LOOKUP_TARGET)((void *)(uintptr_t)pool,handle);
        registers[0]=target;
    } while(target);
}
__attribute__((naked)) void bfv_artillery_nested_score_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_nested_score_phase;"
        "add $4,%%esp; popal; jmp %c0;"
        : : "i"(BFV_ARTILLERY_NESTED_FINISH) : "memory");
}
