# Strategic A* search

`bfv-native-code/src/astar_strategic.c` reconstructs the neighbour expansion of
the bot `AStarStrategicSearch` (lifted source `bf/ai/AStarStrategicSearch.c`)
and the strategic-grid queries it calls (`bf/ai/AIPathfinding.c`,
`bf/ai/misc_2.c`).

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_strategic_same_cell` | `009cf470` | `00784e60` | grid; a, b; RET 8 |
| `bfv_strategic_cell_point` | `009d1330` | `00786b30` | grid; output, point, layer; returns output, RET 0Ch |
| `bfv_strategic_link_z` | `009de9c0` | `00792620` | grid; point, from layer, to layer; RET 0Ch |
| `bfv_strategic_link_x` | `009dea10` | `00792670` | the same for +x |
| `bfv_strategic_link_back_z` | `009dea60` | `007926c0` | grid; point by value (3 dwords), from, to; RET 14h |
| `bfv_strategic_link_back_x` | `009deaf0` | `00792750` | the same for -x |
| `bfv_astar_strategic_start` (vf6) | `009de950` | `007925b0` | search; output; returns output, RET 4 |
| `bfv_astar_strategic_goal` (vf7) | `009de970` | `007925d0` | search; output; returns output, RET 4 |
| `bfv_astar_strategic_estimate` (vf2) | `009de990` | `007925f0` | search; point; RET 4 |
| `bfv_astar_strategic_reached` (vf4) | `009deb80` | `007927e0` | search; point; RET 4 |
| `bfv_astar_strategic_within` | `009debd0` | `00792830` | ecx = point; low, high; RET 8 |
| `bfv_astar_strategic_step` (vf3 helper) | `009decf0` | `00792950` | search; node, index, output, estimate output; RET 10h |
| `bfv_astar_strategic_expand` (vf3) | `009deed0` | `00792b30` | search; node, output points, float costs; RET 0Ch |

Client and server bodies are instruction-identical apart from addresses.

**Grid.** A position is three ints: x, layer, z. A cell covers 64 x 64 units;
its index is `(z >> 6 << row_shift) + (x >> 6)` (grid `+0x28`), and the cells
(grid `+0x1c`) are 16 bytes:

| Offset | Contents |
| --- | --- |
| `+0x00` word | links in +z, 4 bits per layer (bit n: to layer n) |
| `+0x02` word | links in +x, same format |
| `+0x04 + 2*layer` | low 6 bits of that layer's x in the cell |
| `+0x05 + 2*layer` | low 6 bits of that layer's z |
| `+0x0c` byte | bit `4 + layer` set when the layer exists |

A -z or -x link is stored in the neighbouring cell, from its layer back to
ours. Those tests first check the neighbour against two global masks (x:
`00e0ed3c` / `00c314d0`, z: `00e0ed2c` / `00c314c0`) and that the target layer
exists there.

**Search.** The search object holds the reach distance (`+4`), the blocked
cost (`+8`), its owner (`+0x18`; the grid is the owner's `+0x40`), bounds
`low`/`high` (`+0x1c`/`+0x28`, ignored on an axis where they are equal), start
(`+0x34`) and goal (`+0x40`). Distances are `|dx| + |dz|`.

- **Step:** `index >> 2` picks +z, +x, -z or -x and `index & 3` the layer to
  enter. Without a link it returns the blocked cost. Otherwise it moves the
  node 64 units, snaps to the layer's point in the new cell, calls slot 3 of
  the owner's indexed object (native `00980120` / `0072a080`) with the point,
  rechecks the bounds, writes the estimate to the goal and returns the step
  cost. An `index >> 2` above 3 keeps the position and goes on if the low byte
  of `index` is nonzero, as natively.
- **Expand:** tries all 16 neighbours, writes each cost as a float, and keeps
  those that aren't blocked (copying the layer to `+0x0c`). Returns the count.
- **Reached:** same cell and layer as the goal, with the layer present, and
  within the reach distance.

Where the native code returns a byte through `xor al, al`, the C returns the
same whole EAX (for example the cell address with its low byte cleared).

`verify_astar_strategic.py` runs original and rebuilt entries on a random grid
per case. Cases vary the masks (including 0 and all bits), row shift (including
a shift count above 31), layers past 3, bounds, blocked costs, aliased outputs,
duplicate output slots and all 12 x87 control words. Slot 3 is a stub that
records its calls and can move the point. Each case compares EAX, the whole
256 KB arena, the grid masks, ESP, saved registers and the x87 control, tag and
status words (not C0-C3, which the native compare leaves and no caller reads).
Each target passed 4000 cases (307 or 308 per function; the step cases made
106 slot-3 calls and the expand cases 1122). The default full verifier includes these cases. Swapping the layers in
one link test makes it fail.

```powershell
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_astar_strategic.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

For a build made with `--output`, add `--work '{target}-custom'`.

## Setup and run

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_astar_strategic_setup` | `009dec20` | `00792880` | search; owner, start, goal, float reach, low, high; returns the reach distance, RET 18h |
| `bfv_bitset_init` | `009ceda0` | `00784590` | bit set; bit count; returns the bit set, RET 4 |
| `bfv_astar_strategic_run` | `009cf5c0` | `00784fb0` | request; listener argument, four engine arguments; RET 14h |

- **Setup:** stores `0x20` and `0x41fc` at `+0x10`/`+0x14`, copies the bounds,
  sets the owner, queries start and goal on the owner's grid (native
  `00a07350` / `007bb930`, result unused; the second call rereads the owner),
  snaps both to their cells' layer points, and sets the reach distance to
  `_ftol2(reach * 5)`.
- **Bit set:** stores the count and allocates `ceil(count / 32)` words with
  operator new, then clears as many words as the reread count needs.
- **Run:** `n = _ftol2(|radius|)` (request `+0`), size `2n + 1`. The disc mask
  is a `size x size` bit grid (at least 1 x 1); with size 1 or less it is one
  set bit. Otherwise the centre is `size / 2 + 0.5` as a float, and the bit of
  row i, column j is set when `(j + 0.5 - c)^2 + (i + 0.5 - c)^2 <= |radius|^2`
  in extended precision (an unordered compare leaves it clear). It then
  notifies the engine's listeners (`009def90`), runs the A* engine (`009e4620`
  / `00798280`) with the mask, the request's `+0x120` and `+0x118`, 2 and its
  flag byte (`+4`), notifies again (`009defd0`, `009df220`) and deletes the
  mask's words. The native loop is unrolled by four and recomputes the row
  term per pass; the values and sticky x87 flags are the same.

`verify_astar_setup.py` runs these three with stubs for the grid query, cell
point, listeners, engine and operator new/delete. The stubs log their
arguments and the mask's size and bits, and may relink the owner, change the
grid, the bit count, the request's flag or the mask's words between calls.
Radii include zero, halves, non-finite values and values whose `2n + 1`
wraps. Each target passed 3000 cases (1000 per function; 605 runs built a
disc 3 to 129 cells wide, the rest the single-bit mask). Each case compares EAX, the stub
log, the whole arena, ESP, saved registers and the x87 control, tag and status
words (not C0-C3). The default full verifier includes these cases. Dropping
the row term from the disc test, or using the bit count from before
`operator new` instead of the reread one, makes it fail.

```powershell
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_astar_setup.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

For a build made with `--output`, add `--work '{target}-custom'`.

Still native: the A* engine (`009e4620`) and its open/closed lists,
`TaskAStarStrategicSearch::execute` and the local search's successor
generation (its estimate, goal test and end points are in
[ASTAR-LOCAL](ASTAR-LOCAL.md)).
