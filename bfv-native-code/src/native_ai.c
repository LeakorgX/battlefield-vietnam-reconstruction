/* Reconstructed x86 AI interfaces. Build against the inspected game image.
 * The interpreter and bailout control flow are implemented below. Bailout math
 * is reconstructed in numeric_curves.c; vehicle scoring remains native.
 * Byte offsets are recovered ABI fields, not guessed original struct names.
 */
#include <stdint.h>
#include "target.h"
#include "mod_rules.h"
#include "numeric_curves.h"
#define TC __attribute__((thiscall))

typedef uint32_t (TC *get_u32)(void *);
typedef void (TC *call_void)(void *);
typedef void (TC *call_pointer)(void *, void *);
typedef void (TC *call_event)(void *, uint32_t);
typedef uint8_t (TC *fallback_plan)(void *, void *, uint8_t *, uint8_t *);
typedef uint8_t (TC *special_plan)(void *, void *, void *, uint8_t *, uint8_t *);
typedef void *(TC *context_record)(void *);
typedef void (TC *context_notify)(void *, void *, void *, uint32_t, uint32_t);
typedef float (TC *native_rating)(void *, void *, uint32_t, float);
/* Native x87 callees can return an unrounded ST0. Keep extended precision until
 * the original code's explicit float stores, including numeric-call arguments. */
typedef long double (TC *get_metric)(void *);
typedef uint8_t (TC *test_group)(void *, uint32_t);

volatile uint32_t bfv_interpreter_calls;
volatile uint32_t bfv_bailout_calls;
volatile uint32_t bfv_vehicle_calls;

static uint32_t read32(uintptr_t p) { return *(volatile uint32_t *)p; }
static uint16_t read16(uintptr_t p) { return *(volatile uint16_t *)p; }
static uintptr_t method(void *object, uint32_t offset)
{ return read32(read32((uintptr_t)object) + offset); }
static uint32_t get(void *object, uint32_t offset)
{ return ((get_u32)method(object, offset))(object); }

static uintptr_t resolve_handle(uint32_t handle)
{
    uint32_t index = handle & 0xffff;
    if (!index) return 0;
    uintptr_t record = read32(read32(BFV_OBJECT_POOL)) + index * 8 - 8;
    if (read16(record + 6) != (uint16_t)(handle >> 16)) return 0;
    return read32(record);
}

uint8_t TC bfv_interpret(void *interpreter, void *plan, void *object,
                         uint8_t *output_a, uint8_t *output_b)
{
    ++bfv_interpreter_calls;
    uint32_t trace = (read32(BFV_TRACE_INDEX) + 1) & read32(BFV_TRACE_MASK);
    *(volatile uint32_t *)BFV_TRACE_INDEX = trace;
    *(volatile uint32_t *)(BFV_TRACE_FILES + trace * 4) = BFV_TRACE_SOURCE;
    *(volatile uint32_t *)(BFV_TRACE_LINES + trace * 4) = 118;
    if ((uint8_t)get(object, 0x134)) {
        *output_b = 0;
        *output_a = 1;
        return 1;
    }
    uint32_t plan_type = get(plan, 0x0c);
    if (get(object, 0x13c)) {
        void *context = (void *)(uintptr_t)get(object, 0x13c);
        ((call_void)method(context, 0x0c))(context);
    }
    void *context = (void *)(uintptr_t)get(object, 0x13c);
    if (context) {
        ((call_pointer)BFV_CONTEXT_BEGIN)(context, plan);
        uintptr_t notify = method(context, 4);
        uint32_t selector = get(object, 0xdc);
        void *record = ((context_record)BFV_CONTEXT_RECORD)(context);
        ((context_notify)notify)(context, record, plan, 0, selector);
    }
    uint32_t object_type = get(object, 8);
    uint32_t mask = read32(read32((uintptr_t)interpreter + 8) + object_type * 4);
    uint8_t result;
    if (!bfv_plan_enabled(mask, plan_type, object_type)) {
        uintptr_t entity = resolve_handle(get(object, 0xcc));
        if (entity + 0x24 && read32(entity + 0x2c)) {
            uintptr_t component = read32(entity + 0x2c);
            void *receiver = (void *)(uintptr_t)read32(read32(component + 4) + 0x20);
            ((call_event)method(receiver, 0xa0))(receiver, 2);
            component = read32(entity + 0x30);
            if (component) {
                receiver = (void *)(uintptr_t)read32(read32(component + 4) + 0x20);
                ((call_event)method(receiver, 0xa0))(receiver, 3);
            }
        }
        *output_b = 1;
        *output_a = 0;
        result = 0;
    } else {
        uintptr_t rows = read32((uintptr_t)interpreter + 0x0c);
        uintptr_t row = read32(rows + plan_type * 4);
        void *handler = (void *)(uintptr_t)read32(row + object_type * 4);
        if (!handler)
            result = ((fallback_plan)method(plan, 0))(plan, object, output_a, output_b);
        else
            result = ((special_plan)method(handler, 4))(handler, plan, object, output_a, output_b);
    }
    if (context) {
        uintptr_t notify = method(context, 4);
        uint32_t selector = get(object, 0xdc);
        void *record = ((context_record)BFV_CONTEXT_RECORD)(context);
        ((context_notify)notify)(context, record, plan, 1, selector);
        if (*output_b || !*output_a || !result)
            ((call_pointer)BFV_CONTEXT_END)(context, plan);
    }
    return result;
}

float TC bfv_bailout(void *behavior, void *object, uint32_t recompute, float scale)
{
    ++bfv_bailout_calls;
    if ((uint8_t)recompute) {
        uintptr_t entity = resolve_handle(get(object, 0xcc));
        float influence = 1.0f;
        uint32_t group = *(volatile uint8_t *)((uintptr_t)behavior + 4);
        if (((test_group)method(object, 0x70))(object, group) &&
            (uint8_t)get(object, 0x6c)) {
            uintptr_t record = get(object, 0xe0);
            influence = *(volatile float *)(record + 8);
        }
        /* Preserve the inspected routine's component lookup, including its
         * exceptional address-wrap branch. Null/invalid handles are not fixed
         * here: doing so would change original behavior. */
        void *component = (void *)(uintptr_t)(entity + 0x24 ? read32(entity + 0x24) : 0);
        long double metric = ((get_metric)method(component, 0x10))(component);
        volatile float deficit = 1.0L - metric;
        long double shaped = bfv_bailout_curve(deficit);
        volatile float curve_input = shaped * (long double)influence * 100.0L;
        long double score = bfv_bailout_score(curve_input);
        volatile float stored = score * (long double)scale;
        uint32_t cache_selector = get(object, 0xdc);
        uintptr_t ratings = read32((uintptr_t)behavior + 0x1c);
        *(volatile float *)(ratings + cache_selector * 4) = stored;
        /* BBPPattern slot 3 sets its selector flag after storing the rating.
         * Query the selector again, as the native routine does. */
        uintptr_t pattern = read32((uintptr_t)behavior + 0x0c);
        uint32_t selector = get(object, 0xdc);
        *(volatile uint8_t *)(read32(pattern + 4) + selector) = 1;
    }
    uint32_t return_selector = get(object, 0xdc);
    float rating = *(volatile float *)(read32((uintptr_t)behavior + 0x1c) + return_selector * 4);
    return bfv_bailout_rating(rating);
}

float TC bfv_vehicle(void *behavior, void *object, uint32_t recompute, float scale)
{
    ++bfv_vehicle_calls;
    /* BBChange performs candidate evaluation even when argument 2 is zero.
     * Preserve that complete native computation before applying editable rules. */
    float rating = ((native_rating)BFV_NATIVE_VEHICLE)(behavior, object, recompute, scale);
    return bfv_vehicle_rating(rating);
}
