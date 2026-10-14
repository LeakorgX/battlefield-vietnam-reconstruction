/* Fresh-search prefix of the target evaluator, after its native prologue.
 * The bridge saves EBP before entry to C; early_frame reproduces the native
 * pre-EBP layout, frame reproduces the layout used by following scoring stages.
 * Allocation and tree erasure remain services. Tree advance, handle resolution
 * and region-array bounds/lookup are implemented here from instruction evidence. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef uint32_t (TC *get_argument)(void *,uint32_t);
typedef long double (TC *get_time)(void *);
typedef uint32_t (TC *erase_node)(void *,void *,uint32_t);
typedef void (__attribute__((cdecl)) *release)(void *);

static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint8_t byte(uint32_t p) { return *(volatile uint8_t *)(uintptr_t)p; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }
static uint32_t argument_table(uint32_t object,uint32_t table,uint32_t slot,uint32_t arg)
{ return ((get_argument)(uintptr_t)word(table+slot))((void *)(uintptr_t)object,arg); }
static uint32_t argument(uint32_t object,uint32_t slot,uint32_t arg)
{ return argument_table(object,word(object),slot,arg); }
static uint32_t resolve(uint32_t handle)
{
    uint32_t index=handle&0xffffu;
    if(!index) return 0;
    uint32_t record=word(word(BFV_OBJECT_POOL))+index*8-8;
    if(*(volatile uint16_t *)(uintptr_t)(record+6)!=(uint16_t)(handle>>16)) return 0;
    return word(record);
}
static void advance_tree(uint32_t cell)
{
    uint32_t node=word(cell);
    if(byte(node+0x15)) return;
    uint32_t right=word(node+8);
    if(!byte(right+0x15)) {
        uint32_t left=word(right);
        while(!byte(left+0x15)) { right=left;left=word(right); }
        store(cell,right);
        return;
    }
    uint32_t parent=word(node+4);
    while(!byte(parent+0x15) && word(cell)==word(parent+8)) {
        store(cell,parent);parent=word(parent+4);
    }
    store(cell,parent);
}
static uint32_t region_at(uint32_t index)
{
    uint32_t registry=word(BFV_ARTILLERY_PREFIX_REGION_REGISTRY);
    if((int32_t)index<0) return 0;
    uint32_t begin=word(registry+8);
    if(!begin) return 0;
    uint32_t count=(uint32_t)((int32_t)(word(registry+0x0c)-begin)>>2);
    return index<count?word(begin+index*4):0;
}
static uint32_t refresh_needed(uint32_t early_frame,uint32_t behavior,uint32_t descriptor)
{
    uint16_t status;
    __asm__ volatile("flds 84(%[f]); fsubs 52(%[b])"
        : : [f]"r"(early_frame),[b]"r"(behavior) : "st","memory");
    store(early_frame+0xec,descriptor);
    __asm__ volatile("fcomps %[threshold]; fnstsw %%ax"
        : "=a"(status) : [threshold]"m"(*(volatile float *)BFV_ARTILLERY_PREFIX_REFRESH_THRESHOLD)
        : "st","memory");
    return !(status&0x4100);
}
static void refresh_history(uint32_t early_frame,uint32_t behavior)
{
    uint32_t sentinel=word(behavior+0x3c),node=word(sentinel);
    uint32_t empty=node==sentinel;
    store(early_frame+0x48,node);
    if(!empty) do {
        if(!resolve(word(node+0x0c))) {
            uint32_t allocation=word(node+0x10);
            ((release)(uintptr_t)BFV_VECTOR_FREE)((void *)(uintptr_t)allocation);
            uint32_t result=((erase_node)(uintptr_t)BFV_ARTILLERY_PREFIX_TREE_ERASE)
                ((void *)(uintptr_t)(behavior+0x38),(void *)(uintptr_t)(early_frame+0xa4),node);
            node=word(result);store(early_frame+0x48,node);
        } else {
            advance_tree(early_frame+0x48);node=word(early_frame+0x48);
        }
    } while(node!=word(behavior+0x3c));
    store(behavior+0x34,word(early_frame+0x54));
}

uint32_t bfv_artillery_search_prefix_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],early_frame=frame+4,behavior=registers[4];
    uint32_t clock=word(BFV_COLLISION_CLOCK),clock_table=word(clock);
    *(volatile uint8_t *)(uintptr_t)(early_frame+0x13c)=0;
    *(volatile uint8_t *)(uintptr_t)(early_frame+0x148)=0;
    long double now=((get_time)(uintptr_t)word(clock_table+4))((void *)(uintptr_t)clock);
    __asm__ volatile("fstps 84(%[f])" : : "t"(now),[f]"r"(early_frame) : "st","memory");
    uint32_t events=word(BFV_COLLISION_EVENT_MANAGER),context=word(BFV_ARTILLERY_PREFIX_CONTEXT),events_table=word(events);
    store(early_frame+0x20,context);
    uint32_t descriptor=((get_word)(uintptr_t)word(events_table+0x7c))((void *)(uintptr_t)events);
    if(refresh_needed(early_frame,behavior,descriptor)) refresh_history(early_frame,behavior);

    uint32_t bot=word(early_frame+0x1e8);get(bot,0xd8);
    uint32_t gun=word(early_frame+0x1f4),gun_field=gun+0x24u;
    store(early_frame+0xd4,gun_field?word(gun+0x30):0);
    uint32_t component=gun_field?word(gun_field):0;
    store(frame+0xf4,gun_field?word(gun+0x38):0);
    uint32_t pattern;
    if(!byte(behavior+0x44)) {
        pattern=gun_field?word(gun+0x34):0;
    } else {
        uint32_t entry=get(bot,0xd4);store(frame+0xd4,entry);
        uint32_t receiver=word(gun+0x20);
        uint32_t view=argument(receiver,0x8c,frame+0x1bc);
        uint32_t handle=view?argument(view,0x5c,word(frame+0xd4)):UINT32_MAX;
        uint32_t object=resolve(handle),field=object+0x24u;
        pattern=field?word(object+0x34):0;
    }
    store(frame+0x50,pattern);
    store(frame+0xd0,get(component,0x3c));
    uint32_t position=get(gun,0x18);
    store(frame+0x128,word(position));store(frame+0x12c,word(position+4));
    uint32_t z=word(position+8),table=word(bot);
    store(frame+0x130,z);store(frame+0xdc,UINT32_MAX);store(frame+0x44,0);
    store(frame+0x84,0);store(frame+0x48,0);store(frame+0x144,0);
    store(frame+0xd4,UINT32_MAX);store(frame+0x4c,UINT32_MAX);
    uint32_t entry=((get_word)(uintptr_t)word(table+0xd4))((void *)(uintptr_t)bot);
    store(frame+0xa0,entry);
    uint32_t index=get(bot,0xdc),targets=word(behavior+8);
    uint32_t previous=word(targets+index*4),next_table=word(bot);
    store(frame+0x148,previous);
    index=((get_word)(uintptr_t)word(next_table+0xdc))((void *)(uintptr_t)bot);
    uint32_t weapons=word(behavior+0x30),previous_weapon=word(weapons+index*4);
    uint32_t manager=word(BFV_ARTILLERY_PREFIX_QUERY_MANAGER),seed=word(BFV_ARTILLERY_PREFIX_QUERY_SEED);
    store(frame+0xa8,previous_weapon);
    table=word(manager);
    *(volatile uint8_t *)(uintptr_t)(frame+0x13)=0;store(frame+0xf8,seed);
    uint32_t query=argument_table(manager,table,0x34,entry),query_table=word(query);
    index=get(bot,0xdc);
    uint32_t region_index=argument_table(query,query_table,0x7c,index);
    uint32_t region=region_at(region_index);store(frame+0x134,region);
    uint32_t scale=0x3f800000;
    if(region) {
        uint32_t inside=((get_argument)(uintptr_t)BFV_REGION_CONTAINS_POINT3)
            ((void *)(uintptr_t)region,frame+0x128);
        scale=(uint8_t)inside?0x3f800000:0x3f400000;
    }
    store(frame+0x9c,scale);
    uint32_t container=get(bot,0x84),header=word(container+4),iterator=word(header);
    store(frame+0x40,iterator);
    container=get(bot,0x84);
    registers[0]=iterator;registers[1]=bot;registers[2]=query_table;
    return iterator!=word(container+4);
}

__attribute__((naked)) void bfv_artillery_search_prefix_bridge(void)
{
    __asm__ volatile("push %%ebp; pushal; mov %%esp,%%eax; push %%eax;"
        "call _bfv_artillery_search_prefix_phase; add $4,%%esp; test %%eax,%%eax;"
        "jz 1f; popal; jmp %c0; 1: popal; jmp %c1"
        : : "i"(BFV_ARTILLERY_SEARCH_PREFIX_CONTINUE),"i"(BFV_ARTILLERY_AFTER_FIRST_PASS)
        : "memory");
}
