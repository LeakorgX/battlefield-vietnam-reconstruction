# Native recompilation/modding journal

User requires editable, compilable game logic, not a procedural substitute or
decompiler-only references. Chosen route: compile reconstructed functions into a
new fixed-address PE section and replace guarded native vtable entries in a copy
of each original executable. Original binaries remain unchanged.

Implemented: full recovered BAPInterpreter control flow with exact ABI; editable
plan eligibility and returned bailout/vehicle ratings. Full vehicle scoring and
bailout recomputation remain in the original native routines. Replacements are
guarded by exact image hashes and original vtable pointers.

Verification: 1,920 interpreter comparisons, 32 exact cached-bailout comparisons
and 32 vehicle-wrapper ABI cases per target; 3,968 cases passed across client/server.
The context getter uses no stack arguments; the following context virtual callback
consumes four arguments. This was corrected by inspecting the native instructions
and testing callbacks, not by trusting inferred decompiler signatures.

The BBChange function performs vehicle evaluation even when its second argument is
zero. Its wrapper always calls the intact original method, preserving the complete
candidate evaluation before applying editable rating rules. It must not be treated
as a cached-rating getter.

Compiled outputs: BfVietnam-editable.exe and bfvietnam_w32ded-editable.exe in the
game directory. Client startup test launched exact PID 42564. Game settings/save
folders were copied into build/profile-backup-* before launch. Startup slot/counter
checks are recorded separately from controlled native comparison results.

Live client verification PID 32660: all three replacement pointers loaded; 1,128
immutable payload bytes matched the linked code; 545,378 interpreter invocations
and 3,523 vehicle-wrapper invocations observed. The earlier client test process
42564 ended; no corresponding Application Error event was found. The current
test was launched after a repeat build whose output SHA256 exactly matched the
already verified binaries. No end reason is inferred for the first process.

Live dedicated-server verification PID 21144: all three replacement pointers
loaded, payload bytes verified, 5,553 interpreter invocations and 48 vehicle-wrapper
invocations observed. The attempted `+config` override did not change the bound
ports; the server used the installed LAN settings (game port 15567). The unsupported
override was removed from the shipped launcher and its unused configuration was
removed. Server was stopped by exact PID after recording its report. Both client
test processes subsequently ended; no corresponding Windows Application Error
event was found. The user subsequently confirmed manually closing the latest
client test. This is a runtime-execution smoke test, not full-match certification.
Original EXE hashes match both inputs.

Remaining full-source work: reconstruct native numerical helpers, initialization,
object ownership/layouts, vehicle evaluation, physics, weapons, networking and the
rest of the engine. This working native-mod build does not mean that reconstruction
is complete or that the complete engine has been open-sourced.

## Artillery driver reconstruction

Recovered the actual three-stack-argument rating ABI of BBFireArtilleryDriver,
replacing its client/server slot 9 with authored C. The driver resolves the gun,
sends its notification, evaluates the returned component, routes artillery versus
fallback scoring, and writes the bot rating/eligibility tables. The component
predicate is also source-owned; component conversion and the large target
evaluator remain native. This does not reconstruct all artillery behavior.

Focused verification passed 1,536 paired cases per binary, including callback
index/table changes, low-byte conditions, argument bits and rating rounding.
See docs/ARTILLERY.md for the recovered interface and outstanding dependencies.

The final five-replacement build passed all 22,298 emulator checks across both
targets and the 16 real Windows vector/exception executions. Verification tools
now print each subsystem as it starts, so long comparisons show their current
stage. No artillery live-match behavior has been claimed.
