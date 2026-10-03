#include "mod_rules.h"

int bfv_plan_enabled(uint32_t native_mask, uint32_t plan_type, uint32_t object_type)
{
    (void)object_type;
    return (native_mask & (UINT32_C(1) << (plan_type & 31))) != 0;
}

float bfv_bailout_rating(float native_rating)
{
    return native_rating;
}

float bfv_vehicle_rating(float native_rating)
{
    return native_rating;
}
