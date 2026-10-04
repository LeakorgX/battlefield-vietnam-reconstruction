/* Second-pass weapon scan. This rating omits the first-pass history term. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint16_t compare(uint32_t left,uint32_t right)
{
    uint16_t status;
    __asm__ volatile("flds (%1); fcomps (%2); fnstsw %%ax"
        : "=a"(status) : "r"(left),"r"(right) : "st","memory");
    return status;
}
uint32_t bfv_artillery_second_weapons_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],owner=word(frame+0x50),owner_table=word(owner);
    store(frame+0x20,0);store(frame+0x74,UINT32_MAX);store(frame+0x40,0);
    uint32_t vector=((get_word)(uintptr_t)word(owner_table+0x1c))((void *)(uintptr_t)owner),index=0;
    registers[2]=vector;
    for(;;++index) {
        uint32_t begin=word(vector+4);
        if(!begin || index>=(uint32_t)((int32_t)(word(vector+8)-begin)>>2)) break;
        uint32_t weapon=word(begin+index*4),score=frame+0x160+index*4;
        if(!weapon) { store(score,0);continue; }
        uint32_t component=word(frame+0x94);
        uint32_t category=((get_word)(uintptr_t)word(word(component)+0x3c))((void *)(uintptr_t)component);
        uint32_t ratings=word(word(weapon+0x0c)+0x4c),table;
        /* Native captures the weapon table after FILD but before the rounded
         * frame store. The method pointer is read after that store. */
        __asm__ volatile("fildl (%[rating]); movl (%[weapon]),%[table]; fstps 112(%[f])"
            : [table]"=&r"(table) : [rating]"r"(ratings+category*4),
              [weapon]"r"(weapon),[f]"r"(frame) : "st","memory");
        uint32_t available=((get_word)(uintptr_t)word(table+0x24))((void *)(uintptr_t)weapon);
        store(frame+0x3c,available);
        if(available==UINT32_MAX) { available=0x10000;store(frame+0x3c,available); }
        if((int32_t)available>0) {
            __asm__ volatile("fildl 60(%[f]); fdivrs %[bias]; fadds %[one]; fdivrs 112(%[f]); fstps (%[out])"
                : : [f]"r"(frame),[out]"r"(score),
                  [bias]"m"(*(volatile float *)BFV_ARTILLERY_WEAPON_BIAS),
                  [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");
        } else store(score,0);
        if(!(compare(score,frame+0x40)&0x4100)) {
            uint32_t rating=word(score);store(frame+0x74,index);store(frame+0x40,rating);
        }
        uint32_t parameters=word(weapon+0x0c);
        if(!(compare(parameters+0x2c,frame+0x20)&0x4100))
            store(frame+0x20,word(parameters+0x2c));
    }
    registers[4]=index;
    uint16_t status;
    __asm__ volatile("flds %[zero]; flds 64(%[f]); fucompp; fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame),
          [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "st");
    status&=0x4400;
    if(status==0x0400||status==0x4000) return 0;
    /* REP STOSD pads accepted scores, but EBX retains the actual scan count. */
    if((int32_t)index<8) for(uint32_t padding=index;padding<8;++padding)
        store(frame+0x160+padding*4,0);
    return 1;
}
__attribute__((naked)) void bfv_artillery_second_weapons_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_weapons_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_SECOND_WEAPONS_ACCEPT),"i"(BFV_ARTILLERY_SECOND_REJECT) : "memory");
}
