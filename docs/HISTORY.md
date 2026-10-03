# Progress history and scope corrections

## Initial request

The requested target was a full Battlefield Vietnam remake using Bevy, with
multiplayer as the priority, eventually clarified as an exact match to the original
unmodified game. The user wanted binary logic recovered so previously inaccessible
gameplay, AI and physics could be edited.

## Procedural prototype

A Rust/Bevy client/server conquest prototype was implemented, built and launched.
It used procedural geometry and custom simplified gameplay/networking. It was a
working separate game experiment but did not meet the 1:1 requirement. The user
rejected it. It remains in the repository as historical work, labeled accordingly.

## Archive/reference audit

Read-only tooling inventoried archives and extracted selected local reference
configuration/map information. This did not recover native engine logic. The user
directed the project to stop asset-focused work and concentrate on the executable.
The audit tool is retained; extracted game data stays local.

## Native analysis

Client/server RTTI and vtable recovery identified AI/gameplay function leads.
Ghidra exported 41,591 C-like bodies across the analyzed binaries, plus function
indexes and call graphs. Three decompilation failures were recorded explicitly.
This produced an analysis corpus, not buildable original engine source.

## Raw compilation failed

The user requested a compilation test. A 32-bit GCC attempt to compile the raw
interpreter export failed on missing types/globals and decompiler-only notation.
No game EXE was produced by that attempt. Work should not have stopped at this
failure; the user explicitly required editable and compilable code.

## Working native reconstruction/mod build

The interpreter was reconstructed into C with verified x86 interfaces. The build
appends compiled code to a copy of the original PE image and redirects guarded
vtable entries. Editable plan eligibility and rating rules were added.

Client/server builds succeeded, 3,968 scoped checks passed, and actual game/server
processes executed the compiled interpreter and vehicle wrapper repeatedly.
The user confirmed manually closing the last client test. This gives a practical
native modding path but still retains most original engine machine code.

## Current goal is unchanged

The user pointed out that a partial modding layer is not the requested full binary
source reconstruction and asked for a GitHub repository with a very clear status
and remaining-work description. This repository records both the useful partial
result and the unmet original goal. It does not label the engine complete or claim
the original game was released as open source.
