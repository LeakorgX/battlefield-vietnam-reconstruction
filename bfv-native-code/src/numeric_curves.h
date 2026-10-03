#ifndef BFV_NUMERIC_CURVES_H
#define BFV_NUMERIC_CURVES_H

/* The inspected helpers return unrounded x87 ST0 and consume one float argument. */
long double __attribute__((stdcall)) bfv_bailout_curve(float value);
long double __attribute__((stdcall)) bfv_bailout_score(float value);

#endif
