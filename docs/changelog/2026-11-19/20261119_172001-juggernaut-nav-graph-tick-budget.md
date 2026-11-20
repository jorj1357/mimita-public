# Juggernaut nav-graph tick budget and world-phase attribution

# Task

- Task ID: session-20261119-juggernaut-60hz
- Session ID: session-20261119-juggernaut-60hz
- Owner: opencode (juggernaut-60hz)
- Focus: Reach the handoff target of 67 moving Juggernaut NPCs with the
  authoritative server near 60 Hz, guided by live `events.jsonl`.
- Summary: Added bounded per-block world-simulation timing and all-NPC movement
  coverage, then bounded the custom NPC navigation-graph A* and its per-tick
  work. Baseline was ~20 Hz; after the A* budget the same workload reached
  ~40-63 Hz at 46-67 living NPCs. The 99.99%-within-16.667 ms target is not met.
- Status: in-progress (target not reached)
- Date, time, timezone: 2026-11-19T17:20:01+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85 (working tree; not committed)

# Evidence before change

- Run `20261119_151706`, mode 9 (`juggernaut`), `dust2cyberiav4`, server PID
  3240, generation 355, 67 NPCs. `performance.server-tick-window` recorded
  `npc_avg_ms` 29-35, p50 27-34 ms, max 82-105 ms, ~0-1 ticks over 100 ms per
  20-30 samples; `performance.server-loop-window` held ~25-37 Hz.
- The resident tick reports the whole world module as `npc_ms`; the new
  `server.world-phase-window` split it into `npc_sim_ms` (~640-720 ms/s) and
  `physical_contact_ms` (~180-265 ms/s) with the rest small.
- `npc.phase-window` at full load: physics ~197 ms, collision ~143-149 ms,
  decision ~70-98 ms, path ~60 ms, navigation ~17-30 ms, sense ~26-63 ms.
- The dominant outliers were `npc.slow-update` rows for actors 100064-100066
  with `navigator_ms`/`path_ms`/`global_graph_ms` up to 670 ms. `npc.nav.plan-cost`
  attributed it to `global_graph_ms` (670, 588, 512 ms), with Recast = 0.
- `npc.nav.graph-query-cost` attributed the 100-357 ms queries to
  `route_search_ms` (A*) across 3-4 expanding chunk-ring attempts over up to 204
  cached chunks; separate queries spent up to 323 ms in `chunk_build_ms`.
- Collision still used the legacy capsule fallback: `actor-collision.fallback`
  reported `body_part_count=0`, so `runActorTriangleCollisionStep` returned
  false (`actor-triangle-solver.cpp:1335`).
- Movement coverage before the change only sampled `npc.movement-decision` for
  `id % 8 == 0` (3 distinct actors), which cannot prove all 67 moved.

# Specification alignment

- `docs/specs/performance/performance.md`: fixed 60 Hz authoritative simulation;
  99.99% of fixed ticks at or below 16.667 ms; zero `performance.server-tick-stall`
  above 100 ms; bound per-tick work and avoid per-query hot-loop allocation.
- `docs/specs/npc-behavior/npc-behavior.md`: NPC movement and collision stay on
  the shared fixed-tick path; decisions may be staggered but not the simulation.
- Root dev-loop is the only build/run authority. No tick rate, roster, movement
  equation, or collision rule was reduced.

# Exact implementation changes

## Files

- `src/game/server-runtime-module.cpp`: bounded per-block timing in
  `serverRuntimeSimulateWorld` (`server.world-phase-window`: infinite collision,
  `simulateSharedNpcs`, position history, recorder, projectiles, hazards, melee,
  physical contact, world impacts) and one-second all-NPC movement coverage
  (`living_npcs`, `moved_npcs`, `stationary_npcs`).
- `src/npc/npc-nav-graph.cpp`, `src/npc/npc-nav-graph.h`: `findOneRoute` now takes
  a wall-clock budget (6 ms) and reports when it is exceeded; `findRoutes` stops
  expanding the chunk ring once a search exceeds its budget, and enforces an
  8 ms per-tick global-graph budget shared across actors. Chunk construction is
  restored to eager (a partial-build cap was tried and reverted; see below).
- `config/hot-reload/packet-ledger.json`: regenerated twice because concurrent
  packet-source edits made the committed ledger stale and blocked the dev-loop
  codegen check. No packet behavior changed.

# Build and validation

- Build notes queued: `87ba8fb0eb2b`, `7b426253f95f` (superseded),
  `024886c7f609`, `2212a4712a14`, `5261c3539cc4` (reverted), `b7dc14ddf466`.
- Published generations: 360 (world-phase instrumentation), 361, 362 (A* budget),
  366 (chunk-cap attempt), 367 (chunk-cap revert + A* budget). Each build result
  is recorded in `.dev/build-timings/` and the dev-loop `events.jsonl`.
- Runtime evidence, run `20261119_163648` (gen 362):
  - 67 living: `ticks` 27-44/s, `total_avg_ms` 20-32, `npc_sim_ms` 610-729/s,
    `physical_contact_ms` 210-296/s, `moved_npcs` 66-67/67.
  - 46-47 living: `ticks` 47-63/s, `total_avg_ms` 14-18, `npc_avg_ms` 13-18,
    p50 12-19 ms, `moved_npcs` 46/46.
- Runtime evidence, run `20261119_171754` (gen 367, 67 living): `ticks` 12-16/s,
  `total_avg_ms` 57-78 ms, p50 49-60 ms, `physical_contact_ms` 240-419/s,
  `moved_npcs` 66-67/67. This generation also contains unrelated concurrent
  edits, so it is not a clean A/B against gen 362.
- Negative result: capping chunk builds at 8 ms/query in gen 366 starved global
  routing onto the local planner and regressed the run to ~6-7 ticks/s; reverted.

# Remaining work and evidence separation

- Source: the A* budget and instrumentation are saved on disk; not committed.
- Build: gen 367 is published and active; build success is compile/link evidence
  only, not gameplay proof.
- Runtime: the nav-A* stall is removed (`global_graph_ms` outliers gone; search
  now bounded), and full-roster movement is measured. The 60 Hz tick-duration
  gate is not met.
- Next targets, in measured order: (1) `physical_contact_ms` ~240-420 ms/s
  (`tickServerAnimatedPhysicalContactWeapons` prepares full poses for every
  armed NPC and runs an O(n^2) contact pass); (2) `npc_sim_ms` ~550-720 ms/s
  (legacy capsule collision ~143 ms/s, decision ~70-98 ms/s, path ~60 ms/s);
  (3) remaining lazy nav chunk builds up to ~323 ms/query.
- Human acceptance (visual, play-feel, multiplayer) was not performed and must
  not be claimed.
- Concurrent-editor interference: another session edited `src/mapeditor/*`,
  `src/gui/*`, `src/main-systems.cpp`, `src/engine/engine-tick-state.cpp`,
  `src/network/server-gamemode.*`, `src/entities/player-animation.cpp`, and
  changed the dev-loop launch mode to 1 repeatedly. Those edits are preserved
  and are not claimed here.
