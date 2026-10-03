#ifndef BFV_WORD_VECTOR_H
#define BFV_WORD_VECTOR_H
#include <stdint.h>
void __attribute__((thiscall)) bfv_vector_insert(void *vector, uint32_t position, uint32_t count, const void *value);
uint32_t __attribute__((stdcall)) bfv_vector_copy(uint32_t first, uint32_t last, uint32_t destination);
uint32_t __attribute__((stdcall)) bfv_vector_fill(uint32_t first, uint32_t count, const uint32_t *value);
void __attribute__((fastcall)) bfv_vector_shift(uint32_t first, uint32_t last, uint32_t destination_end);
void __attribute__((fastcall)) bfv_vector_fill_range(uint32_t first, uint32_t last, const uint32_t *value);
#endif
