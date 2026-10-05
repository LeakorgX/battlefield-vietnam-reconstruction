/* Entire final-selection tail of the artillery target evaluator.
 * The preceding weight bridge has already popped the saved EBP. Object
 * methods and the score-curve service remain external dependencies. This is
 * source for all three native return paths, not a complete evaluator yet. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef uint32_t (TC *get_argument)(void *,uint32_t);
typedef void (TC *select_target)(void *,void *,uint32_t,uint32_t,uint32_t,uint32_t);
typedef long double (__attribute__((stdcall)) *score_curve)(float);

static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static uint8_t byte(uint32_t p) { return *(volatile uint8_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint32_t call_table(uint32_t object,uint32_t table,uint32_t slot)
{ return ((get_word)(uintptr_t)word(table+slot))((void *)(uintptr_t)object); }
static uint32_t get(uint32_t object,uint32_t slot)
{ return call_table(object,word(object),slot); }
static uint32_t argument_table(uint32_t object,uint32_t table,uint32_t slot,uint32_t arg)
{ return ((get_argument)(uintptr_t)word(table+slot))((void *)(uintptr_t)object,arg); }
static uint32_t argument(uint32_t object,uint32_t slot,uint32_t arg)
{ return argument_table(object,word(object),slot,arg); }

static uint16_t positive(uint32_t address)
{
    uint16_t status;
    __asm__ volatile("flds (%[value]); fcomps %[zero]; fnstsw %%ax"
        : "=a"(status) : [value]"r"(address),
        [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "st","memory");
    return !(status&0x4100);
}
static uint16_t prefer_remembered(uint32_t frame)
{
    uint16_t status;
    __asm__ volatile("flds 320(%[f]); fmuls %[bias]; fcomps 128(%[f]); fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame),
        [bias]"m"(*(volatile float *)BFV_ARTILLERY_FINAL_SELECTION_BIAS) : "st","memory");
    return !(status&0x4100);
}
static void select(uint32_t output,uint32_t bot,uint32_t weapon,uint32_t event_flag)
{
    uint32_t table=word(output);
    ((select_target)(uintptr_t)word(table+0x34))((void *)(uintptr_t)output,
        (void *)(uintptr_t)bot,weapon,0,0,event_flag);
}
static uint32_t has_empty_mode(uint32_t bot,uint32_t output)
{
    uint32_t mode=byte(output+4);
    if(!(uint8_t)argument(bot,0x70,mode)) return 0;
    if(!(uint8_t)get(bot,0x6c)) return 0;
    uint32_t data=argument(bot,0x4c,byte(output+4));
    return word(data+8)==0;
}
static void multiply_field(uint32_t destination,uint32_t factor)
{
    __asm__ volatile("flds (%[dst]); fmuls (%[factor]); fstps (%[dst])"
        : : [dst]"r"(destination),[factor]"r"(factor) : "st","memory");
}
static void copy_float(uint32_t destination,uint32_t source)
{
    __asm__ volatile("flds (%[src]); fstps (%[dst])"
        : : [dst]"r"(destination),[src]"r"(source) : "st","memory");
}
static uint32_t curve_weight(uint32_t frame,uint32_t bot,uint32_t score_offset,uint32_t multiplier)
{
    float doubled;
    __asm__ volatile("flds (%[score]); fadd %%st,%%st; fstps %[value]"
        : [value]"=m"(doubled) : [score]"r"(frame+score_offset) : "st","memory");
    long double weight=((score_curve)(uintptr_t)BFV_ARTILLERY_FINAL_CURVE)(doubled);
    __asm__ volatile("fmuls 40(%[f])" : : "t"(weight),[f]"r"(frame) : "st","memory");
    uint32_t table=word(bot);
    __asm__ volatile("fmuls (%[factor]); fstps 164(%[f])"
        : : [factor]"r"(frame+multiplier),[f]"r"(frame) : "st","memory");
    return table;
}
static void publish_weight(uint32_t frame,uint32_t bot,uint32_t output,uint32_t table)
{
    uint32_t index=call_table(bot,table,0xdc),ratings=word(output+0x1c);
    copy_float(ratings+index*4,frame+0xa4);
    table=word(bot);index=call_table(bot,table,0xdc);
    ratings=word(output+0x1c);
    uint32_t value=word(ratings+index*4);
    /* Preserve the table captured before the second index callback. */
    argument_table(bot,table,0x160,value);
}
static void return_rating(uint32_t bot,uint32_t output)
{
    uint32_t index=get(bot,0xdc),ratings=word(output+0x1c);
    __asm__ volatile("flds (%[value])" : : [value]"r"(ratings+index*4) : "st","memory");
}

void bfv_artillery_final_selection_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],bot=registers[1],output,index,table;
    uint32_t best_positive=positive(frame+0x80);
    uint8_t remembered=byte(frame+0x0f);
    if(best_positive) {
        uint32_t weapon;
        if(remembered && prefer_remembered(frame)) {
            index=get(bot,0xdc);output=word(frame+0xa0);
            uint32_t targets=word(output+8),target=word(frame+0x144);
            store(targets+index*4,target);
            index=get(bot,0xdc);
            uint32_t weapons=word(output+0x30),previous_weapon=word(frame+0xa4);
            store(weapons+index*4,previous_weapon);
            weapon=word(frame+0xd0);
        } else {
            get(bot,0xdc);index=get(bot,0xdc);output=word(frame+0xa0);
            weapon=word(word(output+0x30)+index*4);
            *(volatile uint8_t *)(uintptr_t)(frame+0x0f)=0;
        }
        uint32_t event_flag=word(frame+0x13c);
        select(output,bot,weapon,event_flag);
        if(has_empty_mode(bot,output)) {
            uint32_t factor=get(bot,0xe0);
            multiply_field(frame+0x80,factor+8);
        }
        uint8_t new_selection=byte(frame+0x0f)==0;
        index=get(bot,0xdc);
        uint32_t states=word(output+0x24);
        *(volatile uint8_t *)(uintptr_t)(states+index)=new_selection;
        table=curve_weight(frame,bot,0x80,0x1f0);
        publish_weight(frame,bot,output,table);
        return_rating(bot,output);
        return;
    }

    uint8_t new_selection=remembered==0;
    index=get(bot,0xdc);
    /* The comparison precedes the state store; the receiver is captured in
     * between the FLD and FCOMP in the original, with no intervening callback. */
    uint32_t alternate_positive=positive(frame+0x44);
    output=word(frame+0xa0);
    uint32_t states=word(output+0x24);
    *(volatile uint8_t *)(uintptr_t)(states+index)=new_selection;
    table=word(bot);
    if(alternate_positive) {
        index=call_table(bot,table,0xdc);
        uint32_t targets=word(output+8),target=word(frame+0xd8);
        store(targets+index*4,target);
        uint32_t event_flag=word(frame+0x148),weapon=word(frame+0x48);
        select(output,bot,weapon,event_flag);
        table=curve_weight(frame,bot,0x44,0x44);
        publish_weight(frame,bot,output,table);
        if(has_empty_mode(bot,output)) {
            index=get(bot,0xdc);
            uint32_t ratings=word(output+0x1c),destination=ratings+index*4;
            uint32_t factor=get(bot,0xe0);
            /* This path loads the multiplier first, unlike the best path. */
            __asm__ volatile("flds 8(%[factor]); fmuls (%[dst]); fstps (%[dst])"
                : : [factor]"r"(factor),[dst]"r"(destination) : "st","memory");
        }
        return_rating(bot,output);
        return;
    }

    argument_table(bot,table,0x78,UINT32_MAX);
    uint32_t collection=word(output+0x0c);
    argument(collection,8,bot);
    index=get(bot,0xdc);
    uint32_t ratings=word(output+0x1c);store(ratings+index*4,0);
    index=get(bot,0xdc);
    __asm__ volatile("flds %[zero]" : : [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "st","memory");
    uint32_t targets=word(output+8);store(targets+index*4,UINT32_MAX);
}

__attribute__((naked)) void bfv_artillery_final_selection_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax;"
        "call _bfv_artillery_final_selection_phase; add $4,%%esp; popal;"
        "pop %%edi; pop %%esi; pop %%ebx; add $0x1d8,%%esp; ret $0x14"
        : : : "memory");
}
