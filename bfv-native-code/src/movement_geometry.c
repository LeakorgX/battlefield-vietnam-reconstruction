/* Helpers used by the candidate movement score. */
#include <stdint.h>
#include "movement_geometry.h"
typedef void *(BFV_MOVEMENT_TC *notify_event)(void *,uint32_t);
static uint32_t word(uintptr_t p) { return *(volatile uint32_t *)p; }

float *BFV_MOVEMENT_TC bfv_vector_cross_assign(float *left,const float *right)
{
    /* Capture all left coordinates, then store x/y/z immediately. The right
     * vector is reread for each coordinate, including overlapping buffers. */
    uint32_t saved[3];
    saved[0]=word((uintptr_t)left);
    saved[1]=word((uintptr_t)left+4);
    saved[2]=word((uintptr_t)left+8);
    __asm__ volatile(
        "flds 4(%[l]); fmuls 8(%[r]); flds 8(%[l]); fmuls 4(%[r]); fsubrp; fstps (%[o]);"
        "flds 8(%[l]); fmuls (%[r]); flds (%[l]); fmuls 8(%[r]); fsubrp; fstps 4(%[o]);"
        "flds (%[l]); fmuls 4(%[r]); flds 4(%[l]); fmuls (%[r]); fsubrp; fstps 8(%[o])"
        : : [l]"r"(saved),[r]"r"(right),[o]"r"(left) : "st","memory");
    return left;
}

long double BFV_MOVEMENT_TC bfv_component_event2_scalar(void *component)
{
    uintptr_t owner=word((uintptr_t)component+4),receiver=word(owner+0x20);
    void *event=((notify_event)(uintptr_t)word(word(receiver)+0xa0))((void *)receiver,2);
    return *(volatile float *)(uintptr_t)(word((uintptr_t)event+0x14)+8);
}
