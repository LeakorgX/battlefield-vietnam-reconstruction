/* First-pass timestamp weight, event position and normalized displacement.
 * This frame is still owned by the encompassing artillery evaluator. */
#include <stdint.h>
#include "target.h"
#include "collision_events.h"
#define TC __attribute__((thiscall))
#define SC __attribute__((stdcall))
typedef uint32_t (TC *lookup_timestamp)(void *,uint32_t,float *);
typedef void (TC *write_position)(void *,float *);
typedef uint32_t (TC *event_interface)(void *,uint32_t);
typedef long double (SC *clamp_value)(float,float,float);
typedef long double (SC *maximum_value)(float,float);
typedef long double (TC *vector_length)(const float *);
typedef float *(TC *vector_divide)(float *,float);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }

void bfv_artillery_candidate_setup_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],bot=registers[1],record=word(frame+0x18);
    registers[0]=record;
    uint32_t found=((lookup_timestamp)(uintptr_t)word(word(bot)+0x1a4))(
        (void *)(uintptr_t)bot,word(record),(float *)(uintptr_t)(frame+0x10c));
    if(!(uint8_t)found) store(frame+0x2c,0x3f800000);
    else {
        float scaled;
        __asm__ volatile("flds 88(%[f]); fsubs 268(%[f]); fmuls %[scale]; fstps %[out]"
            : [out]"=m"(scaled) : [f]"r"(frame),
            [scale]"m"(*(volatile float *)BFV_ARTILLERY_SECOND_WEIGHT_SCALE) : "st","memory");
        long double clamped=((clamp_value)(uintptr_t)BFV_FLOAT_CLAMP)(0.0f,scaled,1.0f);
        __asm__ volatile("fldt %[v]; fmuls %[scale]; fsubrs %[one]; fstps 44(%[f])"
            : : [v]"m"(clamped),[f]"r"(frame),
            [scale]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_SCALE),
            [one]"m"(*(volatile float *)BFV_ARTILLERY_CATEGORY_SCALE) : "st","memory");
    }
    uint32_t interface=((event_interface)(uintptr_t)BFV_COLLISION_EVENT_INTERFACE)(
        (void *)(uintptr_t)registers[2],2);
    store(frame+0x30,interface);
    if(interface) ((write_position)(uintptr_t)word(word(interface)+0x2c))(
        (void *)(uintptr_t)interface,(float *)(uintptr_t)(frame+0xc4));
    else {
        /* EDI captures the record before timestamp lookup. The fallback keeps
         * that capture even when a callback changes frame+0x18. Copies are
         * sequential, retaining buffer-overlap effects. */
        record+=0x1c;registers[0]=record;
        store(frame+0xc4,word(record));store(frame+0xc8,word(record+4));store(frame+0xcc,word(record+8));
    }
    /* Resolve native ESP-relative offsets with its pending 0.5 argument:
     * origin +128/+12c/+130; output +64/+68/+6c. All three differences
     * precede stores, with z, x, y float32 rounding and intervening copies. */
    __asm__ volatile(
        "flds 196(%[f]); fsubs 296(%[f]);"
        "flds 200(%[f]); fsubs 300(%[f]);"
        "flds 204(%[f]); fsubs 304(%[f]); fstps 472(%[f]);"
        "movl 472(%[f]),%%edx; fxch; movl %%edx,280(%[f]);"
        "fstps 272(%[f]); movl 272(%[f]),%%eax; movl %%eax,100(%[f]);"
        "fstps 276(%[f]); movl 276(%[f]),%%eax;"
        "movl %%eax,104(%[f]); movl %%edx,108(%[f])"
        : : [f]"r"(frame) : "eax","edx","st","memory");
    /* The native temporary z word remains live in EDX at the continuation. */
    registers[5]=word(frame+0x1d8);
    long double length=((vector_length)(uintptr_t)BFV_VECTOR_LENGTH)((const float *)(uintptr_t)(frame+0x64));
    float rounded;
    __asm__ volatile("fldt %[v]; fstps %[out]" : [out]"=m"(rounded) : [v]"m"(length) : "st");
    /* The native call receives (rounded length, 0.5) after its pending stack
     * temporary is accounted for. The order matters for NaNs. */
    long double distance=((maximum_value)(uintptr_t)BFV_FLOAT_MAXIMUM)(rounded,0.5f);
    __asm__ volatile("fldt %[v]; fstps 40(%[f])" : : [v]"m"(distance),[f]"r"(frame) : "st","memory");
    ((vector_divide)(uintptr_t)BFV_VECTOR_DIVIDE)((float *)(uintptr_t)(frame+0x64),
        *(volatile float *)(uintptr_t)(frame+0x28));
    /* vector_divide returns the same address in EAX and leaves it in ECX. */
    registers[6]=frame+0x64;registers[7]=frame+0x64;
}
__attribute__((naked)) void bfv_artillery_candidate_setup_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_candidate_setup_phase;"
        "add $4,%%esp; popal; jmp %c0;" : : "i"(BFV_ARTILLERY_WEAPONS) : "memory");
}
