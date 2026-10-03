# Verification: what passed and what it means

## Three distinct sets of evidence

1. Initial analysis exported native functions. This was an analysis result, not a
   compilation or runtime correctness test.
2. Early reference oracles ran 192 original-code cases per binary, 384 in total.
   They established selected native paths but did not run newly compiled logic.
3. The current native build was compared against original instructions and tested
   in the game. It passed 1,984 scoped checks per target, 3,968 in total.

## Current controlled cases

- 1,920 interpreter comparisons per target, covering selected plan/object types,
  early return, disabled/allowed masks, fallback/override routing, returned flags,
  context callbacks, event components and cleanup decisions.
- 32 cached-bailout comparisons per target checking exact float32 bits.
- 32 vehicle-wrapper ABI cases per target with a controlled native callee checking
  argument forwarding and returned float bits. These do not evaluate the real
  vehicle-scoring algorithm on fabricated incomplete objects.

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

## Runtime observations

Both generated executables started and executed replacement functions. Read-only
process inspection checked the executable identity, all three patched slots and
1,128 immutable compiled payload bytes. Client counters reached 545,378 interpreter
calls and 3,523 vehicle-wrapper calls; server counters reached 5,553 and 48.
The cached/recomputed bailout wrapper was not observed executing in the retained
runtime samples; its counter was zero.

The user confirmed manually closing the latest client. The temporary server was
stopped after verification. Raw logs and local profile data are not published.
Sanitized observations are in `reports/*/runtime-observation.json`.

The tested installation contains modified archives and a custom map. This runtime
sample is not proof of vanilla content equivalence, sustained complete-match
stability, every AI plan's correctness or all-game compatibility.

## Decompiler failures

- Client AI collision handler at `009d4ba0`: address-range error.
- Server counterpart at `0078a500`: the same address-range error.
- Client function at `008099fd`: decompiler timeout.

Instruction listings exist in the local research corpus. A retry without read-only
constant-pointer assumptions did not repair the server collision-handler failure.
The failures remain explicit in the exported status manifests.

## Reproducing and extending tests

See SETUP.md for required inputs and the verification command. Run the default
rules before introducing changes. For a deliberate modification, record the old
and desired new outcomes and write checks for the changed behavior. Keep original
behavior comparisons for code intended to remain equivalent.

New systems need native reference traces, boundary cases, lifecycle cases and
interaction checks. Passing existing interpreter tests says nothing by itself
about a new artillery/physics/networking implementation.
