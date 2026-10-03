/* BBFireArtilleryDriver rating stage. Names describe recovered roles; the
 * original engine's class declarations and source names are unavailable. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_word)(void *);
typedef void *(TC *get_view)(void *, void *);
typedef uint32_t (TC *get_entry)(void *, uint32_t);
typedef void *(TC *notify)(void *, uint32_t);
typedef void *(TC *convert_view)(void *);
typedef long double (TC *fallback_score)(void *, void *, uint32_t, float);
typedef long double (TC *artillery_score)(void *, void *, uint32_t, float,
                                        void *, void *);
typedef uint8_t (TC *test_bot)(void *, void *);
typedef void (TC *set_word)(void *, uint32_t);
volatile uint32_t bfv_artillery_calls;
volatile uint32_t bfv_artillery_evaluate_calls;
static uint32_t word(uintptr_t address)
{ return *(volatile uint32_t *)address; }
static uintptr_t method(void *object, uint32_t offset)
{ return word(word((uintptr_t)object) + offset); }
static uint32_t get(void *object, uint32_t offset)
{ return ((get_word)method(object, offset))(object); }
static uintptr_t resolve(uint32_t handle)
{
    uint32_t index = handle & 0xffff;
    if (!index) return 0;
    uintptr_t record = word(word(BFV_OBJECT_POOL)) + index * 8 - 8;
    if (*(volatile uint16_t *)(record + 6) != (uint16_t)(handle >> 16)) return 0;
    return word(record);
}

/* The evaluator's low recompute byte chooses between cache validation and a
 * new candidate search. Candidate search remains native until its complete
 * filtering, weapon calculations and target-state updates are recovered. */
long double TC bfv_artillery_evaluate(void *behavior, void *bot,
    uint32_t recompute, float scale, void *gun, void *driver_component)
{
    ++bfv_artillery_evaluate_calls;
    if ((uint8_t)recompute)
        return ((artillery_score)BFV_ARTILLERY_SCORE)
            (behavior, bot, recompute, scale, gun, driver_component);

    void *pattern = (void *)(uintptr_t)word((uintptr_t)behavior + 0x0c);
    uint8_t allowed = ((test_bot)method(pattern, 4))(pattern, bot);
    /* The original captures this table before the first index callback. */
    uintptr_t bot_table = word((uintptr_t)bot);
    uint32_t index = ((get_word)word(bot_table + 0xdc))(bot);
    if (!allowed) {
        *(volatile uint8_t *)(word((uintptr_t)behavior + 0x24) + index) = 0;
        index = get(bot, 0xdc);
        *(volatile uint32_t *)(word((uintptr_t)behavior + 0x1c) + index * 4) = 0;
        return 0.0L;
    }
    uint32_t target = word(word((uintptr_t)behavior + 8) + index * 4);
    if (resolve(target)) {
        index = get(bot, 0xdc);
        *(volatile uint8_t *)(word((uintptr_t)behavior + 0x24) + index) = 0;
        /* This slot is fetched from the table captured before get_index,
         * even if that callback replaces the bot's current table pointer. */
        bot_table = word((uintptr_t)bot);
        index = ((get_word)word(bot_table + 0xdc))(bot);
        uint32_t cached_bits = word(word((uintptr_t)behavior + 0x1c) + index * 4);
        ((set_word)word(bot_table + 0x160))(bot, cached_bits);
        index = get(bot, 0xdc);
        return *(volatile float *)(word((uintptr_t)behavior + 0x1c) + index * 4);
    }
    index = get(bot, 0xdc);
    *(volatile uint8_t *)(word((uintptr_t)behavior + 0x24) + index) = 1;
    ((set_word)method(bot, 0x78))(bot, UINT32_MAX);
    pattern = (void *)(uintptr_t)word((uintptr_t)behavior + 0x0c);
    ((test_bot)method(pattern, 8))(pattern, bot);
    index = get(bot, 0xdc);
    *(volatile uint32_t *)(word((uintptr_t)behavior + 0x1c) + index * 4) = 0;
    return 0.0L;
}

/* The native notification returns the component used by the artillery test.
 * Its result must not be replaced by the notification receiver itself. */
uint8_t TC bfv_artillery_component_test(void *component)
{
    void *owner = (void *)(uintptr_t)word((uintptr_t)component + 4);
    void *base = (void *)(uintptr_t)get(owner, 0x14);
    void *view = ((convert_view)BFV_ARTILLERY_COMPONENT_VIEW)(base);
    return (uint8_t)get(view, 0x18);
}

float TC bfv_artillery(void *behavior, void *bot, uint32_t recompute, float scale)
{
    ++bfv_artillery_calls;
    uintptr_t driver = resolve(get(bot, 0xcc));
    uint32_t entry_id = get(bot, 0xd4);
    void *driver_body = (void *)(uintptr_t)word(driver + 0x20);
    /* The original reuses the incoming bot argument as its temporary slot. */
    uint32_t view_storage = (uint32_t)(uintptr_t)bot;
    void *entries = ((get_view)method(driver_body, 0x8c))(driver_body, &view_storage);
    uint32_t gun_handle = entries
        ? ((get_entry)method(entries, 0x5c))(entries, entry_id) : UINT32_MAX;
    uintptr_t gun = resolve(gun_handle);
    uintptr_t component = gun + 0x24 ? word(gun + 0x30) : 0;
    void *receiver = (void *)(uintptr_t)word(word(component + 4) + 0x20);
    void *notified = ((notify)method(receiver, 0xa0))(receiver, 3);
    if (bfv_artillery_component_test(notified)) {
        void *driver_component = (void *)(uintptr_t)(driver + 0x24 ? word(driver + 0x2c) : 0);
        void *scorer = (void *)(uintptr_t)word((uintptr_t)behavior + 0x34);
        /* Native ST0 is explicitly rounded to float before indexing the table. */
        volatile float rating = (float)bfv_artillery_evaluate
            (scorer, bot, recompute, scale, (void *)gun, driver_component);
        uint32_t index = get(bot, 0xdc);
        *(volatile float *)(word((uintptr_t)behavior + 0x1c) + index * 4) = rating;
        scorer = (void *)(uintptr_t)word((uintptr_t)behavior + 0x34);
        uint8_t eligible = ((test_bot)method(scorer, 0x10))(scorer, bot);
        index = get(bot, 0xdc);
        *(volatile uint8_t *)(word((uintptr_t)behavior + 0x24) + index) = eligible;
    } else {
        uint32_t index = get(bot, 0xdc);
        *(volatile uint8_t *)(word((uintptr_t)behavior + 0x24) + index) = 0;
        void *fallback = (void *)(uintptr_t)word((uintptr_t)behavior + 0x30);
        volatile float rating = (float)((fallback_score)method(fallback, 0x24))
            (fallback, bot, recompute, scale);
        index = get(bot, 0xdc);
        *(volatile float *)(word((uintptr_t)behavior + 0x1c) + index * 4) = rating;
    }
    uint32_t index = get(bot, 0xdc);
    return *(volatile float *)(word((uintptr_t)behavior + 0x1c) + index * 4);
}
