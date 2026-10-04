#ifndef BFV_REGION_GEOMETRY_H
#define BFV_REGION_GEOMETRY_H
#include <stdint.h>
#define BFV_REGION_TC __attribute__((thiscall))
#define BFV_REGION_FC __attribute__((fastcall))
void *BFV_REGION_FC bfv_point_xz(const void *point,void *output);
uint8_t BFV_REGION_FC bfv_point_in_symmetric_bounds(const float *point,
    const float *bound,const float *center);
uint8_t BFV_REGION_TC bfv_region_contains_point2(void *region,const float *point);
uint8_t BFV_REGION_TC bfv_region_contains_point3(void *region,const void *point);
#endif
