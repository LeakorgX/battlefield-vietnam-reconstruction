/* Recovered geometric arithmetic. Names describe observed calculations.
 * Float32 stores and arithmetic order are part of the original x87 behavior. */
#include "collision_geometry.h"

static float as_float(uint32_t bits)
{ union { uint32_t bits; float value; } v = {bits}; return v.value; }
static uint32_t as_bits(float value)
{ union { float value; uint32_t bits; } v; v.value = value; return v.bits; }
static long double root(long double value)
{ __asm__ volatile ("fsqrt" : "+t"(value)); return value; }

long double __attribute__((stdcall)) bfv_line_distance_squared(vector_bits point, vector_bits origin, vector_bits direction)
{
    volatile long double dx = (long double)as_float(point.x) - as_float(origin.x);
    volatile long double dy = (long double)as_float(point.y) - as_float(origin.y);
    volatile long double dz = (long double)as_float(point.z) - as_float(origin.z);
    float ux = as_float(direction.x), uy = as_float(direction.y), uz = as_float(direction.z);
    volatile float cross_x = (long double)uy * dz - (long double)uz * dy;
    volatile float cross_y = (long double)uz * dx - dz * (long double)ux;
    volatile long double cross_z = dy * (long double)ux - (long double)uy * dx;
    volatile long double numerator = cross_z * cross_z;
    numerator = numerator + (long double)cross_y * cross_y;
    numerator = numerator + (long double)cross_x * cross_x;
    volatile long double denominator = (long double)uz * uz + (long double)uy * uy;
    denominator = denominator + (long double)ux * ux;
    return numerator / denominator;
}

long double __attribute__((stdcall)) bfv_collision_distance(vector_bits point, vector_bits start, vector_bits end)
{
    /* Native arithmetic uses only X/Z for the segment and point. */
    volatile float dx = (long double)as_float(end.x) - as_float(start.x);
    /* Y is discarded geometrically, but the native subtraction still executes. */
    volatile float dy = (long double)as_float(end.y) - as_float(start.y);
    (void)dy;
    volatile float dz = (long double)as_float(end.z) - as_float(start.z);
    float nx = as_float(as_bits(dx) ^ 0x80000000u);
    float nz = as_float(as_bits(dz) ^ 0x80000000u);
    volatile long double projection = (long double)dz * as_float(start.z) + (long double)dx * as_float(start.x);
    projection = -projection;
    projection = projection + (long double)dx * as_float(point.x);
    projection = projection + (long double)dz * as_float(point.z);
    if (projection >= 0.0L) {
        volatile long double other = (long double)nz * as_float(end.z) + (long double)nx * as_float(end.x);
        other = -other;
        other = other + (long double)nz * as_float(point.z);
        other = other + (long double)as_float(point.x) * nx;
        if (other >= 0.0L) {
            vector_bits p = {point.x, 0, point.z}, a = {start.x, 0, start.z};
            vector_bits direction = {as_bits(dx), 0, as_bits(dz)};
            return root(bfv_line_distance_squared(p, a, direction));
        }
    }
    vector_bits endpoint = projection < 0.0L ? start : end;
    volatile float ex = (long double)as_float(endpoint.x) - as_float(point.x);
    volatile float ez = (long double)as_float(endpoint.z) - as_float(point.z);
    volatile long double square = (long double)ez * ez + (long double)ex * ex;
    return root(square);
}
