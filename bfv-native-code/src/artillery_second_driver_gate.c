/* Second-pass distance/driver predicate. Later region and category stages
 * retain the evaluator frame. Virtual position/predicate methods stay explicit. */
#include <stdint.h>
#include "artillery_query_helpers.h"
#include "target.h"
typedef uint32_t (BFV_QUERY_TC *get_word)(void *);
typedef uint32_t (BFV_QUERY_TC *predicate)(void *,uint32_t,const float *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static uint32_t position(uint32_t object)
{ return ((get_word)(uintptr_t)word(word(object)+0x18))((void *)(uintptr_t)object); }
static uint16_t distance_status(uint32_t frame)
{
    uint16_t status;
    __asm__ volatile("flds 32(%[f]); fmuls %[scale]; fcomps 40(%[f]); fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_QUERY_SCALE) : "st","memory");
    return status;
}
static void point_pair(uint32_t frame,uint32_t first,uint32_t second)
{
    /* Both coordinates load before either store; signaling NaNs convert
     * through x87 rather than being copied as raw words. */
    __asm__ volatile("flds 8(%[a]); flds (%[b]); fstps 312(%[f]); fstps 316(%[f])"
        : : [f]"r"(frame),[a]"r"(first),[b]"r"(second) : "st","memory");
}
uint32_t bfv_artillery_second_driver_gate_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],driver=registers[0];
    uint16_t status=distance_status(frame)&0x0500;
    if(status!=0x0100 && status!=0x0400) return 1;
    if(!driver) return 0;
    uint32_t target=word(frame+0x18);
    uint32_t first=position(target),second=position(target);
    /* The second position callback precedes receiver/table capture. The
     * event callback follows capture and can change the table's method word. */
    uint32_t receiver=word(frame+0xf8),table=word(receiver);
    registers[4]=receiver;registers[2]=table;
    point_pair(frame,first,second);
    uint32_t event_word=bfv_component_event2_word((void *)(uintptr_t)driver);
    uint32_t result=((predicate)(uintptr_t)word(table+0x84))
        ((void *)(uintptr_t)receiver,event_word,(const float *)(uintptr_t)(frame+0x138));
    return (uint8_t)result==0;
}
__attribute__((naked)) void bfv_artillery_second_driver_gate_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_driver_gate_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_SECOND_DRIVER_ACCEPT),"i"(BFV_ARTILLERY_SECOND_REJECT) : "memory");
}
