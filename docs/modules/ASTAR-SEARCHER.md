# A* searcher step

`bfv-native-code/src/astar_searcher.c` reconstructs the step of the bot A*
searcher: the time-sliced A* loop the local and strategic search tasks run
through `TaskAStarLocalSearch::execute` and `TaskAStarStrategicSearch::execute`
(see [TASK-SCHEDULER](TASK-SCHEDULER.md)). The problem side (start, goal,
estimate, successors, goal test) is the `AStarLocalSearch` or
`AStarStrategicSearch` object; see [ASTAR-LOCAL](ASTAR-LOCAL.md) and
[ASTAR-STRATEGIC](ASTAR-STRATEGIC.md).

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_searcher_step` | `00a09340` | `007bd990` | searcher; job, in progress (byte), running (byte out), time left (double *); RET 10h |
| `bfv_searcher_heap_push` | `00a09090` | `007bd6e0` | searcher; node; RET 4 |
| `bfv_searcher_heap_update` | `00a08e70` | `007bd540` | searcher; node; RET 4 |
| `bfv_searcher_heap_pop` | `00a08a60` | `007bcfd0` | searcher; RET |
| `bfv_searcher_sift_down` | `00a08870` | `007bcde0` | searcher; RET (called only by pop) |
| `bfv_searcher_unclose` | `00a089e0` | `007bcf50` | searcher; node; RET 4 |
| `bfv_searcher_reset` | `00a09190` | `007bd7e0` | searcher; RET; EAX passes through when nothing is allocated |
| `bfv_searcher_alloc` | `00a092c0` | `007bd910` | searcher; RET; returns the node or 0 |
| `bfv_searcher_path` | `00a08ad0` | `007bd040` | searcher; points, max, ids; RET 0Ch; returns the count |
| `bfv_searcher_path_to_ring` | `00a08b50` | `007bd0c0` | searcher; ring, unused, ids; RET 0Ch; returns the path length |
| `bfv_path_ring_extend` | `00a08440` | `007bca20` | ring; count; RET 4; returns the points added |
| `bfv_searcher_closed_points` | `00a08c60` | `007bd1d0` | searcher; points, max, ids; RET 0Ch; returns the count |
| `bfv_searcher_run` | `00a09880` | `007bded0` | searcher; job; RET 4; returns the step's EAX |
| `bfv_searcher_deliver` | `00a098b0` | `007bdf00` | searcher; callback object; RET 4; returns the path length |

Client and server bodies are instruction-identical apart from addresses.

**Searcher** (`0x884` bytes; the local and strategic searchers are globals at
`00e10f98` / `00e11860`, server `00c334e0` / `00c33da8`):

| Offset | Contents |
| --- | --- |
| `+0x04` / `+0x08` | heap (open set) begin / end; 1-based, so empty while `(end - begin) >> 2 < 2` |
| `+0x10` | allocated node vector (begin `+0x14`, end `+0x18`) |
| `+0x20` | a second node list, chained like the closed list; only reset writes it (0) |
| `+0x24` | open hash (bucket heads, chained through node `+0x28`) |
| `+0x28` | closed list head (doubly linked through node `+0x20` / `+0x24`) |
| `+0x2c` | closed hash |
| `+0x30` | nodes allocated |
| `+0x58` | bucket count |
| `+0x5c` | node limit |
| `+0x60` | last node the step stopped at |
| `+0x64` / `+0x68` | start / goal node |
| `+0x6c` | steps left |
| `+0x70` | successor count |
| `+0x74` byte | started |
| `+0x78` / `+0x7c` | sum of step costs / number of successors seen |
| `+0x80` | 256 successor node slots |
| `+0x480` | 256 float step costs |
| `+0x880` | problem |

**Node** (`0x34` bytes): x, y, z, id (`+0x0c`), estimate (`+0x10`), heap index,
cost (`+0x18`), total (`+0x1c`, the heap key), closed prev / next, hash next,
parent (`+0x2c`), child (`+0x30`). A node is identified by x, z and id; its
bucket is `|(z * 31 + x * 17) % buckets|` (signed `idiv`, wrapping arithmetic,
z read before x).

**Problem methods** (vtable slots): 0 delete(1), 2 estimate(node),
3 successors(node, slots, costs) returning the count, 4 is goal(node) (AL),
6 set start(node), 7 set goal(node). The job's method 1 returns the problem.
Problem `+0x10` is the successor array size and `+0x14` the step budget.

**Step.**

1. Read the timer global (`00e0ef50` / `00c314b0`) once, clear `*running`,
   and take the deadline as the timer's method 3 (a double in ST0) plus
   `*time_left`.
2. When not in progress: take the problem from the job, reset the searcher,
   allocate and fill the goal and start nodes, estimate the start, set the
   step budget to the problem's capped by the node limit, empty the successor
   slots and open the start node.
3. Loop while the timer is below the deadline (an unordered compare stops),
   the heap is non-empty and the decremented step budget is positive: pop the
   cheapest node, stop if it is the goal, refill empty successor slots (stop
   if the pool runs out), expand, and relax each successor. Relaxing converts
   the float step cost with `_ftol2`, looks the successor up in the open then
   the closed hash, skips it if the known node is cheaper, otherwise takes the
   new parent, cost and estimate and re-sorts it in the heap, reopens it from
   the closed list, or opens the new node (its slot is then given up). The
   expanded node goes to the closed list and hash.
4. `*running` becomes 1 if time ran out (the search continues next slice);
   the node reached is stored. Otherwise the problem is deleted, the log
   global's method `0xcc` (`00e0ee08` / `00c313d0`) is called, and the step
   returns AL = 1 if the goal was found; on failure it logs again for each of
   node limit reached, budget used up and pool exhausted, resets the searcher
   and returns AL = 0. EAX's upper bytes are what native leaves (the node
   reached, the log's or the reset's return).

Natively the pool-exhausted flag lives in the `time_left` argument slot and
is only written as a byte, so before the first expansion it is that pointer's
low byte; the C keeps this.

**Heap helpers.** The open heap is 1-based. Totals compare signed; equal
totals are decided by the game's random generator (`004e0990` on `00d30c00`,
server `004b1fd0` on `009436a0`, a 31-bit value): v = ((r | 0x80) >> 7) + 1,
and the tie sorts first when v * 2^-24 (`00b5d414`) compares below 0.5
(`00b44e48`) by FCOMP; that is a coin flip, so equal-cost paths are not
explored in a fixed order.

- Push: a node already at its heap index is only moved up (update). Otherwise
  the heap vector's native push_back (`00a08fc0` / `007bd690`) appends a zero
  and the node rises from the new last slot.
- Update: the node rises while it sorts before its parent.
- Pop: sift-down, then the node is unlinked from its open-hash chain.
- Sift-down clears heap[1]'s index, drops the last entry and walks down from
  slot 1 without stopping early. At each level the cheaper child (right
  before left, ties by coin flip) either moves up, when it sorts before the
  moving entry, or takes the moving entry's place and becomes the moving
  entry. The moving entry lands at the final slot if it is still in range.
- Unclose unlinks the node from its closed-hash chain and the closed list.
  Without a previous node the list head advances to the head's own successor.

Their callers reload EAX, ECX and EDX after every call, so the C returns
nothing.

**Node alloc.** Returns 0 once the node vector holds `node_limit` nodes
(unsigned compare), else takes `0x34` bytes from the game's pool allocator
(`00412ee0` on `00e11820`, server `00404290` on `00c33d68`; source tag "",
flags 0), returning 0 if that fails. The node is pushed onto the node vector
(native push_back), `+0x30` is counted, and the node's closed links, hash
link, parent and id are cleared; the rest is what the pool hands out.

**Result output.** A path point is the node's x, y, z (12 bytes).

- Path copy (called by the search tasks' `execute`): counts the parent chain
  from the node the search stopped at; a chain longer than `max` starts that
  many parents up (the end of the path is cut). The points go out start
  first, with each node's id to `ids` when given. A negative `max` walks off
  the start natively.
- Path into a ring: the ring is capacity, head, tail and a point array.
  The ring is first extended by the path length (the extension adds at most
  the free space, stores the tail once unwrapped and then wrapped, and
  returns what it added; indexes wrap by signed `idiv`, made non-negative).
  Then each node, from the end of the path back, goes to the slot that many
  points before the tail if that slot lies between head and tail (wrapped or
  not), and its id to `ids[1]`, `ids[2]`, ... (1-based). The second argument
  is unused.
- Closed-list points: up to `max` points and ids from the closed list, most
  recently closed first, then from the list at `+0x20`.

**Reset.** Does nothing when the node vector is absent (EAX stays the
caller's; a naked entry passes it to the C body) or empty (EAX 0).
Otherwise it empties the `+0x20` list and the closed list, clears both
hashes (rereading the bucket count each step), hands every node back to the
pool (`00412fc0` / `00413600` on the node pool), resizes the node vector to
0 (`00a08de0` / `007bd350`), clears the allocation count and resizes the
heap to 1 (its unused slot 0). EAX is the last resize's; the step's failure
path keeps its upper bytes.

**Run to completion.** Calls the step once with an unlimited time
(`10000000.0` s at `00bfccf0` / `0087b668`), in progress 0, and `running`
written into the wrapper's own job argument slot. The step seeds its
out-of-nodes flag from the low byte of the time pointer, so the wrapper's
frame must match: it is a naked entry that keeps the time 8 bytes below the
return address, with the readable form in its comment. Verified directly (342 cases per target: random entry stack
position, step EAX and `running`; the step's arguments, stack addresses
relative to entry and the frame are compared); a changed in-progress flag,
`running` slot or time slot is caught.

**Path delivery** (called after a step finishes, `009d28d4` and
`009d29f8`). Pushes the chain from the last node through `+0x2c` parents into
a local `std::deque` of node pointers (four per block; push_back
`00a09010` / `0068b240`). Then it takes nodes from the back, so the start
comes first. Each node goes to the callback object's method 0 as
`{x, id, z}` (node `+0x00`, `+0x0c`, `+0x08`), and is popped (the front
offset clears when the deque empties). Then the searcher is reset and the deque
destroyed (`00554100` / `00768f00`); EAX is the path length. Verified
directly (300 cases per target, 3603 callback steps; the deque's push_back
and destructor are logged models whose map wraps around its end). The
mutations caught were a wrong id field, an unwrapped block index, a skipped
reset, a pop that keeps the front offset, and a one-node path skipped.

Still native: the pool allocator and free, and the vectors' push_back,
resize and growth.

**Verification.** `verify_searcher.py` runs the native and C step on the same
random searcher (wired into `verify.py`). The helpers, problem and job
methods, timer and log are stubs: the heap stays sorted by total, alloc hands
out pool nodes up to a per-case cap, and half the cases are long searches on
a 3 x 3 grid with zero and negative step costs, so open and closed matches,
heap updates and reopenings occur. Cases cover fresh and resumed searches,
deadlines crossed at different calls or NaN, step budgets, bucket counts
(including negative), node limits and the time pointer's low byte, and compare
EAX, the stub call log, the arena, ESP, callee-saved registers and the x87
control, tag and non-condition status words across rounding modes.

The heap helpers are verified directly by the same script: the five
helpers in turn, on random heaps (sorted or not, with small totals for ties),
open and closed hashes, closed lists (including a non-head node without a
back link), pushes of new, in-heap and wrongly indexed nodes, and updates.
Pop and push run their nested sift-down and update, and the random generator
runs live (its calls are logged and its state compared). The vector push_back
is a logged model. Each case compares the log, the arena, the generator
state, the heap vector, ESP, callee-saved registers and the x87 words.
Mutations caught: an inverted tie result, sift-down keeping the moving entry,
and unclose advancing the head from the node instead of the head.

Reset, node alloc and the output routines are verified the same way, 400
cases each (reset with a random incoming EAX, absent, empty and full node
vectors, zero and negative bucket counts; free and resize are logged
models): random parent chains up to 40 nodes, `max` below, at and above the
length, rings with negative capacities and head or tail outside the range,
extension requests that are negative or exceed the free space, both closed
lists, node vectors absent, short or at the limit, and a pool allocator that
fails a fifth of the time. The ring extension runs nested; the pool
allocator and the vector push_back are logged models. Each case compares
EAX, the log, the arena, ESP, callee-saved registers and the x87 words.
Mutations caught: reset changing the passed-through EAX, resizing the heap
to 0 or skipping the first node's free; 0-based ring ids, the extension returning the request, a
skipped second list, a signed node-limit compare, an off-by-one path cut and
a wrong wrapped-ring range.
