/* Iterator-result wrapper used by candidate history/category search. */
#include "list_search.h"
static uint32_t word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
void *__attribute__((fastcall)) bfv_find_word_in_nodes(void *output,
    const uint32_t *key,uint32_t first,uint32_t last)
{
    /* Empty ranges do not read the key. Otherwise capture it once before the
     * traversal, then store the iterator only after all node reads. */
    if(first!=last) {
        uint32_t value=*(const volatile uint32_t *)key;
        while(word(first+8)!=value) {
            first=word(first);
            if(first==last) break;
        }
    }
    *(volatile uint32_t *)output=first;
    return output;
}
