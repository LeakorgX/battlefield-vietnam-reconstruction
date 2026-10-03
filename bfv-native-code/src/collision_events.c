/* Recovered event construction and object lookups. Vector insertion executes
 * reconstructed C with a bridge to the retained compiler exception runtime. */
#include <stdint.h>
#include "target.h"
#include "collision_events.h"
#include "word_vector.h"
#define TC __attribute__((thiscall))
typedef int32_t (TC *word_count)(void *);
static uint32_t read32(uintptr_t p) { return *(volatile uint32_t *)p; }
static void write32(uintptr_t p, uint32_t value) { *(volatile uint32_t *)p = value; }
static uint32_t float_bits(float value)
{ union { float value; uint32_t bits; } v; v.value = value; return v.bits; }

uint32_t TC bfv_object_lookup(void *pool, uint32_t handle)
{
    uint32_t index = handle & 0xffffu;
    if (!index) return 0;
    uintptr_t record = read32((uintptr_t)pool) + index * 8 - 8;
    if (*(volatile uint16_t *)(record + 6) != (uint16_t)(handle >> 16)) return 0;
    return read32(record);
}

uint32_t TC bfv_event_interface(void *object, uint32_t index)
{
    /* Native LEA/test guards the table address, not the incoming object pointer. */
    uintptr_t table = (uint32_t)((uintptr_t)object + 0x24u);
    if (!table) return 0;
    return read32((uint32_t)(table + index * 4u));
}

void *TC __attribute__((noinline)) bfv_collision_construct(void *buffer, vector_bits origin, vector_bits contact,
    void *data, uint32_t kind, float timestamp, float strength, uint32_t flags)
{
    uintptr_t p = (uintptr_t)buffer;
    *(volatile uint8_t *)p = (uint8_t)kind;
    write32(p + 8, 0); write32(p + 12, 0); write32(p + 16, 0);
    write32(p + 20, origin.x); write32(p + 24, origin.y); write32(p + 28, origin.z);
    write32(p + 32, contact.x); write32(p + 36, contact.y); write32(p + 40, contact.z);
    write32(p + 44, float_bits(timestamp)); write32(p + 48, float_bits(strength));
    *(volatile uint8_t *)(p + 52) = (uint8_t)flags;
    uintptr_t manager = read32(BFV_COLLISION_EVENT_MANAGER);
    int32_t count = ((word_count)read32(read32(manager) + 0x2c))((void *)manager);
    if (count > 0) {
        uintptr_t source = (uintptr_t)data;
        do {
            uintptr_t begin = read32(p + 8), end = read32(p + 12);
            uint32_t used = begin ? (uint32_t)((int32_t)(end - begin) >> 2) : 0;
            if (begin && used < (uint32_t)((int32_t)(read32(p + 16) - begin) >> 2)) {
                write32(end, read32(source));
                write32(p + 12, end + 4);
            } else {
                bfv_vector_insert((void *)(p + 4), (uint32_t)end, 1, (void *)source);
            }
            source += 4;
        } while (--count);
    }
    return buffer;
}
