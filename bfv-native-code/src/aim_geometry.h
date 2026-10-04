#ifndef BFV_AIM_GEOMETRY_H
#define BFV_AIM_GEOMETRY_H
#include <stdint.h>
#define BFV_AIM_TC __attribute__((thiscall))

float *BFV_AIM_TC bfv_transform_point(const float *matrix,const float *point,float *output);
float *BFV_AIM_TC bfv_vector_difference(const float *origin,float *output,const float *point);
/* The native position helpers return the world-matrix pointer in EAX, while
 * writing their three-coordinate result to the caller's output buffer. */
const float *BFV_AIM_TC bfv_aim_world_position(void *view,float *output);
const float *BFV_AIM_TC bfv_component_position(void *component,float *output);
#endif
