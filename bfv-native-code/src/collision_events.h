#ifndef BFV_COLLISION_EVENTS_H
#define BFV_COLLISION_EVENTS_H
#include "collision_geometry.h"
void *__attribute__((thiscall)) bfv_collision_construct(void *buffer, vector_bits origin, vector_bits contact,
    void *data, uint32_t kind, float timestamp, float strength, uint32_t flags);
uint32_t __attribute__((thiscall)) bfv_object_lookup(void *pool, uint32_t handle);
uint32_t __attribute__((thiscall)) bfv_event_interface(void *object, uint32_t index);
#endif
