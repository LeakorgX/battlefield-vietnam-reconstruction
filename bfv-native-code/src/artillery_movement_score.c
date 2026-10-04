/* Following first-pass movement score. The evaluator's later gates/scoring
 * remain native. Frame offsets retain the established instruction layout. */
#include <stdint.h>
#include "movement_geometry.h"
#include "target.h"
typedef uint32_t (BFV_MOVEMENT_TC *get_word)(void *);
typedef long double (BFV_MOVEMENT_TC *vector_length)(const float *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }
static void copy_vector(uint32_t destination,uint32_t source)
{
    uint32_t x=word(source),y=word(source+4),z=word(source+8);
    store(destination,x);store(destination+4,y);store(destination+8,z);
}
static uint16_t compare_distance(uint32_t frame)
{
    uint16_t status;
    __asm__ volatile("flds 40(%1); fcomps 28(%1); fnstsw %%ax"
        : "=a"(status) : "r"(frame) : "st","memory");
    return status;
}
static void scalar_store(uint32_t component,uint32_t destination)
{
    long double value=bfv_component_event2_scalar((void *)(uintptr_t)component);
    __asm__ volatile("fldt %[v]; fstps (%[o])"
        : : [v]"m"(value),[o]"r"(destination) : "st","memory");
}
static uint16_t scalar_delta(uint32_t component,uint32_t frame)
{
    long double value=bfv_component_event2_scalar((void *)(uintptr_t)component);
    uint16_t status;
    __asm__ volatile("fldt %[v]; fsubs 192(%[f]); fcomps %[limit]; fnstsw %%ax"
        : "=a"(status) : [v]"m"(value),[f]"r"(frame),
        [limit]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_DELTA_LIMIT) : "st","memory");
    return status;
}
static void reciprocal_length(uint32_t vector,uint32_t frame)
{
    long double length=((vector_length)BFV_VECTOR_LENGTH)((const float *)(uintptr_t)vector);
    __asm__ volatile("fldt %[v]; fmuls %[scale]; fadds %[one]; fdivrs %[one]; fstps 32(%[f])"
        : : [v]"m"(length),[f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_SCALE),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");
}
static void projections(uint32_t frame,uint32_t vector)
{
    /* Retain the four dot products simultaneously as in the native x87 stack.
     * Store each difference to float32 before its ordered-negative clamp. */
    __asm__ volatile(
        "flds 184(%[f]); fmuls 144(%[f]); flds 180(%[f]); fmuls 140(%[f]); faddp;"
        "flds 136(%[f]); fmuls 176(%[f]); faddp;"
        "flds 184(%[f]); fmuls 8(%[v]); flds 180(%[f]); fmuls 4(%[v]); faddp;"
        "flds 176(%[f]); fmuls (%[v]); faddp;"
        "flds 108(%[f]); fmuls 8(%[v]); flds 104(%[f]); fmuls 4(%[v]); faddp;"
        "flds 100(%[f]); fmuls (%[v]); faddp;"
        "flds 144(%[f]); fmuls 108(%[f]); flds 140(%[f]); fmuls 104(%[f]); faddp;"
        "flds 136(%[f]); fmuls 100(%[f]); faddp; fsubrp;"
        "fsts 56(%[f]); fcomps %[zero]; fnstsw %%ax; test $5,%%ah; jp 1f; movl $0,56(%[f]);"
        "1: fsubp; fsts 84(%[f]); fcomps %[zero]; fnstsw %%ax; test $5,%%ah; jp 2f; movl $0,84(%[f]); 2:"
        : : [f]"r"(frame),[v]"r"(vector),
        [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "eax","cc","st","memory");
}
static void projection_score(uint32_t vector,uint32_t frame)
{
    long double length=((vector_length)BFV_VECTOR_LENGTH)((const float *)(uintptr_t)vector);
    __asm__ volatile(
        "fldt %[v]; fmuls %[scale]; flds 84(%[f]); fadds 56(%[f]); fadd %%st(0),%%st;"
        "faddp; fadds %[one]; fdivrs %[quarter]; fstps 32(%[f])"
        : : [v]"m"(length),[f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_SCALE),
        [one]"m"(*(volatile float *)BFV_MATH_ONE),
        [quarter]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_QUARTER) : "st","memory");
}

void bfv_artillery_movement_score_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],movement=registers[0];
    if(!movement) { store(frame+0x20,0x3f800000);return; }
    if(compare_distance(frame)&0x4100) {
        reciprocal_length(get(movement,0x14),frame);return;
    }
    uint32_t driver=word(frame+0x1fc);
    if(driver) {
        scalar_store(driver,frame+0xc0);
        /* The event callback may have replaced the driver pointer. */
        copy_vector(frame+0x88,get(word(frame+0x1fc),0x14));
    } else {
        store(frame+0x11c,0);store(frame+0x120,0);store(frame+0x124,0);store(frame+0xc0,0);
        copy_vector(frame+0x88,frame+0x11c);
    }
    if(scalar_delta(movement,frame)&0x4100) { store(frame+0x20,0x3e800000);return; }
    store(frame+0x180,0);store(frame+0x184,0x3f800000);store(frame+0x188,0);
    float *cross=bfv_vector_cross_assign((float *)(uintptr_t)(frame+0x180),(const float *)(uintptr_t)(frame+0x64));
    copy_vector(frame+0xb0,(uint32_t)(uintptr_t)cross);
    uint32_t vector=get(movement,0x14);
    projections(frame,vector);
    /* The vector may alias either rounded/clamped projection destination. */
    projection_score(vector,frame);
}

__attribute__((naked)) void bfv_artillery_movement_score_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_movement_score_phase;"
        "add $4,%%esp; popal; jmp %c0;"
        : : "i"(BFV_ARTILLERY_MOVEMENT_SCORE) : "memory");
}
