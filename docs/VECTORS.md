# Reconstructed word-vector insertion and exception bridge

`src/word_vector.c` reconstructs insertion of repeated four-byte values into the
vector used by collision events. It is called from the reconstructed event
constructor. Other native callers still use the intact original entries.

| Function | Client entry | Server entry | Original ABI |
| --- | --- | --- | --- |
| Insertion | `00428200` | `004297c0` | thiscall, 12 stack argument bytes |
| Copy words using memmove semantics | `00a6ad10` | `0041fbd0` | stdcall, 12 bytes, returns destination end |
| Fill count | `004ef090` | `005f83e0` | stdcall, 12 bytes, returns end |
| Shift range to destination end | `00a03770` | `007901d0` | fastcall, one 4-byte stack argument |
| Fill range | `006f7000` | `00705df0` | fastcall, one 4-byte stack argument |

The shift/range helpers use ECX and EDX for their first two arguments. The build
records their decorated source symbols. Names describe recovered operations.

## Algorithm and layout

The vector header's first word is preserved. Begin, end and capacity-end are at
`+04`, `+08`, and `+0c`. Insertion receives a position, a repeat count, and a pointer
to the value. It captures that value before checking whether the count is zero,
so aliases into the old allocation remain usable across copying and deallocation.
Pointer differences preserve the inspected 32-bit arithmetic and arithmetic shift.
Valid vector ranges and aligned pointers remain caller preconditions.

With insufficient capacity, the new capacity is the larger of required size and
old capacity plus half of old capacity, subject to the original `0x3fffffff`
limit/overflow branches. The code allocates, copies the prefix, fills inserted
words, copies the suffix, releases old storage, and updates header pointers.

With sufficient capacity, the tail's size determines which move/fill path runs.
A short tail moves past the insertion area before filling the remaining new
positions; a longer tail copies its final words, shifts the preceding range, and
fills the insertion area. The implementation preserves callback-visible header
updates and operation order. Copying uses overlap-safe word moves; fills read the
provided value each iteration, as the original helpers do.

## Windows C++ exception ABI

The original insertion registers an FS exception frame and two catch-all regions.
Its raw decompiler omitted `ADD ESP,4` after deallocation and a cleanup funclet,
following a mistaken no-return annotation. Inspection recovered those instructions
and the handler tables before reconstructing the code.

Readable C implements insertion and owns an explicit guard state. A small authored
x86 bridge creates the legacy registration frame, saves the stack pointer, restores
callee-preserved registers, and removes the FS record on normal return. The source
handler loads authored function-info metadata and tail-calls the retained CRT
frame handler. Metadata retains the inspected four states, two catch regions and
unwind-map layout. The guard's allocation pointer and state are volatile.

During reallocation, state 0 covers copying/filling after allocation; its catch
funclet releases the new allocation and rethrows. In the short-tail in-place path,
state 2 covers filling; its catch funclet rethrows. Native `FWAIT` boundaries and
state resets are preserved. The source catch funclets use the registered frame's
allocation field, rather than the omitted decompiler pseudocode.

| Retained runtime service | Client | Server |
| --- | --- | --- |
| Allocation | `00403610` | `00403d80` |
| Deallocation | `00403680` | `00403df0` |
| Length-error construction/throw | `00a894e0` | `0048e690` |
| C++ frame handler | `007c1b6e` | `0063a54e` |
| C++ throw/rethrow | `007c1fae` | `0063a98e` |

This recovers the insertion and its compiler-facing bridge, not the entire CRT.
The inspected binaries have no load-config/SafeSEH table. The build remains tied
to their fixed x86 layouts and runtime ABI; it is not a portable standalone build.

## Verification and remaining limits

`vector_oracle.py` and `verify_vectors.py` compare 507 cases per binary. They run
original and compiled insertion with controlled allocation/deallocation and reference
memmove effects, comparing helper calls, buffer contents, header words, allocation
sizes, guard states, preserved registers, stack cleanup and restored FS linkage.
Cases cover selected small vector sizes/capacities, all insertion positions,
repeated values, aliases, overlaps, zero counts, length-error routing and mutations.

Fault fixtures stop at selected copy/fill calls, read the registered function-info
tables, and dispatch the actual applicable catch funclet using the documented
frame convention. They compare release/rethrow arguments and states. A separate
handler check verifies metadata and forwarding to a controlled CRT handler.
This is simulated fault dispatch: actual Windows exception search, unwind through
outer frames, unmasked floating-point exceptions and allocation failure inside a
live game are not established by these checks.

Eight event-constructor cases enable real insertion (six invoke it), and one
callback/one dispatcher case executes it with controlled heap/memmove services. The dispatcher includes real
geometry. Event lifetime, destruction, heap internals and full-game equivalence
remain outside this verification scope. Reconstructing and validating those paths
is still required for the full-engine goal.
