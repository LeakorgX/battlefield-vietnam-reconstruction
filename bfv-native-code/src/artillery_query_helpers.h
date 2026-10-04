#ifndef BFV_ARTILLERY_QUERY_HELPERS_H
#define BFV_ARTILLERY_QUERY_HELPERS_H
#include <stdint.h>
#define BFV_QUERY_TC __attribute__((thiscall))
uint32_t BFV_QUERY_TC bfv_component_event2_word(void *component);
void *BFV_QUERY_TC bfv_component_query_data(void *component);
void BFV_QUERY_TC bfv_query_vector_append(void *vector,const uint32_t *value);
void BFV_QUERY_TC bfv_query_vector_destroy(void *vector);
#endif
