#ifndef BFV_LIST_SEARCH_H
#define BFV_LIST_SEARCH_H
#include <stdint.h>
void *__attribute__((fastcall)) bfv_find_word_in_nodes(void *output,
    const uint32_t *key,uint32_t first,uint32_t last);
#endif
