/* Final alternate first-pass score scaling and rounded best-target selection.
 * This remains an inline stage of the larger native evaluator. */
#include <stdint.h>
#include "target.h"
#include "scalar_vector_math.h"
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void store(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static void scale_score(uint32_t frame)
{
    float ratio;
    __asm__ volatile("flds 40(%[f]); fdivs 28(%[f]); fmuls %[scale]; fstps %[r]"
        : [r]"=m"(ratio) : [f]"r"(frame),
        [scale]"m"(*(volatile float *)BFV_ARTILLERY_CATEGORY_SCALE) : "st","memory");
    float lower=(float)bfv_float_maximum(0.1f,ratio);
    long double attenuation=bfv_float_minimum(1.0f,lower);
    __asm__ volatile("fsubrs %[one]; fmuls 20(%[f]); fmuls 32(%[f]);"
        "fmuls 116(%[f]); fmuls 52(%[f]); fmuls 44(%[f]); fstps 20(%[f])"
        : : "t"(attenuation),[f]"r"(frame),
        [one]"m"(*(volatile float *)BFV_MATH_ONE) : "st","memory");
}
void bfv_artillery_alternate_finish_phase(uint32_t *registers)
{
    uint32_t frame=registers[3],target=registers[2],bot=registers[1];
    scale_score(frame);
    /* The original call is the complete four-byte MOV EAX,[ECX+0x2c]; RET
     * getter. Keep its single field read after scaling, including aliases. */
    uint32_t limit=word(bot+0x2c);
    store(frame+0x30,limit);
    uint16_t status;
    __asm__ volatile("fildl 48(%[f]); fcomps 92(%[f]); fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame) : "st","memory");
    if(!(status&0x0100)) {
        __asm__ volatile("flds 20(%[f]); fmuls %[scale]; fstps 20(%[f])"
            : : [f]"r"(frame),[scale]"m"(*(volatile float *)BFV_ARTILLERY_LIMIT_SCALE) : "st","memory");
    }
    if(!(word(target+0x10)&8)) return;
    __asm__ volatile("flds 20(%[f]); fcomps 72(%[f]); fnstsw %%ax"
        : "=a"(status) : [f]"r"(frame) : "st","memory");
    if(!(status&0x4100)) {
        uint32_t record=word(frame+0x40),rounded=word(frame+0x14);
        uint32_t data=word(record+8),index=word(frame+0x60);
        store(frame+0x48,rounded);store(frame+0xdc,word(data));store(frame+0x4c,index);
    }
}
__attribute__((naked)) void bfv_artillery_alternate_finish_bridge(void)
{
    __asm__ volatile("pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_alternate_finish_phase;"
        "add $4,%%esp; popal; jmp %c0;"
        : : "i"(BFV_ARTILLERY_FILTER_REJECT) : "memory");
}
