/* Second-pass movement score. The evaluator's later gates/scoring
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
    __asm__ volatile("flds 40(%1); fcomps 32(%1); fnstsw %%ax"
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
    __asm__ volatile("fldt %[v]; fmuls %[scale]; fadds %[one]; fdivrs %[one]; fstps 52(%[f])"
        : : [v]"m"(length),[f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_SCALE),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");
}
static void projections(uint32_t frame,uint32_t vector)
{
    /* Retain the four dot products simultaneously as in the native x87 stack.
     * Store each difference to float32 before its ordered-negative clamp. */
    __asm__ volatile(
        "flds 136(%[f]); fmuls 176(%[f]); flds 184(%[f]); fmuls 144(%[f]); faddp;"
        "flds 180(%[f]); fmuls 140(%[f]); faddp;"
        "flds 180(%[f]); fmuls 4(%[v]); flds 176(%[f]); fmuls (%[v]); faddp;"
        "flds 184(%[f]); fmuls 8(%[v]); faddp;"
        "flds 124(%[f]); fmuls 4(%[v]); flds 120(%[f]); fmuls (%[v]); faddp;"
        "flds 128(%[f]); fmuls 8(%[v]); faddp;"
        "flds 136(%[f]); fmuls 120(%[f]); flds 144(%[f]); fmuls 128(%[f]); faddp;"
        "flds 140(%[f]); fmuls 124(%[f]); faddp; fsubrp;"
        "fsts 36(%[f]); fcomps %[zero]; fnstsw %%ax; test $5,%%ah; jp 1f; movl $0,36(%[f]);"
        "1: fsubp; fsts 60(%[f]); fcomps %[zero]; fnstsw %%ax; test $5,%%ah; jp 2f; movl $0,60(%[f]); 2:"
        : : [f]"r"(frame),[v]"r"(vector),
        [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "eax","cc","st","memory");
}
static void projection_score(uint32_t vector,uint32_t frame)
{
    long double length=((vector_length)BFV_VECTOR_LENGTH)((const float *)(uintptr_t)vector);
    __asm__ volatile(
        "fldt %[v]; fmuls %[scale]; flds 60(%[f]); fadds 36(%[f]); fadd %%st(0),%%st;"
        "faddp; fadds %[one]; fdivrs %[quarter]; fstps 52(%[f])"
        : : [v]"m"(length),[f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_SCALE),
        [one]"m"(*(volatile float *)BFV_MATH_ONE),
        [quarter]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_QUARTER) : "st","memory");
}

/* PUSHAD packet: EDI, ESI, EBP, incoming ESP, EBX, EDX, ECX, EAX.
 * Routes retain the native unit block and driver reload as separate entries. */
uint32_t bfv_artillery_second_movement_score_phase(uint32_t *registers)
{
    uint32_t frame=registers[3];
    if(!registers[0]) return 0;
    if(compare_distance(frame)&0x4100) {
        reciprocal_length(get(word(frame+0x24),0x14),frame);return 2;
    }
    uint32_t driver=word(frame+0x1fc),x,y,z;
    registers[0]=driver;
    if(driver) {
        scalar_store(driver,frame+0xc0);
        /* The scalar callback may replace frame1fc; native EDI stays captured. */
        uint32_t point=get(driver,0x14);
        x=word(point);y=word(point+4);z=word(point+8);
    } else {
        store(frame+0x110,0);x=word(frame+0x110);
        store(frame+0x114,0);y=word(frame+0x114);
        store(frame+0x118,0);z=word(frame+0x118);
        store(frame+0xc0,0);
    }
    /* Driver callbacks precede this capture, and the copy stores follow it. */
    uint32_t movement=word(frame+0x24);
    registers[4]=movement;
    store(frame+0x88,x);store(frame+0x8c,y);store(frame+0x90,z);
    if(scalar_delta(movement,frame)&0x4100) {
        store(frame+0x34,0x3e800000);return 1;
    }
    store(frame+0x150,0);store(frame+0x154,0x3f800000);store(frame+0x158,0);
    float *cross=bfv_vector_cross_assign((float *)(uintptr_t)(frame+0x150),(const float *)(uintptr_t)(frame+0x78));
    copy_vector(frame+0xb0,(uint32_t)(uintptr_t)cross);
    uint32_t vector=get(movement,0x14);
    projections(frame,vector);
    /* The vector may alias either rounded/clamped projection destination. */
    projection_score(vector,frame);
    return 1;
}

__attribute__((naked)) void bfv_artillery_second_movement_score_bridge(void)
{
    __asm__ volatile(
        "pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_second_movement_score_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; cmp $1,%%eax; je 2f;"
        "popal; jmp %c2; 1: popal; jmp %c0; 2: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_SECOND_MOVEMENT_UNIT_SCORE),
        "i"(BFV_ARTILLERY_SECOND_MOVEMENT_SCORE+7),
        "i"(BFV_ARTILLERY_SECOND_MOVEMENT_SCORE) : "memory");
}
