/* First-pass distance, driver predicate and candidate query gate. The wider
 * evaluator still owns the frame and later candidate-scoring phases. */
#include <stdint.h>
#include "target.h"
#include "aim_geometry.h"
#include "artillery_query_helpers.h"
typedef uint32_t (BFV_QUERY_TC *get_word)(void *);
typedef uint32_t (BFV_QUERY_TC *driver_predicate)(void *,uint32_t,const float *);
typedef void (BFV_QUERY_TC *predict_point)(void *,uint32_t,float *);
typedef uint32_t (BFV_QUERY_TC *query_service)(void *,float *,const float *,void *,
    uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t value) { *(volatile uint32_t *)(uintptr_t)p=value; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }
static uint16_t compare_distance(uint32_t frame)
{
    uint16_t status;
    __asm__ volatile("flds 28(%[f]); fmuls %[scale]; fcomps 40(%[f]); fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_QUERY_SCALE) : "st","memory");
    return status;
}
static void point_pair(uint32_t frame,uint32_t first,uint32_t second)
{
    /* Both inputs are loaded before the x/z stores, preserving NaN conversion
     * and overlap. The table needed by the caller is captured before stores. */
    __asm__ volatile("flds 8(%[a]); flds (%[b]); fstps 312(%[f]); fstps 316(%[f])"
        : : [f]"r"(frame),[a]"r"(first),[b]"r"(second) : "st","memory");
}
static void origin(uint32_t frame,uint32_t data)
{
    uint32_t x=word(data+0x30);
    /* The native copies x as raw bits; y/z pass through FLD/FSTP and can quiet
     * signaling NaNs. Store y, then x, then z. */
    __asm__ volatile("flds 56(%[d]); flds 52(%[d]); fstps 340(%[f]);"
        "movl %[x],336(%[f]); fstps 344(%[f])"
        : : [f]"r"(frame),[d]"r"(data),[x]"r"(x) : "st","memory");
}
static void adjust_point(uint32_t frame,uint32_t actual)
{
    /* x/y differences remain extended; z is rounded to +0x194 before addition.
     * All position inputs are read before any candidate-point stores. */
    __asm__ volatile(
        "flds 408(%[f]); fsubs (%[p]); flds 412(%[f]); fsubs 4(%[p]);"
        "flds 416(%[f]); fsubs 8(%[p]); fstps 404(%[f]); fxch;"
        "fadds 224(%[f]); fstps 224(%[f]); flds 228(%[f]); fadd %%st(1),%%st;"
        "fstps 228(%[f]); fstp %%st(0); flds 232(%[f]); fadds 404(%[f]); fstps 232(%[f])"
        : : [f]"r"(frame),[p]"r"(actual) : "st","memory");
}

uint32_t bfv_artillery_query_gate_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],target=registers[2],movement=registers[0];
    uint16_t status=compare_distance(frame)&0x0500;
    uint32_t driver=word(frame+0x1fc);
    if(status==0x0100 || status==0x0400) {
        if(!driver) return 0;
        uint32_t first=get(target,0x18),second=get(target,0x18);
        uint32_t table=word(word(frame+0xf8));registers[0]=table;
        point_pair(frame,first,second);
        /* The point pointer is pushed in advance in the original, for slot
         * 0x84. The event-2 helper itself is zero-argument (RET, not RET 4). */
        uint32_t value=bfv_component_event2_word((void *)(uintptr_t)word(frame+0x1fc));
        uint32_t receiver=word(frame+0xf8);
        uint32_t result=((driver_predicate)(uintptr_t)word(table+0x84))
            ((void *)(uintptr_t)receiver,value,(const float *)(uintptr_t)(frame+0x138));
        return (uint8_t)result?0:1;
    }
    if(driver) return 1;
    uint32_t component=get(registers[1],0xb0);
    uint32_t data=(uintptr_t)bfv_component_query_data((void *)(uintptr_t)component);
    uint32_t handle=word(word(frame+0x1f8)+0x20);
    origin(frame,data);
    store(frame+0x100,0);store(frame+0x104,0);store(frame+0x108,0);
    store(frame+0x30,handle);
    bfv_query_vector_append((void *)(uintptr_t)(frame+0xfc),(const uint32_t *)(uintptr_t)(frame+0x30));
    store(frame+0x30,word(target+0x20));
    bfv_query_vector_append((void *)(uintptr_t)(frame+0xfc),(const uint32_t *)(uintptr_t)(frame+0x30));
    uint32_t point=word(frame+0x18)+4,matrix=get(target,0x24);
    bfv_transform_point((const float *)(uintptr_t)matrix,(const float *)(uintptr_t)point,(float *)(uintptr_t)(frame+0xe0));
    if(movement) {
        uint32_t table=word(movement),reference=word(frame+0x58);
        ((predict_point)(uintptr_t)word(table+0x28))((void *)(uintptr_t)movement,reference,(float *)(uintptr_t)(frame+0x198));
        adjust_point(frame,get(target,0x18));
    }
    uint32_t table=word(word(frame+0x24));registers[0]=table;
    /* Slot 0x2c is a zero-argument identity getter. The original pre-pushes
     * the final query arguments (1,0), then leaves them for the ten-word call. */
    uint32_t identity=get(target,0x2c),receiver=word(frame+0x24);
    uint32_t result=((query_service)(uintptr_t)word(table+0x50))((void *)(uintptr_t)receiver,
        (float *)(uintptr_t)(frame+0x150),(const float *)(uintptr_t)(frame+0xe0),
        (void *)(uintptr_t)(frame+0xfc),0,0,1,1,identity,1,0);
    uint32_t rejected=(uint8_t)result!=0;
    bfv_query_vector_destroy((void *)(uintptr_t)(frame+0xfc));
    return !rejected;
}

__attribute__((naked)) void bfv_artillery_query_gate_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_query_gate_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0; 1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_QUERY_ACCEPT),"i"(BFV_ARTILLERY_FILTER_REJECT) : "memory");
}
