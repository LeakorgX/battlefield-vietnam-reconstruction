/* First-pass movement vector retrieval and the initial distance/flag gates.
 * Later velocity arithmetic and candidate scoring remain in the evaluator. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }

static uint16_t scaled_distance(uint32_t frame,uint32_t scale)
{
    uint16_t status;
    __asm__ volatile("flds 28(%1); fmuls (%2); fcomps 40(%1); fnstsw %%ax"
        : "=a"(status) : "r"(frame),"r"(scale) : "st","memory");
    return status;
}
static uint16_t vector_threshold(uint32_t frame)
{
    uint16_t status;
    /* sqrt(z*z + y*y + x*x), retaining each native extended intermediate. */
    __asm__ volatile(
        "flds 128(%[f]); fmuls 128(%[f]); flds 124(%[f]); fmuls 124(%[f]); faddp;"
        "flds 120(%[f]); fmuls 120(%[f]); faddp; fsqrt; fcomps %[limit]; fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame),
        [limit]"m"(*(volatile float *)BFV_ARTILLERY_MOVEMENT_THRESHOLD) : "st","memory");
    return status;
}

/* PUSHAD packet: EDI, ESI, EBP, incoming ESP, EBX, EDX, ECX, EAX.
 * EDI must carry the captured movement object into the later native scoring
 * stage. ESI/EBP/EBX and the incoming frame remain live and preserved. */
uint32_t bfv_artillery_movement_gate_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],movement=word(frame+0x30);
    registers[0]=movement;
    store(frame+0x78,0); store(frame+0x7c,0); store(frame+0x80,0);
    if(movement) {
        uint32_t vector=get(movement,0x14);
        /* The original reads all three words before writing the frame. */
        uint32_t x=word(vector),y=word(vector+4),z=word(vector+8);
        store(frame+0x78,x); store(frame+0x7c,y); store(frame+0x80,z);
    }
    uint32_t source=word(frame+0x1f8),target=registers[2];
    if((word(source+4)&2) || !(word(target+4)&2)) return 0;
    uint32_t predicate=get(word(frame+0x50),0x28);
    if((uint8_t)predicate) {
        /* Native AH mask accepts equality, smaller values and unordered input.
         * The other outcome resumes the score=1 stage. */
        return (scaled_distance(frame,BFV_ARTILLERY_MOVEMENT_ALT_SCALE)&0x4100)?0:2;
    }
    uint16_t distance=scaled_distance(frame,BFV_ARTILLERY_MOVEMENT_SCALE)&0x0500;
    if(distance!=0x0100 && distance!=0x0400 && (vector_threshold(frame)&0x4100)) return 0;
    store(frame+0x20,0);
    return 1;
}

__attribute__((naked)) void bfv_artillery_movement_gate_bridge(void)
{
    __asm__ volatile(
        "pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_movement_gate_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; cmp $1,%%eax; je 2f;"
        "popal; jmp %c2; 1: popal; jmp %c0; 2: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_MOVEMENT_CONTINUE),"i"(BFV_ARTILLERY_MOVEMENT_SCORE),
        "i"(BFV_ARTILLERY_MOVEMENT_UNIT_SCORE) : "memory");
}
