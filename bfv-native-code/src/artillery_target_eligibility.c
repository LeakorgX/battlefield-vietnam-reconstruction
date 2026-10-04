/* Generation-handle traversal used by the alternate first-pass target path.
 * Object methods and the component-view conversion remain explicit dependencies. */
#include "artillery_target_eligibility.h"
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef uint32_t (TC *get_argument)(void *,uint32_t);
uint8_t TC bfv_artillery_component_test(void *component);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }
static uint32_t call(uint32_t object,uint32_t slot,uint32_t argument)
{ return ((get_argument)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object,argument); }
static uint32_t lookup(uint32_t handle)
{
    uint32_t index=handle&0xffffu;
    if(!index) return 0;
    uint32_t entry=word(word(BFV_OBJECT_POOL))+index*8-8;
    if(*(volatile uint16_t *)(uintptr_t)(entry+6)!=(uint16_t)(handle>>16)) return 0;
    return word(entry);
}
static uint8_t object_event3(uint32_t object)
{
    uint32_t component=word(object+0x30),owner=word(component+4),receiver=word(owner+0x20);
    uint32_t event=call(receiver,0xa0,3);
    return bfv_artillery_component_test((void *)(uintptr_t)event);
}
uint32_t bfv_target_handle_eligible_phase(uint32_t handle,uint32_t argument,uint32_t *scratch)
{
    uint32_t object=lookup(handle);
    if(!object) return 0;
    if(!(uint8_t)get(word(object+0x20),0x70)) {
        uint32_t owner=get(word(object+0x20),0x94),next=0xffffffffu;
        if(owner) {
            owner=get(word(object+0x20),0x94);
            next=call(owner,0x5c,argument);
        }
        object=lookup(next);
    }
    /* Native's seed branch returns false for a missing component. The later
     * linked-object loop instead continues through +0x84 in that case. */
    if(!(object+0x24u) || !word(object+0x30)) return 0;
    if(object_event3(object)) return 1;
    uint32_t owner=call(word(object+0x20),0x8c,(uint32_t)(uintptr_t)scratch);
    uint32_t next=owner?call(owner,0x5c,argument):0xffffffffu;
    uint32_t linked=lookup(next);
    if(!linked) return 0;
    for(;;) {
        if(linked+0x24u && word(linked+0x30) && object_event3(linked)) return 1;
        owner=call(word(object+0x20),0x84,(uint32_t)(uintptr_t)scratch);
        next=owner?call(owner,0x5c,argument):0xffffffffu;
        linked=lookup(next);
        if(!linked) return 0;
    }
}
uint8_t __attribute__((fastcall,naked)) bfv_target_handle_eligible(
    uint32_t handle __attribute__((unused)),uint32_t argument __attribute__((unused)))
{
    /* Native's local output word starts as ECX and is popped back into ECX.
     * Its callbacks can overwrite that word; preserve this unusual ABI. */
    __asm__ volatile("push %%ecx; pushal; lea 32(%%esp),%%eax;"
        "mov 20(%%esp),%%edx; mov 24(%%esp),%%ecx; push %%eax; push %%edx; push %%ecx;"
        "call _bfv_target_handle_eligible_phase; add $12,%%esp; mov %%al,28(%%esp);"
        "mov 32(%%esp),%%ecx; mov %%ecx,24(%%esp); popal; add $4,%%esp; ret"
        : : : "memory");
}
