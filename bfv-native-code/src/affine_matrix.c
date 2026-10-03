/* Original affine 4x4 composition. The last column is fixed to 0,0,0,1.
 * Per-element multiplication/addition order and column-first stores are part
 * of the recovered behavior, including when output overlaps an input. */
#include <stdint.h>
#include "affine_matrix.h"
#define TC __attribute__((thiscall))

/* Three extended products, two extended sums, then one float32 output store.
 * Operand order matters for NaNs; an ordinary C dot product can reorder it. */
#define PRODUCTS \
    "mov 0(%[p]),%%eax; flds (%%eax); mov 4(%[p]),%%eax; fmuls (%%eax);" \
    "mov 8(%[p]),%%eax; flds (%%eax); mov 12(%[p]),%%eax; fmuls (%%eax); faddp;" \
    "mov 16(%[p]),%%eax; flds (%%eax); mov 20(%[p]),%%eax; fmuls (%%eax); faddp;"
static void element(float *output, const float *a, const float *b,
    const float *c, const float *d, const float *e, const float *f,
    const float *translation)
{
    _Static_assert(sizeof(void *) == 4, "Original x86 matrix primitive requires 32-bit pointers");
    const float *pointers[] = { a, b, c, d, e, f, translation, output };
    if (translation) {
        __asm__ volatile(PRODUCTS "mov 24(%[p]),%%eax; fadds (%%eax);"
            "mov 28(%[p]),%%eax; fstps (%%eax)"
            : : [p]"r"(pointers) : "eax", "st", "memory");
    } else {
        __asm__ volatile(PRODUCTS "mov 28(%[p]),%%eax; fstps (%%eax)"
            : : [p]"r"(pointers) : "eax", "st", "memory");
    }
}

float *TC bfv_affine_compose(float *output, const float *left, const float *right)
{
    for (uint32_t column = 0; column != 3; ++column) {
        for (uint32_t row = 0; row != 4; ++row) {
            const float *l = left + row * 4;
            const float *r = right + column;
            const float *translation = row == 3 ? right + 12 + column : 0;
            float *destination = output + row * 4 + column;
            if (column == 0 && row == 0)
                element(destination, l, r, l + 1, r + 4, l + 2, r + 8, translation);
            else if (column == 1 && row == 0)
                element(destination, l + 1, r + 4, r, l, r + 8, l + 2, translation);
            else if (column == 2)
                element(destination, l, r, r + 8, l + 2, r + 4, l + 1, translation);
            else
                element(destination, l + 2, r + 8, l + 1, r + 4, l, r, translation);
        }
    }
    output[3] = 0.0f;
    output[7] = 0.0f;
    output[11] = 0.0f;
    output[15] = 1.0f;
    return output;
}
