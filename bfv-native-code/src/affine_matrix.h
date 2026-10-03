#ifndef BFV_AFFINE_MATRIX_H
#define BFV_AFFINE_MATRIX_H
/* Sixteen row-major floats; row 3 is translation, column 3 is homogeneous.
 * Preserve native column-first writes if buffers overlap. */
float *__attribute__((thiscall)) bfv_affine_compose(float *destination,
    const float *left, const float *right);
#endif
