#ifndef BFV_MOVEMENT_GEOMETRY_H
#define BFV_MOVEMENT_GEOMETRY_H
#define BFV_MOVEMENT_TC __attribute__((thiscall))
float *BFV_MOVEMENT_TC bfv_vector_cross_assign(float *left,const float *right);
/* Event ID and field offsets are established; physical units remain unresolved. */
long double BFV_MOVEMENT_TC bfv_component_event2_scalar(void *component);
#endif
