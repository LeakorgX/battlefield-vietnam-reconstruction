# Reconstructed collision events and lookups

`src/collision_events.c` recovers the event constructor and two object helpers.
Both reconstructed collision paths now call this C. Original entries remain
intact for any other native callers.

| Source function | Client entry | Server entry | ABI |
| --- | --- | --- | --- |
| `bfv_collision_construct` | `009dc3b0` | `007ae550` | thiscall, 44 argument bytes, returns receiver |
| `bfv_object_lookup` | `00926af0` | `006e8930` | thiscall, 4 argument bytes, returns object word |
| `bfv_event_interface` | `0092afb0` | `006e88b0` | thiscall, 4 argument bytes, returns indexed word |

Names reflect observed use, rather than recovered original identifiers.

## Constructor and object layout

The constructor receives two raw 12-byte vectors, a word-array pointer, an event
kind word, timestamp, strength and a final flag word. It stores only the low byte
of the kind and final flags. The callback still forwards complete argument words;
this constructor is where the byte narrowing occurs.

| Object offset | Observed contents |
| --- | --- |
| `+00` | Kind byte |
| `+04` | Vector header/allocator word, untouched |
| `+08`, `+0c`, `+10` | Vector begin, end and capacity-end pointers, initially zero |
| `+14` | First vector, three raw words |
| `+20` | Second vector, three raw words |
| `+2c` | Timestamp word |
| `+30` | Strength word |
| `+34` | Final flag byte |

Other bytes in the 56-byte allocation remain untouched. Timestamp/strength bits
are copied without arithmetic, preserving NaN payloads and signed zeros.

After initializing fields, the constructor calls virtual method `+2c` on the
manager at client global `00e0eb20` / server `00c3117c`. Its signed return fixes
the number of words copied; zero or negative counts skip copying. It does not
re-read the count after each insertion. Each word is read when appended, retaining
mutations made by callbacks. The vector state is also read after callbacks.

If capacity is available, it stores the word and advances the end pointer.
Otherwise it calls the reconstructed generic insertion helper on the vector at object
`+04`, with its current end, count 1 and a pointer to the next input word.

## Lookup behavior

Object lookup uses the handle's low 16 bits as a one-based index into eight-byte
records. Zero indices return zero without reading the pool. The record generation
at `+06` must match the handle's upper 16 bits; a match returns its first word,
including zero. Bounds and pool initialization remain caller preconditions.

Indexed interface lookup reads `object + 0x24 + index*4` with 32-bit wrapping.
The native guard tests whether `object + 0x24` is zero, not whether the incoming
object is null. Thus receiver `0xffffffdc` returns zero; a null receiver retains
the original invalid access rather than becoming a newly accepted input.

## Remaining services and verification

The insertion helper at client `00428200` / server `004297c0` now executes
reconstructed C and an authored x86 exception bridge. Its heap services, length
error and C++ exception runtime remain native. See [VECTORS.md](VECTORS.md) for
algorithm, ABI, exception metadata and verification limits. Manager methods,
event lifetime, destruction and allocator services also remain native dependencies.

`event_oracle.py` and `verify_events.py` compare 251 cases per binary: 176
constructor, 45 generation lookup and 30 indexed lookup cases. They check raw
object bytes and array words, return values and argument cleanup, untouched
padding, count signs, byte flags, float/NaN words, reserve/growth branches and
callback mutations. Most cases control insertion; eight enable the actual helper
with controlled heap services. These checks do not establish heap internals or
all Windows exception paths. The separate live vector probe validates selected
synchronous C++ cleanup/rethrow paths, as documented in VECTORS.md.

Three callback and three dispatcher cases additionally execute real constructor and
lookup code with controlled manager services. One of each also executes actual
vector insertion with controlled heap/memmove services. Eight direct constructor
cases enable insertion as well. The dispatcher cases also
execute real geometry. Their traces and event effects must match the original.
Live event lifetime and sustained-match behavior are not established by these
fixtures; full engine reconstruction remains incomplete.
