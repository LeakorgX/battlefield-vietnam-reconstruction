# Verification: what passed and what it means

## Three distinct sets of evidence

1. Initial analysis exported native functions. This was an analysis result, not a
   compilation or runtime correctness test.
2. Early reference oracles ran 192 original-code cases per binary, 384 in total.
   They established selected native paths but did not run newly compiled logic.
3. The current native build was compared against original instructions. It passed
   9,613 scoped checks per target, 19,226 in total. Earlier builds also executed
   replacement functions in the game, as recorded below.

## Current controlled cases

- 1,920 interpreter comparisons per target, covering selected plan/object types,
  early return, disabled/allowed masks, fallback/override routing, returned flags,
  context callbacks, event components and cleanup decisions.
- 32 cached-bailout comparisons per target checking exact float32 bits.
- 554 recomputed-bailout comparisons per target checking float32 return/cache bits,
  pattern flags, call ordering, group argument and stack cleanup. They cover
  influence conditions, low-byte predicate results, metric boundaries, signed zero,
  scale factors, changing selectors and cache/flag pointers moved by callbacks.
  Reference helpers are native; rebuilt helpers execute reconstructed C with the
  same deterministic table fixtures. Object methods
  are controlled. See [BAILOUT.md](BAILOUT.md) for the recovered interface.
- 3,840 curve-helper comparisons per target checking the exact 80-bit x87 return,
  stack cleanup, an empty x87 stack and control-word preservation. They cover
  float32 neighbors, subnormals, infinities, NaN payloads, signed zeros, all four
  rounding directions, three precision modes, synthetic tables and tables produced
  by original initializers. The neighboring word past each 101-element allocation
  is controlled, including NaN cases. Unmasked exceptions and other status flags
  are not checked. These helper comparisons do not establish whole-game parity.
- 32 vehicle-wrapper ABI cases per target with a controlled native callee checking
  argument forwarding and returned float bits. These do not evaluate the real
  vehicle-scoring algorithm on fabricated incomplete objects.
- 129 collision-callback comparisons per target with controlled registry and
  allocator services. Most control constructor behavior; three run real event
  construction and lookups with controlled manager methods; one also enables real
  insertion with controlled heap services. Exact receiver/argument bytes,
  call order, vector/timestamp mutations, record-counter wraparound, state predicates
  and vtable-pointer/slot mutations are checked. Client/server handle-field offsets
  are distinct. See [COLLISION.md](COLLISION.md); real collision-service behavior
  and a live invocation of the new callback are not established by these fixtures.
- 64 direct notification-helper comparisons per target, checking full-word state
  comparison, upper flag-bit preservation, selectors, callback state mutations,
  the zero-argument position getter, five dispatcher argument words and stack
  cleanup. Callback comparisons also execute the original notification helper in
  the reference run rather than replacing it with a stub. Both callback/helper
  runs execute their dispatcher with an empty actor list.
- 148 direct dispatcher comparisons per target check repeated selector reads,
  low-byte state predicates, handle generations, absent entities, live count
  changes, distance threshold equality/NaNs, allocation failure, full flag words,
  vector mutations, call order and 20-byte argument cleanup. Thirteen cases execute
  the original/reconstructed distance calculation, including a changing actor list
  and vector mutations; the remaining cases control the geometry result. Event
  services are controlled except for three cases that execute real event construction
  and lookup helpers, with controlled manager methods. One also enables real vector
  insertion with controlled heap services.
- 2,136 geometry comparisons per target check the full 80-bit result, 36-byte
  argument cleanup, empty x87 stack and preserved control word. Both line-distance
  squared and planar segment distance run under all four rounding directions and
  three precision modes. Inputs cover endpoints, interior projections, zero-length
  segments, random vectors, float32 extremes, subnormals, infinities, signed zeros
  and selected NaN payloads. See [GEOMETRY.md](GEOMETRY.md).
- 251 event comparisons per target: 176 constructor, 45 pool lookup and 30 indexed
  interface lookup cases. Constructor checks compare all 56 object bytes, copied
  array words, callbacks, untouched padding, raw float/NaN bits, byte flags, signed
  word counts and 44-byte argument cleanup. Most cases control insertion, including reserve/growth paths and input mutations. Lookup checks include
  generation mismatch, zero handles/entries and wrapped interface-table addressing.
  Eight constructor cases enable original/reconstructed insertion with controlled
  allocation/memmove. See [EVENTS.md](EVENTS.md). Three callback and three dispatcher
  cases execute these real constructor/lookups; one of each also executes insertion.
  The dispatcher cases combine them with real geometry.
- 507 vector/exception-bridge comparisons per target check insertion at every
  position in selected small vectors, growth and capacity paths, aliased values,
  overlapping copies, zero counts, length-error routing, callback mutations,
  callee-preserved registers and FS registration restoration. Simulated faults
  compare handler tables and execute the actual catch funclets, checking release
  and rethrow arguments. Handler thunks forward to a controlled CRT handler.
  These emulator checks do not establish real Windows exception search/unwind
  behavior; the separate live check below validates selected synchronous paths.
  See [VECTORS.md](VECTORS.md).

External virtual methods, event receivers and context/list helper bodies use
controlled fixtures. Original and reconstructed interpreter instructions receive
the same fixture. Flags/results/call sequences and stack cleanup are checked.

## Important corrections found through instruction review

- Fallback interpreter dispatch pushes three arguments, although the initial
  decompiler export incorrectly shows two.
- Bailout rating uses three stack arguments, including a float multiplier.
- The context-record getter itself takes no stack arguments. The next context
  virtual call consumes four arguments. Treating the getter as consuming three
  caused the original fixture to fail and was corrected from the assembly.
- Vehicle-change scoring performs native candidate evaluation even when the
  second argument is zero. It must not be replaced with a cached-table getter.
- A candidate function pointer into read-only data was rejected by requiring
  executable-section membership before creating a function seed.

These illustrate why a raw C-like export is not sufficient source reconstruction.

Numerical comparisons also caught a directed-rounding error from replacing a
negative multiplication with a sign change, and a coefficient promoted with more
precision than its original float32 value. `-frounding-math` and an explicitly
stored float32 coefficient preserve the inspected arithmetic. Emulated x87 state
is initialized with an empty register stack; the helpers must leave it empty
after their return value is captured.

## Runtime observations

An earlier build of both generated executables started and executed replacement functions. Read-only
process inspection checked the executable identity, all three patched slots and
1,128 immutable compiled payload bytes. Client counters reached 545,378 interpreter
calls and 3,523 vehicle-wrapper calls; server counters reached 5,553 and 48.
The cached/recomputed bailout wrapper was not observed executing in the retained
runtime samples; its counter was zero. These observations predate the C bailout
recomputation implementation and do not establish live execution of that new path.

The user confirmed manually closing the latest client. The temporary server was
stopped after verification. Raw logs and local profile data are not published.
Sanitized observations are in `reports/*/runtime-observation.json`.

The tested installation contains modified archives and a custom map. This runtime
sample is not proof of vanilla content equivalence, sustained complete-match
stability, every AI plan's correctness or all-game compatibility.

## Live Windows vector/exception checks

The current build additionally passed 16 live executions: original and reconstructed
insertion, four scenarios each, across client and server. This is eight paired
comparisons and is separate from the 19,226 emulator checks. Normal insertion,
length errors, protected growth failure and protected in-place filling failure
matched. Actual native C++ exceptions propagated to an outer frame, failed growth
released the guard's exact allocation once, and FS linkage was restored.

`verify_live_exceptions.py` uses its own temporary processes with game threads paused
and controlled fault calls. The actual game heap and C++ runtime execute. It checks
file hashes, native insertion bytes, immutable payload bytes and patched slots;
restores temporary call patches; and stops its child afterward. Published reports
contain no profile data or process logs. See [VECTORS.md](VECTORS.md) for reproduction
and limits. These checks do not prove sustained matches, asynchronous faults,
allocator exhaustion, all exception types, vanilla data parity or event lifetime.

## Decompiler failures

- Client AI collision handler at `009d4ba0`: address-range error.
- Server counterpart at `0078a500`: the same address-range error.
- Client function at `008099fd`: recovered after raising the timeout to 180 seconds.
- Server candidate at `0074696b`: input-varnode error; boundary is disputed.

Instruction listings exist in the local research corpus. A retry without read-only
constant-pointer assumptions did not repair the server collision-handler failure.
The remaining failures and the boundary dispute remain explicit in the exported
status manifests and `reports/server/boundary-review.tsv`.

## Reproducing and extending tests

See SETUP.md for required inputs and the verification command. Run the default
rules before introducing changes. For a deliberate modification, record the old
and desired new outcomes and write checks for the changed behavior. Keep original
behavior comparisons for code intended to remain equivalent.

New systems need native reference traces, boundary cases, lifecycle cases and
interaction checks. Passing existing interpreter tests says nothing by itself
about a new artillery/physics/networking implementation.
