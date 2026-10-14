2026 10 06 1704 jorj todo  explain ok  

idk this not really the right place for it but  idk this hte plan so i dotn wnan forget 


Recast marks walkable triangles via norm[1] (Recast.cpp:353); MiMITA is Z-up (gravity on .z, floor normal (0,0,1)). recast-navigation.cpp feeds raw Z-up triangles, so floors get norm[1]=0 → no walkable area → path_not_found. I'll prove this before changing anything by adding one bounded diagnostic that reports Recast's walkable-triangle/region/poly counts and the start/dest nearest-poly refs, then read it live.
P0 — Unblock Recast (coordinate + diagnostics)
Files: src/npc/recast-navigation.{h,cpp}, src/npc/npc.cpp, src/network/server-npcs.cpp.

1. Add explicit Z-up→Y-up conversion (handedness-preserving: (x,y,z)→(x,z,-y)) applied to vertices, bmin/bmax, query start/dest, extents; inverse on returned points. Record a coordinate_system value in the navmesh version.

2. Add bounded stage diagnostics: geometry tri count, Recast bounds, width×height, walkable-area count, region count, poly count, start/dest poly refs + nearest points, corridor size, and the exact failure stage (nearest-start / nearest-dest / findPath / straightPath). Emit as npc.nav.diag-style state-change events only.

3. Fix semantics: prepare() currently does a fake center→center query. Make bake-success report build stats (polys/regions/walkable area); report route success separately. Keep events bounded (no per-tick spam).

4. Build MIMITA_EXE_NAME=mimita-<ts>-recast-coord-v1.exe python build_agent.py; --versioninfo; run MIMITA_NPC_NAV_COMPARE=1 mimita-...exe --server --bind 127.0.0.1:0 --map dust2cyberiav4 --gamemode counterstrike --npcs 2 --timeout 5 --no-map-rotation --no-discord-notification; read events.jsonl.

Gate: real Dust 2 long query returns a corridor/corners; or, if not, the first failing stage is explicitly identified.

P1 — Isolated route proof on real Dust 2
Reuse the adapter under an env gate (no new self-test framework unless needed): same-floor, around a building, ramp/stairs, invalid start/dest, no-path, and the previously-failing long route. Keep isolated-query evidence separate from NPC-movement evidence.

P2 — Recast route → existing movement (still compare-safe, then per-preset recast)
Files: src/npc/recast-navigation.*, src/npc/npc-navigator.cpp, src/npc/npc.cpp, src/npc/npc-traversal.*.

1. Extend RecastNavigationResult to expose next corner + corridor (already returns points).

2. Add an explicit backend selector (custom/compare/recast) — config/dev override, not 
inferred from mode name. Default stays custom; compare preserves current behavior.

3. In recast mode, convert Detour's next corner into the same result.dir/waypoint shape npc-navigator.cpp already produces (npc-navigator.cpp:1164), so npc.cpp movement arbitration and physicsMainUpdate (npc.cpp:2072) are untouched.

4. Add corridor retention, replan on stale navmesh/blocked/no-progress, and bounded fallback to custom/hold. MiMITA keeps physics, jumping, collision.
Gate: one real NPC reaches a real destination in the executable with no teleport/bypass, proven in events.jsonl.

P3 — NPC behavior + core round/bomb loop (CS spec alignment)
Reuse existing owners; add no parallel brain.
- TeamBrain (src/npc/team-brain.*): site selection, attack/defend assignment, rotations, retake, anti-stacking, teammate-death response.
- ActorBrain (src/npc/npc-utility.*, npc-behavior.*, npc-state-machine.*): goal kinds (plant/defuse/hold/rotate/hunt-LKP/cover/retreat/search), patience, combat interruption + objective resumption, deterministic route diversity.
- Combat (src/npc/npc-combat.*): reaction delay, aim error/smoothing/leading, weapon selection, reload logic — driven by npc_behavior_profile.
- Round/bomb loop (src/network/server-gamemode.*, src/gamemode/*, config/gamemodes/counterstrike.json, config/maps/dust2cyberiav4.json): verify/complete one-life round flow, bomb assign/drop/pickup/plant/defuse/explode, and synchronized 3-2-1-GO freeze.

Evidence discipline (every executable change)

Separately report: source owners/functions · newly named exe build · --versioninfo + exact EVENTS_JSONL_PATH · event chain in that file · focused checks · human visual acceptance. A build never counts as behavior proof.
Acceptance targets

Use the contract thresholds (100% routes resolved or explicit hold; ≥95% positive progress; zero unrecovered wall/collision loops; ≤1 reversal/30s; ≥90% reach tactical dest before first engagement; ≥90% resume after combat).

I'm ready to implement, starting with the P0 diagnosis event and coordinate fix.