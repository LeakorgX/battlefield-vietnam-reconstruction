/* Artillery evaluator's handle -> timestamp map. Red/black insertion and
 * balancing remain native; unsigned lookup and timestamp construction are C. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef void *(*allocate_word)(uint32_t);
typedef struct { uint32_t handle, timestamp; } history_value;
typedef struct { uint32_t node; uint8_t inserted; uint8_t padding[3]; } insert_result;
typedef void *(TC *insert_value)(void *, insert_result *, const history_value *);
volatile uint32_t bfv_target_history_calls;
static uint32_t word(uintptr_t address)
{ return *(volatile uint32_t *)address; }

/* Nodes have left/parent/right links at 0/4/8, key at 12, value pointer at 16,
 * and a sentinel byte at 21. The map's header pointer is at byte offset 4. */
static uintptr_t find(uintptr_t map, uint32_t handle)
{
    uintptr_t candidate = word(map + 4);
    uintptr_t node = word(candidate + 4);
    while (!*(volatile uint8_t *)(node + 21)) {
        if (word(node + 12) < handle) node = word(node + 8);
        else { candidate = node; node = word(node); }
    }
    uintptr_t header = word(map + 4);
    if (candidate == header || handle < word(candidate + 12)) return header;
    return candidate;
}

float *TC bfv_target_history(void *behavior, uint32_t handle)
{
    ++bfv_target_history_calls;
    uintptr_t map = (uintptr_t)behavior + 0x38;
    uintptr_t node = find(map, handle);
    if (node != word(map + 4)) return (float *)(uintptr_t)word(node + 16);
    void *timestamp = ((allocate_word)BFV_VECTOR_ALLOCATOR)(4);
    *(volatile uint32_t *)timestamp = 0xc61c4000; /* float32 -10000 */
    history_value value = { handle, (uint32_t)(uintptr_t)timestamp };
    insert_result result;
    ((insert_value)BFV_TARGET_HISTORY_INSERT)((void *)map, &result, &value);
    /* Native code returns this allocation even if insertion reports a duplicate.
     * Do not replace it by result.node's value or silently free it. */
    return timestamp;
}
