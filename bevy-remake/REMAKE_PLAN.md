# Full remake: remaining work

The runnable conquest build establishes the client/server boundary. It is not feature
or content parity with Battlefield Vietnam. The following work is still required.

1. **Original map conversion.** Inventory local RFA archives; create a bounded RFA reader
   and unpack to an ignored local cache. Convert heightmaps, terrain layers, static object
   placements, control points, spawn points and light settings into a versioned Bevy map
   format. The current install's selected map is `Oblivion` in `maplist.con`; do not confuse
   the procedural River Village test map with an imported original map.
2. **Asset conversion.** Convert proprietary standard meshes to glTF, textures to supported
   image formats, skeletons and animation clips into Bevy animation graphs. Resolve
   material names and scale/coordinate conventions. Keep original game assets local.
3. **Infantry parity.** Classes, loadouts, additional weapons, grenades, projectiles,
   damage locations, recoil/spread, first/third-person animation, spawn selection,
   collision/terrain physics, squad orders, audio and original HUD behavior.
4. **Vehicle parity.** Tanks, transports, helicopters, planes, boats; multi-seat ownership,
   turrets, projectiles, vehicle health, destruction, entry/exit and flight/water physics.
   The current jeeps demonstrate network ownership and ground motion only.
5. **Multiplayer production work.** Reliable game events and inventory transitions,
   snapshot delta compression, entity interest management, input redundancy,
   server-side lag compensation, jitter buffer, bandwidth/load tests, transport security,
   authentication, browser/lobby, discovery, NAT traversal and dedicated server administration.
   Raise the actor limit only after budgeting packet sizes and testing larger sessions.
6. **Bots and modes.** Navmesh/pathfinding on imported terrain; vehicle AI, class tactics,
   configurable teams, map rotation and original mode/rule variants.
7. **Release validation.** Multi-machine and adverse-network playtests, full match soak tests,
   Windows/Linux deployment builds, reproducible asset import, saved settings, accessibility
   controls and performance budgets on original-sized maps.

Suggested next milestone: import one original map with its terrain and control points,
then play a two-computer conquest round on it using this server.
