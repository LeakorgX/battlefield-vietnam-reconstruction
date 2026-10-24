# Local A* search

`bfv-native-code/src/astar_local.c` reconstructs the estimate, the goal test
and the start and goal end points of the bot `AStarLocalSearch` (lifted source
`bf/ai/AStarLocalSearch.c`). The searcher ([ASTAR-SEARCHER](ASTAR-SEARCHER.md))
calls these through the search object's vtable.

| Function | Client | Server | Interface |
| --- | --- | --- | --- |
| `bfv_astar_local_estimate` (vf2) | `009e5180` | `00798e10` | search; node; RET 4 |
| `bfv_astar_local_reached` (vf4) | `009e63f0` | `0079a080` | search; node (may be rewritten); RET 4 |
| `bfv_astar_local_set_start` (vf6) | `009e5da0` | `00799a30` | search; output node; EAX = start z, RET 4 |
| `bfv_astar_local_set_goal` (vf7) | `009e5e10` | `00799aa0` | search; output node; EAX = goal z, RET 4 |
| `bfv_astar_local_add_cell` | `009e6ee0` | `0079ab70` | search; from node, dx, dz; RET 0xc, EAX unused |
| `bfv_astar_local_add_cell_avoiding` | `009e7200` | `0079ae90` | the same, also testing keep-out areas |

Client and server bodies are instruction-identical apart from addresses.

**Positions.** A node is x (int), y (float height), z (int) and a layer
(`+0x0c`). The layer is also a cell size: a node with layer n covers the
2^n x 2^n square that x and z round down to.

| Search offset | Contents |
| --- | --- |
| `+0x04` | reach radius on each of x and z |
| `+0x18` | owner; the layer lookup's map, `+0xc508` its default layer |
| `+0x1c` | start x, y, z (y is filled with the ground height) |
| `+0x28` | goal x, y, z |
| `+0x34` | target x, y, z: where a node in the goal cell snaps to |
| `+0x40`, `+0x48` | goal x and z rounded down to the goal layer's cell (`+0x44` cleared) |
| `+0x4c`, `+0x50` | start and goal layer; looked up when negative |
| `+0x5c` | reach limit for the sum of the x and z distances |
| `+0x60`, `+0x6c` | low and high bounds for goal mode 3 |
| `+0x9c` | goal mode |
| `+0xa0` | byte: test added cells against the `+0x60`/`+0x6c` bounds |
| `+0xa4`, `+0xa8`, `+0xac` | successor output: node pointers, step costs (float), count |
| `+0xb0` | the node being expanded |
| `+0xb8`, `+0xbc` | keep-out areas, 16 bytes each: x, z, radius, distance-sum limit |

**Estimate.** `3 * round(|goal y - node y|) + |dx| + |dz|`. The height
difference is rounded by the native float-to-int helper (`00522cf0` /
`004d8260`, current rounding mode). Natively each of dx and dz loads both
`|a - b|` and `|b - a|` on the x87 stack, stores the second as a float (which
raises the precision flag above 2^24) and keeps the smaller. The two are always
equal, so the first is kept and truncated by ftol2. The C keeps the float store
and the dead branch.

**Goal test.** Goal mode 3 asks the strategic bounds test (`within`). Modes
above 3 always count as reached. Modes 0 to 2: if the node has a non-zero
layer and its cell is the goal cell, the node is overwritten with the target
and counts. Otherwise it counts when `|x - target x|` and `|z - target z|` are
each at most the radius (signed) and their sum is at most the reach limit. The
byte result keeps the upper bytes of the last value in EAX (the x offset, the z
distance, the sum, the target x or the mode).

**End points.** Set start looks up the start layer once (fastcall
`009e57f0` / `00799480` with the owner and the point), writes it to the node,
asks the terrain object (`00da21a0` / `00c0f118`, method `+0x4c`, float x and z)
for the ground height, then rereads x and z and writes x, height, z to the node
and the height into the start's y. Set goal does the same; the first time it
also falls back to the owner's default layer when the lookup returns -1 and
keeps the goal cell.

**Adding a cell.** The successor generation (vf3) calls two helpers that add
the neighbour dx, dz of a node to the output list. The next output node gets
the neighbouring cell at the node's layer (its centre for layers above 0) with
y cleared. The cell is rejected when x or z is outside the world masks
(`00e0ed3c`/`00e0ed2c`, server `00c314d0`/`00c314c0`), when the layer's cell
map (owner `+0x3c`, `[[map+0x50]+layer*4]`) reports it blocked (method
`+0x0c`), when `+0xa0` is set and the bounds test fails, or (avoiding variant
only) when it is inside a keep-out area: signed `|dx|` and `|dz|` below the
radius and their sum below the limit. An accepted cell gets the terrain height
and normal (terrain method `+0x5c`) and the step cost
`16 * ((1 << layer) + layer cost) + round(16 * (3 * |from y - height| + 7 * (1 - |normal y|)^2))`,
the layer cost from owner `+0x48`, stored as an unsigned float; its estimate
(vf2) times 16 goes to node `+0x10` and the layer to `+0x0c`. A rejected cell
whose node is above the owner's default layer is split: the helper recurses on
the finer cell towards dx, dz and then on the one beside it.

**Verification.** `verify_astar_local.py` runs the original and reconstructed
entries side by side. The layer lookup is a logging stub that returns a chosen
layer (often -1) and the terrain height method is a logging stub that returns
a chosen float. The rounding helper, ftol2 and the bounds test stay native.
Inputs include NaN, infinity, denormal and large float heights, coordinates
near ±2^31 and 2^24, every goal mode, nodes inside and outside the goal cell,
and nodes placed on top of the search object's own start, goal, target and
goal-cell fields, so the order of reads and writes shows. Each case compares
EAX, the stub logs, the whole arena, ESP, saved registers and the x87 control,
tag and status words (not C0-C3), across rounding modes. Each target passed
4000 cases (1000 per function; 963 layer lookups, 2000 height calls). The
default full verifier includes them. These mutations were caught: writing the
node before rereading the start's z, `<` for the reach-limit test, a snap that
leaves the node's y, `2 *` for the height weight, a kept `+0x44`, `< 0` for the
failed-lookup test and an unsigned radius test. Dropping the float store in the
estimate passes, because Unicorn does not raise x87 exception flags (see
[STATIC-RECOMP](../STATIC-RECOMP.md#verified-results)); the store is kept from
the instruction evidence.

```powershell
uv run --with pefile --with capstone --with unicorn python bfv-native-code/tools/verify_astar_local.py --game-dir 'D:\Games\Battlefield Vietnam' --target both
```

For a build made with `--output`, add `--work '{target}-custom'`.

`verify_astar_local_cells.py` checks the two cell helpers the same way. The
cell map, terrain and estimate are logging stubs answering from chosen masks,
normals, heights and estimates; the rounding helper and the bounds test stay
native. Cases cover layers 0 to 33, default layers up to five below (so splits
recurse), world masks, keep-out areas near the node, the expanded node placed
on the next output nodes, and every rounding mode. Each case compares the whole
arena (search, owner, output nodes and costs, stub logs), ESP, saved registers
and the x87 control and tag words. Each target passed 4000 cases (1794 added
cells). The stack address of the normal is not logged, since it depends on the
frame layout.

Still native: the body of the successor generation (vf3, `009e7b80` /
`0079b810`), which walks the eight directions with inlined copies of these
helpers.
