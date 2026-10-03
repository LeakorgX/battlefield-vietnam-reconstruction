#ifndef BFV_HISTORY_TREE_H
#define BFV_HISTORY_TREE_H
#include <stdint.h>
#define BFV_TC __attribute__((thiscall))

/* Native nodes use 32-bit links, even when the research tools run on a 64-bit host. */
typedef struct { uint32_t handle, timestamp; } bfv_history_value;
typedef struct { uint32_t node; uint8_t inserted, padding[3]; } bfv_history_result;
bfv_history_result *BFV_TC bfv_history_insert(void *map, bfv_history_result *result,
                                            const bfv_history_value *value);
uint32_t *BFV_TC bfv_history_insert_node(void *map, uint32_t *result,
    uint32_t left, uint32_t parent, const bfv_history_value *value);
void BFV_TC bfv_tree_previous(uint32_t *iterator);
void BFV_TC bfv_tree_rotate_left(void *map, uint32_t pivot);
void BFV_TC bfv_tree_rotate_right(void *map, uint32_t pivot);
void *BFV_TC bfv_tree_construct(void *node, uint32_t left, uint32_t parent,
                              uint32_t right, const bfv_history_value *value, uint32_t color);
#endif
