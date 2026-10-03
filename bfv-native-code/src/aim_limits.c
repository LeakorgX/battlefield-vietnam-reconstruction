/* Direction-to-mount aiming limit predicate used by artillery target scoring.
 * Matrix composition and the inverse-sine runtime remain native dependencies. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
#define SC __attribute__((stdcall))
typedef void *(TC *get_matrix)(void *);
typedef void *(TC *compose_matrix)(void *, const void *, const void *);
typedef long double (SC *inverse_sine)(float);
typedef void *(TC *notify_event)(void *, uint32_t);
static uint32_t word(uintptr_t address)
{ return *(volatile uint32_t *)address; }
static uintptr_t method(uintptr_t object, uint32_t offset)
{ return word(word(object) + offset); }
static uint16_t compare(long double value, const float *bound)
{
    uint16_t status;
    __asm__ volatile("fldt %[value]; fcomps %[bound]; fnstsw %%ax"
        : "=a"(status) : [value]"m"(value), [bound]"m"(*bound) : "st");
    return status;
}
static uint32_t less(uint16_t status)
{ status &= 0x0500; return status == 0x0100 || status == 0x0400; }
static uint16_t compare_bound(long double value, const float *bound)
{
    uint16_t status;
    __asm__ volatile("fldt %[value]; flds %[bound]; fcomp %%st(1); fnstsw %%ax; fstp %%st(0)"
        : "=a"(status) : [value]"m"(value), [bound]"m"(*bound) : "st");
    return status;
}
static long double dot(const float *row, const float *direction)
{
    long double value;
    /* (row.y * direction.y + row.z * direction.z) + row.x * direction.x. */
    __asm__ volatile(
        "flds 4(%[r]); fmuls 4(%[d]); flds 8(%[r]); fmuls 8(%[d]);"
        "faddp; flds (%[r]); fmuls (%[d]); faddp; fstpt %[value]"
        : [value]"=m"(value) : [r]"r"(row), [d]"r"(direction) : "st", "memory");
    return value;
}
static long double reflect_angle(long double angle, float sign)
{
    long double value;
    /* GAS uses the opposite no-operand spelling from Intel's FSUBRP ST(1). */
    __asm__ volatile("fldt %[angle]; flds %[sign]; fmuls %[pi]; fsubp; fstpt %[value]"
        : [value]"=m"(value) : [angle]"m"(angle), [sign]"m"(sign),
          [pi]"m"(*(volatile float *)BFV_AIM_PI) : "st");
    return value;
}

uint8_t TC bfv_aim_within_limits(void *view, const float *volatile direction)
{
    uintptr_t owner = word((uintptr_t)view + 4);
    uintptr_t mount = word((uintptr_t)view + 8);
    float matrix[16];
    void *world = ((get_matrix)method(owner, 0x1c))((void *)owner);
    ((compose_matrix)BFV_AIM_COMPOSE)(matrix, (void *)(mount + 0x80), world);
    /* Native tests exact float bits, and still composes the matrix first. */
    if (word(mount + 0x68) == 0xc0490fdb && word(mount + 0x74) == 0x40490fdb) return 1;

    const float *saved_direction = direction;
    long double projection = dot(matrix, saved_direction);
    const float *negative_one = (const float *)BFV_AIM_NEGATIVE_ONE;
    const float *one = (const float *)BFV_MATH_ONE;
    union { float value; uint32_t bits; } input;
    if (!(compare_bound(projection, negative_one) & 0x4100)) input.bits = 0xbf800000;
    else if (less(compare_bound(projection, one))) input.bits = 0x3f800000;
    else input.value = projection;
    /* The native function reuses its incoming pointer slot as a float temporary.
     * Retain the original pointer separately for the second projection. */
    direction = (const float *)(uintptr_t)input.bits;
    long double angle = ((inverse_sine)BFV_AIM_INVERSE_SINE)(input.value);
    const float *zero = (const float *)BFV_ARTILLERY_FILTER_ZERO;
    if (less(compare(dot(matrix + 8, saved_direction), zero))) {
        float sign;
        if (!(compare(angle, zero) & 0x4100)) sign = 1.0f;
        else if (less(compare(angle, zero))) sign = -1.0f;
        else sign = 0.0f;
        angle = reflect_angle(angle, sign);
    }
    if (less(compare(angle, (const float *)(mount + 0x68)))) return 0;
    return (compare(angle, (const float *)(mount + 0x74)) & 0x4100) != 0;
}

uint8_t TC bfv_aim_direction(void *component, const float *direction)
{
    uintptr_t owner = word((uintptr_t)component + 4);
    uintptr_t receiver = word(owner + 0x20);
    /* The native wrapper pushes direction early for the following predicate;
     * event dispatch consumes only event 5, not that direction argument. */
    void *view = ((notify_event)method(receiver, 0xa0))((void *)receiver, 5);
    return bfv_aim_within_limits(view, direction);
}
