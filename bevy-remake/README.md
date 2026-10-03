# Vietnam / Bevy

A new Rust/Bevy 0.19.1 multiplayer conquest implementation alongside the installed
Battlefield Vietnam game. This is a working foundation for a remake, **not a complete
Battlefield Vietnam replacement**. It does not patch or run the original executable.
The current map, soldiers, rifle and jeeps use procedural placeholder geometry.

## Play

From PowerShell in this directory:

```powershell
.\Start-Game.ps1
```

This builds both executables and launches the client with a local server and eight bots.
The client owns that local server process and stops it when the client closes.
Rust stable and the Microsoft C++ Build Tools/Windows SDK are needed for compilation.
The first build compiles Bevy and can take several minutes. Later launches can use
`-SkipBuild`. The binaries are in `target/debug`.

For LAN multiplayer, run a dedicated server on one computer:

```powershell
.\Start-Server.ps1 -Bots 8
```

On each player's computer, use the server's LAN address:

```powershell
.\Start-Game.ps1 -Connect '192.168.1.10:23000'
```

Allow UDP 23000 on the server's firewall if prompted. No firewall rules are changed by
these scripts. `--host` binds a private local server to localhost; use the dedicated
server script to accept LAN players. Internet matchmaking, NAT traversal, encryption
and server discovery are not implemented. The current transport is intended for local
development and trusted LAN play.

## Controls and conquest

| Control | Action |
| --- | --- |
| Left click | Capture mouse / fire |
| Mouse | Aim |
| W A S D | Move |
| Shift | Sprint |
| Space | Jump |
| R | Reload (2 seconds) |
| E | Enter a nearby jeep / exit |
| W / S in jeep | Forward / reverse |
| A / D in jeep | Steer |
| Tab | Scoreboard |
| Escape | Release mouse and stop input |
| F10 | Quit |

Teams are assigned to balance player counts. Stand within nine metres of a flag for
eight seconds to capture a neutral flag; enemy ownership must first be neutralized.
Both teams present stops capture. Holding at least two flags drains opposing tickets.
Deaths cost one ticket; rifle hits do 34 damage. Respawn takes four seconds. The first
team to exhaust its tickets loses, and the next round starts after twelve seconds.
Bots capture flags, navigate around obstacles, and shoot visible enemies. A match has
at most sixteen actors; human players replace bots when the server is full.

## What is implemented

- Dedicated headless Bevy server: 60 Hz simulation and 20 Hz snapshots.
- UDP handshake retries, endpoint-bound random session tokens, version rejection,
  monotonic input sequences, stale-input stop, idle disconnect and client reconnection.
- Server-owned movement, bounds/collision, rifle hits, cover occlusion, ammo/reload,
  health, kills/deaths, respawning, capture progress, tickets and round reset.
- Client horizontal prediction with acknowledgement replay; remote soldier interpolation.
- First-person view, procedural village, foliage, fog, flag rings, shot tracers, two jeeps,
  health/ammo UI, minimap and scoreboard.
- Bounded packet parsing and receive work. Full sixteen-actor snapshots fit within a
  1200-byte UDP payload, verified by a test.

## Architecture and validation

`crates/shared` owns the protocol, collision map and deterministic gameplay rules.
`crates/server` hosts that simulation in Bevy's fixed schedule, without a GPU or window.
`crates/client` renders the world and sends input, never damage or authoritative positions.
Horizontal prediction is shared with the server. Jumping and vehicle camera positions
are authoritative, so these have more visible latency than walking. There is no lag
compensation for hitscan yet. Collision currently uses simple static boxes; vehicles
have one seat, no suspension and no damage model. Bots use local steering, not a navmesh.

```powershell
cargo test --locked -p bfv-shared -p bfv-server
cargo check --locked --workspace
cargo fmt --all -- --check
```

The server integration test launches a real headless server and two independent UDP
clients, then checks replicated movement/combat, wrong-token rejection, disconnect,
rejoin and protocol rejection. Optional hidden graphical smoke run:

```powershell
.\target\debug\bfv-client.exe --host --smoke --exit-after 10 --screenshot smoke.png
```

## Completing the remake

See [REMAKE_PLAN.md](REMAKE_PLAN.md) for the outstanding original-content and feature
work. The original RFA archives and proprietary asset formats are not loaded by Bevy.
No original textures, meshes, animations, music or maps have been converted yet.
The separate `universal-modder-main` toolkit is left untouched.

Bevy release reference: https://bevy.org/news/bevy-0-19/

