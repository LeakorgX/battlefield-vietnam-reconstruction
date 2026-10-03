# AI collision callback reconstruction

`bfv_collision` in `bfv-native-code/src/native_collision.c` recovers the control
flow of `AICollisionHandler` vtable slot 1: client `009d4ba0`, server `0078a500`.
The raw decompiler still fails on these functions. This implementation was written
from their instruction listings and checked against original instructions; it is
separate from the generated decompilation corpus.

The build redirects the hash-guarded client slot at `00bf8c64` and server slot at
`008775cc` to compiled C. This adds a fourth replacement alongside the interpreter
and the two rating methods. `bfv_collision_calls` records diagnostic invocations.

## Callback behavior

The callback receives the handler in ECX and three stack arguments: source object,
other object and an opaque 32-bit payload. It pops 12 bytes. The C interface is
void: the original exits leave unrelated query/helper values in EAX rather than
a consistent callback result; tests compare effects instead of treating EAX as
a returned gameplay value.

It queries the source interface and registry, obtains a member body and
component, then follows one of these paths:

- A missing other object produces a native notification with selector `-1`.
- A failed source-interface query with another object present queries that other
  interface and exits without further processing.
- With two objects, it looks up actor states. Their low-byte predicates determine
  whether to create/attach an event, increment a record counter, and forward the
  collision payload to the source actor.
- Event creation resolves a component handle, updates a pool entry's timestamp,
  queries its event interface and attempts a 56-byte allocation. Success constructs
  the event; failure still attaches a null event and queries the resulting selector.
- The final notification receives the original registry member, source component,
  source object and selector, retaining `-1` when no event interface was available.

This is an AI callback coordinating collision events. The collision detector,
physical response, actor classes and other collision-handler methods are not
reconstructed by this change.

## Recovered layout and ABI

| Interface | Offset | Observed use |
| --- | --- | --- |
| Source object | `+48` | Word forwarded with other object and payload |
| Object | `+4c` | Interface queried at virtual method `+2c` |
| Source object | client `+164`, server `+15c` | Registry handle |
| Converted body view | `+5c` | Component pointer |
| Registry member | virtual `+38` | Member body |
| Actor | virtual `+c4` | Low-byte state predicate |
| Actor | virtual `+d4` | Component selector |
| Component | virtual `+5c` | Selector-to-handle lookup |
| Actor | virtual `+88` | Record lookup; counter at record `+30` |
| Other actor | virtual `+144`, `+a8` | Attach event, obtain selector |
| Source actor | virtual `+148` | Forward source word, other object and payload |
| Event interface | `+08` | Opaque data passed to event constructor |

Names describe observed use and are not recovered original source identifiers.
The constructor receives two 12-byte vectors by value, opaque event data, a flag,
timestamp, float scale and final word. Its `thiscall` argument cleanup is 44 bytes.
Vectors are copied as raw words so NaN payloads and signed zeros are not changed
by C floating-point conversions. The opaque event-data argument is not the new
event allocation pointer; overlapping native stack offsets initially obscured it.

The callback captures component/pool vtable pointers before certain callbacks,
but reads the method slot afterward. Source preserves that distinction: a callback
may replace an object's vtable pointer or alter a slot in the saved table.
Position vectors are copied after the timestamp callback, retaining its mutations.

## Retained native services

| Service | Client entry | Server entry |
| --- | --- | --- |
| Body view conversion | `0066b190` | `00564460` |
| Pool entry lookup | `00926af0` | `006e8930` |
| Event interface lookup | `0092afb0` | `006e88b0` |
| Allocation service | `00412ee0` | `00404290` |
| Event constructor | `009dc3b0` | `007ae550` |
| Notification helper | `009d4b60` | `0078a4c0` |

Object virtual methods and registry/global initialization also remain native.
`tools/build.py` records the version-specific addresses and handle field offset.

## Verification and limits

`collision_oracle.py` and `verify_collision.py` compare 118 controlled cases per
binary. Tests record exact call order, receiver pointers, argument bytes, vectors,
timestamp arguments, event-allocation side effects and record-counter wraparound.
They check stack cleanup and cover absent inputs/lookup results, low-byte state
predicates, allocation failure and callback mutations of vectors/vtable pointers
and slots. Distinct sentinel values in the two possible handle fields catch
accidentally sharing the client layout with the server.

These services are controlled stubs in both runs. The tests establish callback
control flow and its observed service ABI, not correctness of the native services,
real collision responses, sustained-match behavior or every invalid-pointer path.
When both source and other are null, the original dereferences the source; the C
implementation preserves that precondition instead of adding a gameplay change.
No live invocation of the new collision replacement has yet been retained.
