# AI task scheduler

`bfv-native-code/src/task_scheduler.c` reconstructs the routines that queue,
remove and time bot AI tasks (pathfinding searches, strategic queries and
other `Task` subclasses; lifted source `bf/ai/Task.c` and `bf/ai/misc_3.c`).

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_task_sleep` (`Task` vf) | `009de050` | `00791cb0` | task; double delay; RET 8 |
| `bfv_task_tree_remove` | `00a09e90` | `007bebd0` | scheduler; root, task, remove-all byte; RET 0Ch |
| `bfv_scheduler_time_left` | `00a09f50` | `007bec90` | scheduler; returns st0 |
| `bfv_scheduler_add` | `00a09f70` | `007becb0` | scheduler; task, double delay, as-child byte; RET 10h |
| `bfv_scheduler_remove` | `00a0a000` | `007bed40` | scheduler; task; RET 4 |

Client and server bodies are instruction-identical apart from addresses.

**Layout.** A task links its children through `first_child` (`+0x20`) and
`next` (`+0x1c`), and its sequel, the task queued to run after it, through
`+0x18`. It holds its wake time (`+0x08`, double), its scheduler (`+0x14`),
its delay (`+0x28`, double), a sleeps-until-woken byte (`+0x30`) and a byte at
`+0x31` that sleep and add clear. The scheduler holds the root task (`+0x00`),
the current slice's start and length (`+0x08`, `+0x10`), the running task
(`+0x18`) and the task added last (`+0x1c`).

- **Sleep / add delay:** a delay of exactly -1 sets sleeps-until-woken and a
  delay of 0; any other value (NaN too) is kept.
- **Add:** clears the task's links and sets its scheduler. When the as-child
  byte is set or nothing was added before, the task becomes the first child of
  the running task, due at its wake time plus the delay; otherwise it becomes
  the sequel of the last added task. Either way it is now the last added.
- **Remove:** searches root's children depth first. At the child equal to the
  task (or at the first child, when remove-all is set) it recurses with
  remove-all set, then unlinks the task from root's child list: its sequel
  takes its place with the task's next link and a wake time of the task's wake
  time plus the sequel's delay; without a sequel the previous child (or root)
  takes the task's next link and wake time. Returns 1 in AL when it unlinked.
  `scheduler_remove` starts at the root.
- **Time left:** `(slice length + slice start)`, rounded to a double, minus
  `getTimeSeconds()` (native `00894540` / `006634b0`), returned unrounded.

Doubles the native code copies through the x87 stack are still x87 copies, so
a signalling NaN delay is stored quiet and raises the invalid flag. Where the
native code returns a byte, the C returns the same whole EAX. `Task::sleep`
leaves FNSTSW and the caller's upper bits in EAX natively; it is `void` here
and no caller reads it.

`verify_task_scheduler.py` runs original and rebuilt entries on random task
trees: random child order, detached tasks, sequels anywhere, wake times,
delays and slice times from finite, signed-zero, infinite, NaN, signalling-NaN
and denormal values, remove-all and as-child bytes with clear low bytes, and
all 12 x87 control words. getTimeSeconds is a stub returning a chosen double in
both images. Each case compares EAX (not for sleep), the time-left result as
80 bits, the whole arena, ESP, saved registers and the x87 control, tag and
status words (not C0-C3). getTimeSeconds also sets EAX in the stub, as the
native one overwrites it. Each target passed 3000 cases (600 per function;
188 tree removals and 289 scheduler removals unlinked a task). The default
full verifier includes these cases. Inverting the add's "nothing added yet"
test makes it fail.

```powershell
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_task_scheduler.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

For a build made with `--output`, add `--work '{target}-custom'`.

## Search task execute

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_task_local_search_execute` (`TaskAStarLocalSearch::execute`, vf) | `009de160` | `00791dc0` | task; returns EAX |
| `bfv_task_strategic_search_execute` (`TaskAStarStrategicSearch::execute`, vf) | `009de230` | `00791e90` | task; returns EAX |

Both methods are the same code over different globals: a searcher object
(client `00e10f98` / `00e11860`, server `00c334e0` / `00c33da8`) and a slot
naming the task whose search it runs (client `00e0f9cc` / `00e0f9d0`, server
`00c31d98` / `00c31d9c`).

- The task takes the scheduler's time left (rounded to a double). If the slot
  holds another task, it sleeps with the delay at `00b426c0` / `00807350`
  (0.0) and tries again on its next turn.
- Otherwise it claims the slot and calls the searcher's step (`00a09340` /
  `007bd990`, reconstructed in [ASTAR-SEARCHER](ASTAR-SEARCHER.md)) with its search (`+0x3c`), its in-progress byte (`+0x38`), a
  still-running byte and the time left. The still-running byte becomes the
  new in-progress byte.
- Still running: it sets `+0x34` to 1 and sleeps, keeping the slot.
- Finished: it frees the slot. A zero AL from the step stores -1 into
  `*result` (`+0x40`). Otherwise, with a positive point count (`+0x4c`), it
  stores what the searcher's path copy (`00a08ad0` / `007bd040`, arguments
  `+0x44`, the count and `+0x48`) returns; with no points it stores 0.

The result pointer and path fields are reread after the step, as natively.
The C returns the EAX the native code leaves: the sleep's, the step's, the
path copy's or the point count.

`verify_search_task.py` runs original and rebuilt entries with the time left
replaced in both images by code that loads a chosen double, and with
Task::sleep, the step and the path copy as logging stubs. The step stores a
chosen still-running byte, returns chosen EAX values and in some cases
rewrites the task's result pointer and path fields, its in-progress byte or
the slot. Times include signed zeros, infinities, quiet and signalling NaNs
and denormals; slots start empty, held by the task or by another task. Each
case compares EAX, the stub log (including the time-left double the step is
handed), the task and result words, both slots, ESP, saved registers and the
x87 control, tag and status words (not C0-C3). Each target passed 2000 cases
(1000 per method; local / strategic: 206 / 209 found the slot taken, 484 / 487
left the search running, 119 / 126 failed, 95 / 81 had no points and 96 / 97
copied a path). The default full verifier
includes these cases. Caching the result pointer before the step makes it
fail.

```powershell
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_search_task.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

## Queueing and player map searches

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_task_unschedule` (`Task` vf) | `009de020` | `00791c80` | task; returns EAX |
| `bfv_task_queue` | `009de030` | `00791c90` | task; child, double delay, as-child; RET 10h |
| `bfv_task_player_map_execute` (`TaskPlayerMapSearch::execute`, vf) | `009de300` | `00791f60` | task; returns EAX |
| `bfv_task_strategic_player_map_execute` (`TaskStrategicPlayerMapSearch::execute`, vf) | `009de390` | `00791ff0` | task; returns EAX |

- **Unschedule:** with no scheduler (`+0x14`) it returns the task in EAX;
  otherwise it returns `scheduler_remove(task)`.
- **Queue:** adds the child to the task's scheduler with the delay (copied
  through the x87 stack, so a signalling NaN is stored quiet) and as-child
  byte.
- **Player map executes:** the task embeds an `AStarLocalSearch` (`+0xa8`)
  and a search task child (`+0x58`). The local one calls the search's prepare
  (`009e55d0` / `00799260`: map `+0x50`, start `+0x38`, goal `+0x44`, limit
  `+0x17c`, range -1.0f) and finish (`009e61e0` / `00799e70`); the strategic
  one calls the strategic prepare (`009e56e0` / `00799370`) with pointers to
  `+0x178` and `+0x184`. Each then fills the child (result `+0x16c`, path
  `+0x170`, points `+0x178` local / `+0x174` strategic, the search, the
  task name "TaskAStarLocalSearch" / "TaskAStarStrategicSearch", state 2) and
  queues it with delay 0.0, not as a child. The fields are read after the
  prepare calls, as natively.

## Strategic search task

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_task_strategic_search_run` (`TaskStrategicSearch::execute`, vf) | `009de410` | `00792070` | task; returns EAX |

The task embeds an A* search task child (`+0x58`), a cell query task
(`TaskGetStrategicCellInfoNo`, `+0xa8`) and an `AStarStrategicSearch`
(`+0x2a0`), and holds the start and goal cells (`+0x2ec`, `+0x2f0`, -1 until
found), the caller's result, path and point count (`+0x2f4`..`+0x2fc`), its
state (`+0x300`) and a float reach (`+0x304`). Each turn runs one state:

- **0:** without a start cell, it initializes the cell query (`009de100` /
  `00791d60`: map, start point, reach 0, `&start_cell`) and queues it with
  delay 0.
- **1:** without a start cell, it stores -1 into `*result` and returns 0.
  Without a goal cell, a reach above 64.0 (`00b90220` / `00836acc`, FCOMP, so
  NaN is not above) takes cell 0; if the goal cell is still -1 it queues the
  cell query for the goal with the reach's bits.
- **2:** without a goal cell, it stores -1 into `*result` and returns the
  result pointer. Otherwise it writes the start and goal cells into the
  points' y, runs the strategic setup (`009dec20` / `00792880`) with the reach
  and two zeroed bounds, and queues the search task child (named
  "TaskAStarStrategicSearch", search `+0x2a0`) with the result, path and
  point count read after the setup.
- **Other states** return state - 2.

States 0 and 1 then reread the state, store it plus one and sleep with delay
0. The native cell query init returns with ECX still the query, which the
native code passes on to the queue; the C passes the query's address.

## Strategic cell query task

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_task_cell_query_execute` (`TaskGetStrategicCellInfoNo::execute`, vf) | `009de710` | `00792370` | task; returns EAX |

The task holds a point (`+0x38`), a map (`+0x44`, whose `+0x40` is the
strategic grid), an embedded `TaskPlayerMapSearch` (`+0x48`), the caller's
out pointer (`+0x1e0`), the chosen layer (`+0x1e4`, -1 until found), a
tried-layer mask (`+0x1e8`), the reach's bits (`+0x1ec`) and the search's
result (`+0x1f0`).

- With a layer and a non-negative search result it stores the layer into
  `*out` and returns the result.
- Otherwise it resets the layer to -1 and tries layers 0-3. A layer counts
  when bit `4 + layer` of the grid cell's flag byte (cells at grid `+0x1c`,
  16 bytes each, byte `+0xc`; row shift at grid `+0x28`, masked to 5 bits as
  SHL does) is set and the layer is not tried. For it, it fetches the layer
  point (`009d1330` / `00786b30`), sets its height (fastcall `009e58b0` /
  `00799540`: ECX map, EDX point, then a byte that is set when bit `layer`
  of the flag byte is; the native code leaves other bits in the upper bytes
  of that argument, which the callee never reads) and measures it against
  the point (`009de5c0` / `00792220`). A distance below the best so far
  (start 0x100000) calls the measure again for the new best and takes the
  layer.
- No layer: it stores -1 into `*out` and returns 4.
- Otherwise it marks the layer tried, sets the height of the last fetched
  layer point again, initializes the player map search (`009de670` /
  `007922d0`) from the point to that layer point inside the 64-unit cell
  around the point (low `x & ~63, 0, z & ~63`, high +64), with result
  `+0x1f0` and the reach, queues it with delay 0 and sleeps with delay 0.

`verify_task_queue.py` runs original and rebuilt entries on random tasks
with scheduler add and remove, Task::sleep, the prepare, finish, setup and
cell query init calls as logging stubs (the cell query stub keeps ECX, as
natively); some stubs rewrite the result, path, scheduler or state fields.
Cell query cases use a small strategic grid with random flag bytes; the
layer point, height, distance and player map init are stubs (height writes
the point's y, distances include the 0x100000 boundary and negative values,
some layer point calls rewrite the tried mask or the out pointer).
Delays include signed zeros, infinities, quiet and signalling NaNs and
denormals, and reaches include 64.0 and its neighbours, signed zeros,
infinities, quiet and signalling NaNs and denormals. Each case compares EAX,
the stub log (the setup's bounds by content, as the stack frames differ), the
arena, ESP, saved registers and the x87 control, tag and status words (not
C0-C3). With `--count 4000` each target passed 10000 cases (1000 per queue function,
769 unschedules with a scheduler; 2000 strategic search steps: 247 ran the
setup, 216 queued a cell query, 302 only slept and 1235 returned early; 2000
cell queries: 562 queued a search, 93 found no layer after trying some and
1345 made no calls; 2000 direct distances). With the two inits below it is
12000 cases per target. The default full
verifier includes these cases. Reading the result pointer before the local
prepare, storing the state read before a cell query plus one, or keeping the
first distance instead of measuring again makes it fail.

## Strategic cell distance

`009de5c0` / `00792220` (`bfv_strategic_cell_distance`, thiscall: ECX the
first point, one stack argument the second) measures two grid points on x
and z. Per axis it loads `|b - a|` and `|a - b|` with FILD (equal, apart
from the wrapped `0x80000000`), stores the second as a float (which can
raise the precision flag), compares the two and truncates the smaller with
`_ftol2`; the float copy is used only when the second compares below the
first. That x87 sequence stays in a small asm helper so the status word
matches. The result is `max + ((min * 0x170000) >> 16) - min`, i.e.
max + 22 * min, with the 32-bit product wrapping and the shift arithmetic.

`verify_task_queue.py` also calls it directly (the cell query's distance
stub steps aside) on random points, including 0, ±1, 63, 64, the int32
limits and values around 2^24 where floats round. Each target passed 2000
such cases; using 0x160000 for the factor makes it fail.

## Cell query and strategic player map search inits

`009de100` / `00791d60` (`bfv_cell_query_init`) sets up a
TaskGetStrategicCellInfoNo: name, point (+0x38), map (+0x44), out pointer
(+0x1e0), then `*out = -1`, then the reach (+0x1ec), tried 0 and layer -1.
`009de670` / `007922d0` (`bfv_player_map_search_init`) sets up the
TaskStrategicPlayerMapSearch embedded at the query's +0x48: map (+0x50) first,
then name, start, goal, path goal (+0x170), path points (+0x174), result
(+0x16c), low (+0x178) and high (+0x184) copied element by element, reach
(+0x190). Natively both return with ECX still the task and EAX holding the
last value loaded (out, the reach). Callers rely on ECX, so the exported
entries are small naked stubs that save ECX, re-push the arguments, call the
C body and restore ECX before `ret`.

`verify_task_queue.py` calls them directly with point, bound and out
pointers that may lie inside the task or the next object, so copy order and
the late `*out` store show up, and checks ECX on return. Each target passed
1000 of each; storing `*out` before the out field makes it fail.

```powershell
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_task_queue.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

Still native: the scheduler's run loop, the other task subclasses' `execute`
methods, the layer point and height, the
searcher's step and path copy, the search prepare and finish, task construction and destruction.
