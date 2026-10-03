# Native executable recovery

This workflow reads the executable; it does not process game data archives.
Output belongs in `bfv-reference-local`, outside this source repository.

1. Run `analysis/tools/binary_logic.py` with `capstone` and `pefile`. It records the executable
   hash and finds MSVC x86 type descriptors, complete-object locators and vtables.
2. Import the executable into a separate Ghidra project and run native analysis.
3. Run `ExportNativeLogic.java OUTPUT_DIR FUNCTION_SEEDS_TSV` as a post-script.
   This resolves/creates the seeded functions, exports the full function index,
   decompiles AI/gameplay seeds plus direct callees, and writes their direct call graph.
   Add the optional third argument `all` to include every detected internal function.
   Existing exports are reused; each failure remains visible in the manifest.
4. `InspectNativeFunctions.java OUTPUT_DIR ADDRESS...` exports instruction listings
   and inferred signatures for functions requiring ABI review or decompiler repair.
5. Run `analysis/tools/native_oracle.py` with `pefile` and `unicorn` to execute controlled
   native AI paths. Use `--exe bfvietnam_w32ded.exe` for the dedicated-server image.

`rtti-vtables.json` preserves class/vtable/slot evidence and alias names.
`seed-resolution.tsv` reports which addresses actually resolved to functions.
`decompilation.tsv` reports every success and failure. Failed functions are not
silently represented as successfully recovered code. Each `.c` file includes the
input binary hash and address for verification against the assembly.

Recovered C-like output is an analysis reference. It is not the original engine's
source tree, and it does not become a buildable engine just by exporting it. Struct
layouts, indirect calls, ownership, signatures and runtime behavior still need
recovery before the code can be ported faithfully. Class/slot labels identify
membership; they do not claim the original method name was recovered.

`AuditHistoryTree.java OUTPUT_TSV` checks the six history-tree replacement entries
in each supported binary. Run it against an already analyzed project with
`-noanalysis -readOnly`. It verifies the original hash, indexed function sizes,
complete patch instructions and recorded references into those instructions.
It writes metadata only; no native function bodies are included in the report.

`AuditArtilleryWeapons.java OUTPUT_TSV`, also read-only, checks the inline
first-pass weapon-selection block's ownership, instruction boundaries, branch
exits and recorded incoming references. This block remains part of a partially
reconstructed evaluator; it is not counted as a separate complete function.
