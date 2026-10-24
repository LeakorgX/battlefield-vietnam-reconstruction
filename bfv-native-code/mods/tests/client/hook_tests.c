/* Hooks exercised by tools/test_mods.py on small client functions (not a game mod).
 *
 * 00956230  mov eax, [esp+4]; fld [ecx+eax*4+0xbc]; ret 4     (float getter)
 * 00970d00  fld [esp+4]; fadd [ecx+0x204]; fstp [ecx+0x204]; ret 4
 * 009698a0  mov eax, [esp+4]; fld [ecx+eax*4+0x94]; fadd st0, st0; ret 4 */
#include "bfv_mod.h"

uint32_t test_calls;

/* after-hook: change a float result, log */
MOD_AFTER(0x00956230, test_add_100)
{
    test_calls++;
    MOD_SET_RESULT_F(MOD_RESULT_F + 100.0f);
    MOD_LOGF("getter %u -> %f\n", MOD_ARG(0), MOD_RESULT_F);
}

/* before-hook: change an argument, or skip the function */
MOD_BEFORE(0x00970d00, test_triple_or_skip)
{
    if (MOD_ARG_F(0) < 0.0f) MOD_RETURN(0x1234);
    MOD_ARG_F(0) = MOD_ARG_F(0) * 3.0f;
}

/* replace a function with a float result computed through a game call (which
 * runs the hooked getter, after-hook included) */
MOD_BEFORE(0x009698a0, test_replace)
{
    float v = GAME_THISCALL_F(0x00956230, MOD_THIS, MOD_ARG(0));
    MOD_RETURN_F(v * 0.5f);
}

/* names from the generated headers: a virtual method by name (mod_slots.h)
 * and a field named in notes/client/names.txt (mod_fields.h)
 * 00956340  fld [ecx+0x14]; ret     (AISettings slot 3)
 * 0095af10  fld [ecx+0x1c]; ret     (AISettings slot 9, getViewDistance) */
MOD_BEFORE(0x00956340, test_named_method)
{
    float view = GAME_THISCALL_F(GAME_METHOD(MOD_THIS, AISettings, getViewDistance), MOD_THIS);
    MOD_RETURN_F(view + (float)M32(MOD_THIS + AISettings_maxNBots));
}
