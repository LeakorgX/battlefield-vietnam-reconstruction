#ifndef BFV_COLLISION_GEOMETRY_H
#define BFV_COLLISION_GEOMETRY_H
#include <stdint.h>
typedef struct { uint32_t x, y, z; } vector_bits;
long double __attribute__((stdcall)) bfv_line_distance_squared(vector_bits point, vector_bits origin, vector_bits direction);
long double __attribute__((stdcall)) bfv_collision_distance(vector_bits point, vector_bits start, vector_bits end);
#endif
