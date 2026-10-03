/* First-pass artillery weapon rating and selection. The surrounding candidate
 * evaluator still owns the frame until its other phases are reconstructed. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);

enum {
    RECORD=0x18, MAX_PARAMETER=0x1c, DISTANCE=0x28, BEST_SCORE=0x34,
    HISTORY_CURSOR=0x38, WEAPON_OWNER=0x50, AVAILABLE_COUNT=0x54,
    BEST_INDEX=0x60, TARGET_COMPONENT=0x94, BEHAVIOR=0xa4,
    WEAPON_VECTOR=0xac, CLASS_RATING=0xbc, BEST_WEAPON=0xec, SCORES=0x160
};
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }

/* Preserve the native float32 stores and extended intermediates. Using normal
 * C comparisons here would change unordered-value decisions and x87 flags. */
static void class_rating(uint32_t source,uint32_t destination)
{
    __asm__ volatile("fildl (%0); fstps (%1)" : : "r"(source),"r"(destination) : "st","memory");
}
static void rate(uint32_t cursor,uint32_t frame,uint32_t destination)
{
    /* score = class_rating / (1 + ((history_a-history_b)*20 + 10)/count).
     * The two record arrays' semantic names have not yet been established. */
    __asm__ volatile(
        "flds -32(%[cursor]); fsubs (%[cursor]); fmuls %[scale]; fadds %[bias];"
        "fidivl 84(%[frame]); fadds %[one]; fdivrs 188(%[frame]); fstps (%[out])"
        : : [cursor]"r"(cursor),[frame]"r"(frame),[out]"r"(destination),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_HISTORY_LIMIT),
        [bias]"m"(*(volatile float *)BFV_ARTILLERY_WEAPON_BIAS),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");
}
static uint16_t compare(uint32_t left,uint32_t right)
{
    uint16_t status;
    __asm__ volatile("flds (%1); fcomps (%2); fnstsw %%ax"
        : "=a"(status) : "r"(left),"r"(right) : "st","memory");
    return status;
}
static uint16_t compare_best_to_zero(uint32_t best)
{
    uint16_t status;
    __asm__ volatile("flds %[zero]; flds (%[best]); fucompp; fnstsw %%ax"
        : "=a"(status) : [best]"r"(best),
        [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "st","memory");
    return status;
}

/* PUSHAD packet: EDI, ESI, EBP, incoming ESP, EBX, EDX, ECX, EAX.
 * EBX and frame fields remain live. Both continuations overwrite EDI and the
 * volatile integer registers before using them; ESI and EBP are preserved. */
uint32_t bfv_artillery_weapons_phase(uint32_t *registers)
{
    uint32_t frame=registers[3], owner=word(frame+WEAPON_OWNER), owner_table=word(owner);
    store(frame+MAX_PARAMETER,0);
    store(frame+BEST_INDEX,UINT32_MAX);
    store(frame+BEST_SCORE,0);
    uint32_t vector=((get_word)(uintptr_t)word(owner_table+0x1c))((void *)(uintptr_t)owner), index=0;
    uint32_t cursor=word(frame+RECORD)+0x54;
    store(frame+WEAPON_VECTOR,vector);
    store(frame+HISTORY_CURSOR,cursor);
    for (;;) {
        uint32_t begin=word(vector+4);
        if (!begin) break;
        uint32_t count=(uint32_t)((int32_t)(word(vector+8)-begin)>>2);
        if (index>=count) break;
        uint32_t weapon=word(begin+index*4), score=frame+SCORES+index*4;
        if (weapon) {
            uint32_t category=get(word(frame+TARGET_COMPONENT),0x3c);
            uint32_t table=word(word(weapon+0x0c)+0x4c);
            class_rating(table+category*4,frame+CLASS_RATING);
            uint32_t available=get(weapon,0x24);
            store(frame+AVAILABLE_COUNT,available);
            if (available==UINT32_MAX) {
                available=0x10000;
                store(frame+AVAILABLE_COUNT,available);
            }
            if ((int32_t)available>0) rate(word(frame+HISTORY_CURSOR),frame,score);
            else store(score,0);
            uint32_t parameters=word(weapon+0x0c);
            uint16_t status=compare(frame+DISTANCE,parameters+0x28)&0x0500;
            if (status==0x0100 || status==0x0400) store(score,0);
            if (!(compare(score,frame+BEST_SCORE)&0x4100)) {
                uint32_t bits=word(score);
                store(frame+BEST_INDEX,index);
                store(frame+BEST_SCORE,bits);
                store(frame+BEST_WEAPON,weapon);
            }
            if (!(compare(parameters+0x2c,frame+MAX_PARAMETER)&0x4100))
                store(frame+MAX_PARAMETER,word(parameters+0x2c));
        } else store(score,0);
        cursor=word(frame+HISTORY_CURSOR);
        registers[4]=word(frame+BEHAVIOR);
        ++index;
        store(frame+HISTORY_CURSOR,cursor+4);
        vector=word(frame+WEAPON_VECTOR);
    }
    uint16_t status=compare_best_to_zero(frame+BEST_SCORE)&0x4400;
    if (status==0x0400 || status==0x4000) return 0;
    /* The original pads only accepted candidates, and does not clamp the scan
     * to eight weapons. It relies on the engine's own container constraints. */
    if ((int32_t)index<8)
        for (;index<8;++index) store(frame+SCORES+index*4,0);
    return 1;
}

__attribute__((naked)) void bfv_artillery_weapons_bridge(void)
{
    __asm__ volatile(
        "pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_weapons_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0;"
        "1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_WEAPONS_ACCEPT),"i"(BFV_ARTILLERY_FILTER_REJECT) : "memory");
}
