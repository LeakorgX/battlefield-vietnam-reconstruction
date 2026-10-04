/* Region predicates used by the following artillery score modifier. Names
 * describe inspected arithmetic; ownership and world units remain unresolved. */
#include "region_geometry.h"
static uint32_t word(uintptr_t p) { return *(volatile uint32_t *)p; }

void *BFV_REGION_FC bfv_point_xz(const void *point,void *output)
{
    /* Copy x before reading z. Overlap can change the second source word. */
    *(volatile uint32_t *)output=word((uintptr_t)point);
    *(volatile uint32_t *)((uintptr_t)output+4)=word((uintptr_t)point+8);
    return output;
}

static uint16_t compare(const float *left,const float *right)
{
    uint16_t status;
    __asm__ volatile("flds (%[a]); fcomps (%[b]); fnstsw %%ax"
        : "=a"(status) : [a]"r"(left),[b]"r"(right) : "st","memory");
    return status;
}
static uint8_t ordered_less(uint16_t status)
{
    status&=0x0500;
    return status==0x0100 || status==0x0400;
}

uint8_t BFV_REGION_FC bfv_point_in_symmetric_bounds(const float *point,
    const float *bound,const float *center)
{
    /* Reject ordered point < bound on either axis, then reject ordered
     * reflected-x < point-x. Final y accepts less/equal or unordered. All
     * comparisons accept unordered, rather than using ordinary C bounds tests.
     * x remains extended while reflected y is rounded to a float32 local. */
    if(ordered_less(compare(point,bound))) return 0;
    if(ordered_less(compare(point+1,bound+1))) return 0;
    float reflected_y;uint16_t status;
    __asm__ volatile(
        "flds (%[c]); fsubs (%[b]); fadds (%[c]);"
        "flds 4(%[c]); fsubs 4(%[b]); fadds 4(%[c]); fstps %[y];"
        "fcomps (%[p]); fnstsw %%ax"
        : "=a"(status),[y]"=m"(reflected_y)
        : [c]"r"(center),[b]"r"(bound),[p]"r"(point) : "st","memory");
    if(ordered_less(status)) return 0;
    return (compare(point+1,&reflected_y)&0x4100)!=0;
}

uint8_t BFV_REGION_TC bfv_region_contains_point2(void *region,const float *point)
{
    uintptr_t object=(uintptr_t)region;
    if(*(volatile uint8_t *)(object+0x1c4)) {
        uint16_t status;
        /* Preserve the native two differences and four-register square-root
         * sequence, including NaN choice and stack-register retirement. */
        __asm__ volatile(
            "flds (%[p]); fsubs 132(%[o]); flds 4(%[p]); fsubs 136(%[o]);"
            "fld %%st(0); fmul %%st(1),%%st; fld %%st(2); fmul %%st(3),%%st;"
            "faddp; fsqrt; fstp %%st(2); fstp %%st(0); fcomps 460(%[o]); fnstsw %%ax"
            : "=a"(status) : [p]"r"(point),[o]"r"(object) : "st","memory");
        return ordered_less(status);
    }
    uint32_t bounds[4];
    bounds[0]=word(object+0x84);bounds[1]=word(object+0x88);
    bounds[2]=word(object+0x114);bounds[3]=word(object+0x118);
    return bfv_point_in_symmetric_bounds(point,(const float *)(bounds+2),(const float *)bounds);
}

uint8_t BFV_REGION_TC bfv_region_contains_point3(void *region,const void *point)
{
    uint32_t planar[2];
    bfv_point_xz(point,planar);
    return bfv_region_contains_point2(region,(const float *)planar);
}
