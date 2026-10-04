/* Position helpers used by the artillery candidate aiming gate. Matrix layout
 * and evaluation order match the inspected x86 code, including buffer overlap. */
#include "aim_geometry.h"
typedef void *(BFV_AIM_TC *get_object)(void *);
typedef void *(BFV_AIM_TC *notify_event)(void *,uint32_t);
static uint32_t word(uintptr_t p) { return *(volatile uint32_t *)p; }

float *BFV_AIM_TC bfv_transform_point(const float *matrix,const float *point,float *output)
{
    /* Affine point transform. Compute z, then y, then x before storing any
     * coordinate. Addition order differs for x and is significant for rounding
     * and NaN payloads. All products retain the native x87 precision. */
    __asm__ volatile(
        "flds 8(%[m]); fmuls (%[p]); flds 40(%[m]); fmuls 8(%[p]); faddp;"
        "flds 24(%[m]); fmuls 4(%[p]); faddp; fadds 56(%[m]);"
        "flds 4(%[m]); fmuls (%[p]); flds 36(%[m]); fmuls 8(%[p]); faddp;"
        "flds 20(%[m]); fmuls 4(%[p]); faddp; fadds 52(%[m]);"
        "flds 32(%[m]); fmuls 8(%[p]); flds 16(%[m]); fmuls 4(%[p]); faddp;"
        "flds (%[p]); fmuls (%[m]); faddp; fadds 48(%[m]);"
        "fstps (%[out]); fstps 4(%[out]); fstps 8(%[out])"
        : : [m]"r"(matrix),[p]"r"(point),[out]"r"(output) : "st","memory");
    return output;
}

float *BFV_AIM_TC bfv_vector_difference(const float *origin,float *output,const float *point)
{
    /* output = point - origin. Read all inputs before any stores, including
     * when output partly or fully overlaps an input vector. */
    __asm__ volatile(
        "flds (%[p]); fsubs (%[o]); flds 4(%[p]); fsubs 4(%[o]);"
        "flds 8(%[p]); fsubs 8(%[o]); fxch %%st(2);"
        "fstps (%[out]); fstps 4(%[out]); fstps 8(%[out])"
        : : [o]"r"(origin),[p]"r"(point),[out]"r"(output) : "st","memory");
    return output;
}

const float *BFV_AIM_TC bfv_aim_world_position(void *view,float *output)
{
    uintptr_t owner=word((uintptr_t)view+4);
    const float *world=((get_object)(uintptr_t)word(word(owner)+0x1c))((void *)owner);
    /* Fetch the mount after the callback; it may have replaced this pointer. */
    const float *mount=(const float *)(uintptr_t)(word((uintptr_t)view+8)+0x80);
    /* Transform world translation by the mount matrix. Unlike transform_point,
     * this helper stores each coordinate immediately, so aliasing can affect
     * the later coordinates. Keep its native product and addition order. */
    __asm__ volatile(
        "flds 16(%[m]); fmuls 52(%[w]); flds 32(%[m]); fmuls 56(%[w]); faddp;"
        "flds 48(%[w]); fmuls (%[m]); faddp; fadds 48(%[m]); fstps (%[out]);"
        "flds 20(%[m]); fmuls 52(%[w]); flds 4(%[m]); fmuls 48(%[w]); faddp;"
        "flds 36(%[m]); fmuls 56(%[w]); faddp; fadds 52(%[m]); fstps 4(%[out]);"
        "flds 24(%[m]); fmuls 52(%[w]); flds 8(%[m]); fmuls 48(%[w]); faddp;"
        "flds 40(%[m]); fmuls 56(%[w]); faddp; fadds 56(%[m]); fstps 8(%[out])"
        : : [m]"r"(mount),[w]"r"(world),[out]"r"(output) : "st","memory");
    return world;
}

const float *BFV_AIM_TC bfv_component_position(void *component,float *output)
{
    uintptr_t owner=word((uintptr_t)component+4), receiver=word(owner+0x20);
    void *view=((notify_event)(uintptr_t)word(word(receiver)+0xa0))((void *)receiver,5);
    return bfv_aim_world_position(view,output);
}
