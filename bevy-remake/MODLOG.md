# Battlefield Vietnam / Bevy: parity work

## Accepted requirement, 2026-10-03

The user requires a 1:1 match to **original unmodified Battlefield Vietnam**, with
multiplayer prioritized. The earlier procedural conquest implementation does not
satisfy that requirement. Its screenshots and simulation tests establish only that
the prototype works; they provide no evidence of parity with Battlefield Vietnam.

## Route

Read-only native x86 binary analysis and local RFA data import, followed by engine
behavior recovery and original-game comparison. Disassembly is evidence for native
behavior, not automatically recovered source code. No binary patches are part of
this work. Local game data and disassembly go in the sibling `bfv-reference-local`
directory, outside the Bevy source project.

## Baseline concerns

- `Mods/BfVietnam/init.con` advertises 1.21, but the executables expose no file or
  product version through the Windows version API. This does not prove their stock
  version or originality.
- Existing `bfv-bot-commander/MODLOG.md` documents modifications to game.rfa,
  objects.rfa and Oblivion.rfa. Backups labelled original exist for these archives.
- Oblivion is the installed selected custom map; it is not the vanilla parity target.
  Ia_Drang is the first stock-map candidate for import.
- Backup labels alone cannot certify authenticity. Record hashes and provenance;
  do not silently call local files verified stock files.

## First reference tools

`tools/reference_audit.py` inventories RFA directories with bounded parsing,
substitutes the local original-labelled backups for modified gameplay archives,
extracts data into the sibling reference directory using the existing BGA tool,
and records file hashes, source-line gameplay configuration and object templates.
Its binary analysis records PE sections/imports, RTTI names, embedded source paths,
command-registration candidates and targeted x86 string-reference disassembly.

## Acceptance gates

1. Verify the exact vanilla version and content corpus.
2. Import an original map's geometry, placements, materials and terrain without
   procedural stand-ins; resolve every missing asset explicitly.
3. Recover infantry/weapon/vehicle/physics/UI rules from data and native handlers.
4. Compare repeatable input/state/render traces with the original game.
5. Implement original multiplayer semantics and validate multi-client behavior.
6. Claim a subsystem matches only when the comparison passes. A visual resemblance,
   successful compilation or a test written solely against new code is insufficient.

## User correction: binary-only focus

Stop asset conversion and archive work. Focus on native gameplay, physics,
networking and AI behavior in the client and dedicated-server executables.

Ghidra 12.1.4 and JDK 21 were already installed. Native analysis uses a separate
Ghidra project in the local reference directory. `tools/binary_logic.py` recovers
MSVC RTTI complete-object locators and class-linked virtual function tables.
`tools/ghidra/ExportNativeLogic.java` exports resolved native function bodies and
their call relationships. No original binary is patched or launched by these tools.

Initial client index: 242 AI class candidates, 132 gameplay class candidates,
2,165 distinct function seeds (1,092 associated with AI).
Initial dedicated-server index: 240 AI class candidates, 127 gameplay candidates.
Addresses in previous ad hoc notes are treated as leads, not trusted function names.
RTTI-derived class association and Ghidra function resolution are recorded separately.

Completed binary-focused recovery: 25,507 client and 16,084 dedicated-server
detected function bodies exported to the local reference directory. Two client
functions and one server function failed C decompilation; all three have assembly
listings. Both complete Ghidra projects are saved. Class indexes link AI, networking,
physics and other gameplay RTTI entries to their native exports.

Unicorn executes the original code for 192 cases per binary (384 total): 32 exact
cached-rating float32 selections and 160 plan-interpreter routing/flags/stack
checks. These use controlled external virtual-call stubs and null interpreter
context; they do not establish complete AI, multiplayer or whole-game parity.

Assembly review corrected two ABI assumptions in the analysis notes: interpreter
fallback dispatch receives three arguments; BBBailOut slot 9 receives three stack
arguments, including a float multiplier omitted by the initial C export. A false
function target in read-only data was rejected by requiring executable-section
membership. Corrected client seed count is 2,164 (1,091 AI); unresolved seeds: zero.

`bfv-reference-local/native-logic/RECOVERY.md` records addresses, verified native
paths, limitations and remaining reconstruction work. This is a binary recovery
milestone, not a buildable open-source engine or completed 1:1 remake.

User requested an EXE compilation test. Installed 32-bit MinGW GCC was used to
compile the recovered BAPInterpreter function as the first object-build stage.
Compilation failed (exit 1) on Ghidra-only types and unresolved engine globals;
the exported function also contains non-C subfield expressions. No rebuilt game
EXE was produced. Full diagnostics and build status are saved under
`bfv-reference-local/native-logic/build-attempt`. Earlier native oracle checks
executed original binary instructions, not a rebuilt engine.

Continued after user requested compilable modding code: created the independent
`bfv-native-code` project. It reconstructs and compiles the native AI interpreter,
adds editable plan/bailout/vehicle-rating rules, and appends linked native code to
guarded copies of the original client/server PE images. This provides a real game
EXE build route while the remaining engine stays in original machine code.

Both outputs compile and match 3,968 controlled original-versus-compiled/ABI checks.
Runtime inspection verified loaded code and replacement slots in both game
processes. Client observed over 545,000 interpreter calls; dedicated server over
5,500. Full engine source reconstruction and sustained stability remain unproven.
Client test processes later ended without a known reason or Windows Application
Error record; the user was asked whether the windows were closed manually. See
the native project's README, MODLOG and build reports for exact scope/evidence.

User confirmed manually closing the latest client test. Its exit is explained by
that action, not recorded as a demonstrated crash in the rebuilt interpreter.
