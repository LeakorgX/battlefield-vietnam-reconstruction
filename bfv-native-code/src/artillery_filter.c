/* First candidate-pass filter. The bridge retains the native evaluator frame
 * until its remaining scoring/search phases are reconstructed. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef long double (TC *get_metric)(void *);
typedef float *(TC *history_lookup)(void *, uint32_t);
volatile uint32_t bfv_artillery_filter_calls;
static uint32_t word(uintptr_t p) { return *(volatile uint32_t *)p; }
static uintptr_t method(uintptr_t object, uint32_t offset)
{ return word(word(object) + offset); }
static void store(uintptr_t p, uint32_t value)
{ *(volatile uint32_t *)p = value; }

/* FCOMP, rather than a C ordered comparison, preserves native unordered-value
 * rules and invalid-operation flags. These small x87 primitives keep the same
 * precision, rounding mode and stack-pop behavior as the inspected block. */
static uint16_t compare_metric(long double value)
{
    uint16_t status;
    __asm__ volatile("fldt %[v]; fcomps %[zero]; fnstsw %%ax"
        : "=a"(status) : [v]"m"(value),
          [zero]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_ZERO) : "st");
    return status;
}
static uint16_t compare_history(float reference, const float *value)
{
    uint16_t status;
    __asm__ volatile("flds %[ref]; fsubs %[value]; fcomps %[limit]; fnstsw %%ax"
        : "=a"(status) : [ref]"m"(reference), [value]"m"(*value),
          [limit]"m"(*(volatile float *)BFV_ARTILLERY_FILTER_HISTORY_LIMIT) : "st");
    return status;
}

/* PUSHAD packet: EDI, ESI, EBP, incoming ESP, EBX, EDX, ECX, EAX.
 * Only EBP and the two frame fields are live outputs of this replaced phase;
 * the native accept/reject continuations overwrite EDI and volatile registers. */
uint32_t bfv_artillery_filter_phase(uint32_t *registers)
{
    ++bfv_artillery_filter_calls;
    uintptr_t frame = registers[3];
    uintptr_t behavior = registers[4];
    uintptr_t record = word(word(frame + 0x40) + 8);
    store(frame + 0x18, (uint32_t)record);
    uint32_t handle = word(record);
    uint32_t index = handle & 0xffff;
    if (!index) return 0;
    uintptr_t pool_record = word(word(BFV_OBJECT_POOL)) + index * 8 - 8;
    if (*(volatile uint16_t *)(pool_record + 6) != (uint16_t)(handle >> 16)) return 0;
    uintptr_t candidate = word(pool_record);
    registers[2] = (uint32_t)candidate;
    if (!candidate) return 0;
    uint32_t identity = ((get_word)method(candidate, 0x2c))((void *)candidate);
    if (identity == word(frame + 0xa0)) return 0;
    if (!(uint32_t)(candidate + 0x24)) return 0;
    uintptr_t component = word(candidate + 0x24);
    store(frame + 0x94, (uint32_t)component);
    if (!component) return 0;
    if (word(candidate + 4) & ((1u << 13) | (1u << 16) | (1u << 19))) return 0;
    long double metric = ((get_metric)method(component, 0x10))((void *)component);
    uint16_t status = compare_metric(metric) & 0x4100;
    if (status == 0x0100 || status == 0x4000) return 0;
    /* The metric callback may replace the record pointer in the native frame. */
    record = word(frame + 0x18);
    if (*(volatile uint8_t *)(record + 0x28) && (int32_t)word(record + 0x30) < 0) return 0;
    handle = word(record);
    const float *history = ((history_lookup)BFV_TARGET_HISTORY)((void *)behavior, handle);
    float reference = *(volatile float *)(frame + 0x58);
    status = compare_history(reference, history) & 0x0500;
    return status != 0x0100 && status != 0x0400;
}

__attribute__((naked)) void bfv_artillery_filter_bridge(void)
{
    __asm__ volatile(
        "pushal; mov %%esp,%%eax; push %%eax; call _bfv_artillery_filter_phase;"
        "add $4,%%esp; test %%eax,%%eax; jz 1f; popal; jmp %c0;"
        "1: popal; jmp %c1;"
        : : "i"(BFV_ARTILLERY_FILTER_ACCEPT), "i"(BFV_ARTILLERY_FILTER_REJECT) : "memory");
}
