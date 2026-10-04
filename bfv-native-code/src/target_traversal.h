#ifndef BFV_TARGET_TRAVERSAL_H
#define BFV_TARGET_TRAVERSAL_H
#include <stdint.h>
uint32_t __attribute__((thiscall)) bfv_next_target_handle(void *object,
    void *scratch,volatile uint32_t argument);
#endif
