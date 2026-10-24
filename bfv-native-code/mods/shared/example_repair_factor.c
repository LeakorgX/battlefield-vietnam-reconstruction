/* Example mod: repair packs repair twice as fast (client and server).
 *
 * `ObjectTemplate.repairFactor <value>` in objects/HandWeapons/RepairPack/
 * Objects.con runs the console command handler
 * Cmd_ObjectTemplate_repairFactor::execute (client 00626890, server 0051fbe0;
 * readable/<target>/ref2/world/console_08f57028.c). When the command sets the
 * value ([this + 0x1c] == 1), the handler stores it as the float at
 * ObjectTemplate_repairFactor (client + 0x618, server + 0x5ec) of the active
 * HandFireArmsTemplate. This hook runs after the handler and doubles what it
 * stored. Every name below has each target's own value, so this one file
 * builds into both executables. */
#include "bfv_mod.h"

MOD_AFTER(Cmd_ObjectTemplate_repairFactor_execute, double_repair_factor)
{
    if (M32(MOD_THIS + 0x1c) != 1) return;  /* 0 = the command reads the value */
    uint32_t template = GAME_THISCALL(getActiveTemplateAs, M32(HandFireArmsTemplate_classID));
    float factor = F32(template + ObjectTemplate_repairFactor) * 2.0f;
    F32(template + ObjectTemplate_repairFactor) = factor;
    MOD_LOGF("repairFactor set to %f\n", factor);
}
