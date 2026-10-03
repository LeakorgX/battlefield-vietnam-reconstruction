# Binary analysis status

The current pass exports every internal function defined in the inspected Ghidra
projects, including import/tail-call thunks. This does not establish that every
function in the binaries has been identified or reconstructed.

| Result | Client | Dedicated server |
| --- | ---: | ---: |
| Defined functions | 38,657 | 23,986 |
| C-like outputs | 38,656 | 23,984 |
| Failed exports | 1 | 2 |
| New pointer-supported candidates | 12,927 | 7,827 |
| New aligned orphan candidates | 100 | 27 |
| Decoded nonpadding bytes outside function bodies | 131,706 | 73,728 |
| Executable bytes without decoded instructions | 568,299 | 364,766 |

The 62,640 outputs include overlapping client/server implementations and inferred
candidate functions. They are not 62,640 independently validated source functions.
New candidate boundaries require review. Unclassified executable bytes may include
padding, embedded tables, data or missed code; byte counts are not reconstruction
percentages.

## Recovery results

- Client `008099fd`: recovered with a 180-second timeout.
- Client `009d4ba0` and server `0078a500`: AI collision-handler exports still fail
  with an address-space range error. Three simplification/constant-inference
  profiles and an isolated calling-convention experiment did not resolve it.
- Server `0074696b`: fails with an input-varnode adjustment error under all three
  retry profiles. Its instruction evidence is preserved locally.

No failed function is replaced by fabricated C or marked as successful.
`reports/*/decompilation.tsv` is the export manifest; recovery reports record
individual retries. Discovery tables retain the pointer or prologue evidence.
`reports/*/coverage.json` and the gap tables record the remaining coverage audit.

## Reproduce

See [SETUP.md](SETUP.md). `analysis/Run-Analysis.ps1` extracts RTTI seeds, imports or
reuses the local Ghidra projects, discovers candidates, audits coverage and exports
all known functions. Outputs stay outside the source repository.

`RecoverFailedFunctions.java OUTPUT_DIR ADDRESS...` retries selected entries and
writes diagnostic XML. `InspectCollisionABI.java OUTPUT_DIR ADDRESS` is an
experimental prototype diagnostic and must be run with Ghidra `-readOnly`; its
inferred prototypes are not validated game interfaces.

Raw C-like outputs, diagnostic XML and instruction listings remain local. The
repository includes the tools and metadata needed to regenerate them from the
matching game installation. These exports still need type/layout recovery,
readable implementations and behavioral validation before they form a full
editable engine build.
