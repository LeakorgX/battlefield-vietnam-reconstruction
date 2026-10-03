#ifndef BFV_MOD_RULES_H
#define BFV_MOD_RULES_H
#include <stdint.h>
/* Edit these rules, then run build.ps1. Defaults preserve native behavior. */
int bfv_plan_enabled(uint32_t native_mask, uint32_t plan_type, uint32_t object_type);
float bfv_bailout_rating(float native_rating);
float bfv_vehicle_rating(float native_rating);
#endif
