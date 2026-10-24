# AI: how bots decide, and where to change it

A modder's map of the bot AI in `BfVietnam.exe`. It covers the data files, the
decision formulas recovered from the code, and the addresses to patch. All
addresses are client VAs (image base 0x400000).

Each statement is marked as one of:
- **Verified:** read from the disassembly.
- **Data:** read from shipped `.con` files.
- **Inferred:** behaviour that is likely but not yet traced.

## 1. Architecture

```
SAI (strategic AI, per team)          Bot (per AI soldier/vehicle)
  Strategies.con / conditions.con       BotMain  (behaviour selection)
  prerequisites.con                       |- weights  bot+0x60[]
  StrategicAreas.con (areas, groups)      |- UrgencyMerger bot+0xac
        |  orders (areas to take)          |- behaviours bot+0x74[] (BB*)
        v                                  |- current plan bot+0x78 (BBP*)
  AIBotManager (LOD, scheduling) -------->  \- interpreter (BAPST* + entries)
```

- **Behaviours (BB\*)** score how urgent they are.
- **Plan generators (BBP\*)** turn the winning behaviour into a plan.
- The **interpreter** runs the plan as a tree of steps (`BAPSTSerial`,
  `BAPSTParallel`, `BAPSTIf`, `BAPSTWait`, ...). The leaves are per-vehicle
  *interpreter entries* (`TankMoveTo`, `MouseTurretAimAt`, `Sense`, `InfoWrapper`, ...).

Globals (verified):

| Address | Object |
| --- | --- |
| 0xe0eb20 | IAISettings (vtable 0xbf1c94) |
| 0xdfa8d0 | IAISettingsModifier (vtable 0xbf1c24, subobject at +4 of AISettings, ctor 0x9568e0) |
| 0xdfa8c8 | IBotManager |
| 0xe0ef50 | Timer |
| 0xe0cd58 | object handle table |
| 0xd30c00 | Random |
| 0xe0eea8 | basic bot weights (`setBasicBotWeights`) |
| 0xe0eeac | byte: number of behaviours |
| 0xe0f670 | 101-float AI curve table |

## 2. Data files

Global AI data lives in `Mods/BfVietnam/Archives/BfVietnam/game.rfa`, under
`BfVietnam/game/AI/`. Per-map AI data is under `levels/<map>/` (`AI.con`,
`AIPathFinding.con`, `ai/StrategicAreas.con`, `Strategies.con`,
`conditions.con`, `prerequisites.con`). **Data.**

### 2.1 AIbehaviours.con

The 8 behaviour slots (0–7) are Avoid, MoveTo, Idle, Fire, Scout, TakeCover,
Change and Special.

The 20 vehicle types (0–19) are Tank, Plane, Boat, Infantery, Fixed, Car,
UnarmedTank, LandingCraft, Passenger, BoatFixed, LandingCraftPassenger,
LandingCraftFixed, UnarmedBoat, FixedLargeBore, ArtilleryDriver, Heli,
Amphibius, FreePassenger, LandingCraftFreePassenger and HeliUnarmed.

```
aiSettings.setVehicleBehaviour <Vehicle> <Behaviour> <UrgencyGen BB*> <PlanGen BBP*> <Prio> <ParallelMask> <UrgencyCurve> <Inhibitors>
```

- An urgency generator of `NULL` leaves the slot empty: that behaviour is never
  chosen (verified, 0x942940).
- An unknown name prints `Error: Behaviour '...' doesn't exist` (0x941720).
- `ParallelMask` is a 32-bit mask.
- `Inhibitors` names a `createBehaviourModifiers` set. Its 8 floats become this
  behaviour's inhibitor array (BotBehaviour+0x20). See section 3.

Shipped plan names that are **not** registered plan generators:
- `BBPFireDriveAttack` (Car Fire);
- `UnRelated` (Fixed MoveTo, HeliUnarmed Fire).

Those slots can score but produce no plan.

The file warns: "NEVER ALLOW IDLE's urgency to become 0. The AI will CRASH."
Keep Idle above 0 in every weight set and inhibitor set (the shipped value is 0.01).

### 2.2 Weight sets (`createBehaviourModifiers`)

| Set | Avoid | MoveTo | Idle | Fire | Special | Scout | TakeCover | Change |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| UnitWeights (basic, also default inhibitor) | 1.0 | 1.0 | 0.01 | 9.6 | 2.0 | 1.0 | 1.0 | 1.0 |
| StandardWeights (personality, teams 1 and 2) | 1.0 | 1.0 | 0.01 | 9.7 | 2.0 | 1.0 | 1.0 | 1.0 |
| PlaneWeights | 0.5 | 1.0 | 0.1 | 1.8 | 1.0 | 1.0 | 0.0 | 1.0 |
| AvoidInhibit | 1.0 | 0.3 | 1.0 | 2.0 | 0.5 | 1.0 | 1.0 | 1.0 |
| ChangeInhibit | 0.8 | 1.0 | 0.01 | 2.0 | 0.0 | 0.5 | 1.0 | 7.5 |
| ControlPointEvac | 0.3 | 1.8 | 0.01 | 3.5 | 2.0 | 0.5 | 0.0 | 1.0 |
| FireunarmedWeights | 1.0 | 1.5 | 0.1 | 9.5 | 1.0 | … | … | … |

- `setBasicBotWeights UnitWeights` fills 0xe0eea8 (via 0x970680).
- `setStandardPersonality StandardWeights <team>` sets the per-bot personality
  array (bot+0x44, inferred).
- `setStandardMoralAdmin default <team>`: see section 5.

### 2.3 Urgency curves

```
aiSettings.createUCConstant  UCUnion 1.0
aiSettings.createUCLinear    UCFire  -0.22 1.3        # y = -0.22*t + 1.3
aiSettings.createUCXInverse  UCScout 2.5 0.9 1.0 0.5  # y = 2.5/(t+0.9) + 0.5
```

The curve input `t` is **the number of seconds the behaviour has been active**
(verified, 0x9dc720). The curve applies only to the behaviour that is currently
running. Every other behaviour uses 1.0. Effects:

- **UCFire:** 1.3 at the moment firing starts, 1.0 after about 1.4 s, and 0 at
  about 5.9 s. So a bot's Fire behaviour fades out about 6 s into an engagement.
  Fire must then win again from scratch (with curve 1.0) to restart.
- **UCScout:** about 3.3 at the start, decaying toward 0.5. It is defined but
  unused in the shipped vehicle setups.
- **UCUnion:** no time effect.

## 3. Behaviour selection (BotMain::vf4, 0x9760c0), verified

Each decision tick:

1. **Weights** (BotMain 0x9706c0, recomputed when morale level or settings change):
   ```
   w[i] = (moraleFlag ? personality[i] * moraleWeights[i] : moraleWeights[i])
          * basicWeights[i] * bot+0x1c[i]
   ```
   The result is stored in bot+0x60[i]. The personality array is bot+0x44 and
   the morale weights are bot+0x48. The meaning of bot+0x1c is not yet
   identified; it is probably a per-unit or per-vehicle factor.
2. **Effective weight** of each behaviour i:
   ```
   W_i = bot+0x60[i] * curve_active(t_active) * inhibitors_of_active[i]
   ```
   - The curve factor applies only when i is the active behaviour (see 2.3).
   - `inhibitors_of_active` is the `Inhibitors` set of the **currently running**
     behaviour. For example, while Change (ChangeInhibit) runs, Special is
     multiplied by 0.0 and Change by 7.5, which makes the bot stick to changing
     vehicles.
3. If `W_i > 0`, it calls `behaviour_i->evaluate(bot, 1, W_i)` (vf9). Each BB\*
   computes a raw score in [0, 1+] from its senses, then:
   ```
   urgency_i = curveTable(2 * score) * extra * W_i
   ```
   This is the BBFire tail at 0x996a08. Other BB\* classes follow the same
   pattern; their score terms are not yet traced.
4. **Hysteresis:** unless a re-decision is forced (bot+0x14), the merger only
   re-ranks when some behaviour's urgency changed by more than ×0.87 or ×1.15
   since the last decision. These are hard-coded immediates 0x3f5eb852 and
   0x3f933333 in `merger->vf6(bot, 0.87, 1.15)`.
5. **Winner** = the highest urgency (merger vf3/vf7). The winner's index and
   start time go into bot+0x4c. Then `winner->generatePlan(bot)` (vf10) runs.
   A new plan replaces bot+0x78, and the old plan is deleted.

### Curve table (0x9d6540, filled at 0x9d5f60)

```
x <= 0 -> 0;  x >= 1 -> 1;  else table[int(x*100)]   (no interpolation)
```

Samples at x = 0, 0.05, …, 1.0:

```
0 .146 .292 .439 .579 .684 .754 .805 .842 .871 .895 .914 .930 .943 .955 .965 .974 .981 .988 .994 1
```

The curve is linear (slope about 2.9) up to x ≈ 0.18, then saturates. Because
the input is `2*score`, any score above about 0.5 is already at full urgency.

## 4. Fire behaviour and weapons, verified

On the active `WeaponTemplate` (AI weapon template):

| Offset | Field (console name) |
| --- | --- |
| +0x18 | precision |
| +0x1c | charge flag |
| +0x20 | ChargePossibility (charge roll at 0x93cbc0, chance clamped to [0, 1]) |
| +0x28 / +0x2c | minRange / maxRange |
| +0x38 | exitVelocity (used for lead) |
| +0x40 | deviation (getter 0x93cc90; ×1.5 when a global mode flag is set) |
| +0x44 | deviationCorrectionTime |

- Aim error over time (0x9ea9d0):
  ```
  deviation * max(0, (1-a)*T - a*(now - t0))
  ```
  Here T is deviationCorrectionTime. The error starts large and shrinks to 0 as
  the bot "corrects" its aim.
- WeaponFireArm::vf30 (0x9eaa80) writes the real weapon's spread (+0x208):
  ```
  f + (1 - 0.75a)*b
  ```
  It is optionally divided by `2(a-0.25)+0.25`. The caller (which gives the
  meaning of a and b) has not been found yet. a is probably skill-related.
- IAISettings +0xbc holds 3 floats {1, 3, 8}. They are read by the BBFire
  family (vf31/vf32); they are likely range or zoom bands.
- Target switching: a new target must score more than ×1.2 the current one.
- The Aimer at 0xa02a70–0xa02db0 is a turn-rate controller (constants 60, 5, 8,
  2, ±1000), not accuracy.

## 5. Morale (MoralAdmin), verified, dormant

- BotMain::vf6 (0x9706a0) does `morale += x * bot+0x20`, stored at bot+0x3c.
- The admin at bot+0x84 (0x9aff20) picks a level from its thresholds. On a level
  change it installs that level's weight array (bot+0x48) and flag (+0x50), then
  recomputes the weights.
- The only admin is the built-in `default` (0x947190): 1 level, threshold 1.0,
  all weights 1.0. No console command creates more levels.

So **morale has no effect in the shipped game**. Making it matter needs an EXE
change, either a new admin with several levels or a patch to 0x947190.

## 6. Global settings (IAISettings, 0xe0eb20), verified

| Getter | Field | Default | Console |
| --- | --- | ---: | --- |
| vf9 | +0x1c | 600 | view distance (`setViewDistance`; Ia_Drang AI.con sets 405) |
| vf10 | +0x20 | 512 | stats view distance |
| vf13 | +0x2c | 32 | int (grid or count) |
| vf15 | +0x24 | 0.75 | **bot skill** (`aiSettings.setBotSkill`) |
| vf16 | +0x28 | 35 | SAI update frequency |
| vf31/32 | +0xbc[3] | 1, 3, 8 | Fire-family floats |

### Bot skill

Bot skill is read in at least two places:

- **InfoWrapper / sensing roll** (EntryInfoWrapper::execute 0x9b2fc0, roll at
  about 0x9b327d):
  ```
  v = (skill+1) * 0.5 * (value/ratio) * 0.1 - rand
  ```
  - It fails if `v <= 0` or `rand >= 0.9`. **Even skill 1.0 fails at least 10% of
    the time.**
  - On failure, the target point is rotated by a random angle in [-90°, +90°]
    and pushed out by up to 4 units. Success and failure are counted per entry.
- **BotManager** (0x9c17c9): `(skill+1) * 0.1 * x`.

Raising skill from 0.75 to 1.0 raises the factor (skill+1) from 1.75 to 2.0, so
only by about 14%. Values above 1 are not clamped in these formulas (untested in
game).

## 7. Bot manager scheduling (per-map AI.con), data

From Ia_Drang:

```
AIBotManager.setLodLevelTicks 6 6 6
AIBotManager.setLodLevelPriority 6 4 2
AIBotManager.setPlannedDecisionMakingThreshold   0.5 1.0 3.0
AIBotManager.setUnplannedDecisionMakingThreshold 0.3 0.8 2.0
AIBotManager.setDecisionMakingInterleave 2 2
AIBotManager.setSensingQuotient 2 1
AIBotManager.setSystemQuotient 30 40 30
```

The three values are per LOD level (near, mid and far bots). The thresholds are
inferred to be seconds between decisions, with or without a plan running. Lower
values give faster reactions and cost more CPU. How BotManager consumes them is
not yet traced.

## 8. Strategic AI (SAI), data

- **Strategies.con** defines test1, assault, allOutAttack, allDefence, default
  and airToNaval. Each one sets:
  - `Aggression` and `NumberOfAttacks/Defences`;
  - `setPrerequisite` and `setWeight`;
  - `setTreeModifier TTN*` and `setTreeModifierCombatStrength`;
  - `setStrategicObjectsModifier` (ControlPoint, Flank, Neutral, Base).

  The map selects a strategy with `ai.addSAIStrategy <side> <name>`.
- **conditions.con:**
  - Constant, Homogenous and Area conditions;
  - Crisp or Fuzzy, with QuotientGreater/Smaller over Ticket, Naval or Air
    values;
  - Required or Advisory, with a TargetValue.
- **prerequisites.con:** `addCondition <cond> <weight>`.
- **radio.con:** message subscriptions with timeouts. `playerRadioModifier 3.0`
  makes player radio requests 3× as weighty as bot ones.
- **StrategicAreas.con:**
  - `aiStrategicArea.create name x/y x/y value category`;
  - `setActive`, `addNeighbour`, `addAllowedVehicleGroup`, `addObjectTypeFlag`,
    `setOrderPosition`, `vehicleSearchRadius`, `playerSensitive*`;
  - `createVehicleGroup` and `addVehicleToVehicleGroup` (vehicle index 0–19);
  - **`setTakeable <team> <0|1>` where team must be 1 or 2.** Team 0 corrupts
    the area object and crashes the host (handler 0x906c20 writes
    `area[team+0x18b]`).
- **AIPathFinding.con:**
  ```
  ai.addSearchMap name waterHeight waterDepth maxSlope brush lowClip hiClip considerAITypes secondLayer levels
  ```
  It also uses `addSearchType`, `setMapSpawnPoints` and `setSmoothing`.

## 9. Modding recipes

| Goal | Change | Where |
| --- | --- | --- |
| Bots keep firing longer | Flatten UCFire, e.g. `createUCLinear UCFire -0.08 1.3` (zero at about 16 s), or use `UCUnion` for Fire | AIbehaviours.con |
| Bots fire less, objectives more | Lower Fire in UnitWeights or StandardWeights (9.6/9.7); raise MoveTo | AIbehaviours.con |
| Per-team personality | Make a new weight set, then `setStandardPersonality <set> <team>` | AIbehaviours.con |
| Infantry takes cover more | Raise TakeCover in StandardWeights, or the TakeCover prio | AIbehaviours.con |
| Fewer vehicle swaps | Lower Change in ChangeInhibit (7.5) | AIbehaviours.con |
| Car drivers attack | Replace `BBPFireDriveAttack` with a registered generator (e.g. `BBPFire2d`); untested | AIbehaviours.con |
| Disable a behaviour for a vehicle | Use urgency generator `NULL` (never on Idle) | AIbehaviours.con |
| Accuracy | Per weapon: deviation, deviationCorrectionTime, precision in the AI weapon templates. Globally: `aiSettings.setBotSkill` | weapon AI .con, AIbehaviours.con |
| Faster reactions | Lower the DecisionMakingThreshold values | map AI.con |
| More or less decisive switching | Patch 0.87/1.15 (closer to 1 = twitchier) | EXE `merger->vf6` immediates in 0x9760c0 |
| Different urgency shape | Patch the table fill | EXE 0x9d5f60 |
| Remove the 10% skill failure | Patch the 0.9 constant | EXE: the 0.9 at 0xb69400 is read by 28 instructions, so redirect only the roll at about 0x9b327d to a new constant |

## 10. Open questions

- The individual score terms of each BB\* evaluate (Fire is partly traced).
- The caller of WeaponFireArm::vf30, and the meaning of a and b.
- The meaning of bot+0x1c and of IAISettings +0xbc.
- How BotManager uses the LOD thresholds and quotients.
- Plan generator internals (BBP\*) and the interpreter entry parameters.
