# NPC performance handoff — server sub-stage timing and presentation budget

Recorded: 2026-11-17 ~15:40 EST  
Status: source changes implemented; game generation 328 published; EXE build
worker reported success, but the dev-loop artifact/state was not yet confirmed
when this handoff was written. No runtime or human acceptance yet.

Authority: `AGENTS.md`, `docs/ROUTER.md`, the routed performance and dev-loop
specifications. If this handoff disagrees with those documents, they win.

## Objective

Reach the original performance target on payload funworld3 with all NPCs:

- client: 240 FPS, at most 4.17 ms per frame for 99.99% of a 60-second window;
- server: 59.9–60.1 Hz for 99.99% of windows;
- NPC movement, collision, damage, and firing remain on the fixed 60 Hz path;
- restore local keyframed animations and keep NPC presentation acceptable.

## Current build and runtime state

- Dev loop is running: run `20261117_170234`.
- Server PID `12272` is alive on the old runtime artifact.
- Client-04 PID `5152` is alive; it may be stale but its journal is updating.
- Game generation `328` is published:
  `build/game/generation-328/game-g328.dll`.
- The game compile reached 642/643 with zero failures and the link completed in
  about 36 s.
- `build/progress.json` reports the cold EXE build worker as
  `phase=build_complete`, `result=success`, 2 compiled, 85 skipped, 51.5 s
  compile, 2.25 s link, updated `2026-11-17T20:38:12.949Z`.
- `.dev/state.json` still reported `status=building`, `active_gen=96`, and the
  old `latest_artifact_id=2026-11-17T17:02:29Z`. Treat the artifact publish
  state as unconfirmed until the next agent checks it. Do not infer acceptance
  from the build worker result alone.

Do not run `build.py`, `game_build_worker.py`, a compiler, `--request build`,
`--request game-dll`, `--request cold-link-trial`, or `agent-gate.py`. The root
dev loop owns builds. `agent-gate.py` stops the dev loop before testing.

## Implemented source changes

### 1. Server NPC physics sub-stage timing

Files:

- `src/physics/physics-mini.h:22` — `PhysicsStageCost` and
  `physicsStageCost()`.
- `src/physics/physics-mini.cpp:49` — thread-local stage record.
- `src/physics/physics-mini.cpp:213-552` — timers for config, move-pre,
  collision sweep, move-post, facing, procedural animation, and total.
- `src/physics/physics-mini.h:45` — optional `updateAnimation` argument;
  default `true`, so player callers are unchanged.

The NPC reports these fields in `npc.slow-update` and `npc.phase-window`:

- `physics_config_ms`
- `move_pre_ms`
- `collision_ms`
- `move_post_ms`
- `facing_ms`
- `animation_ms`
- `aim_ms`
- `pose_containment_ms`
- `animation_skipped`
- `aim_skipped`

Caveat: early returns from `physicsMainUpdate_Internal` (`dt <= 0`, flight,
roll/ragdoll paths) leave `totalMs` at zero. NPC normal movement is measured.

### 2. Server-only NPC presentation budget

Files:

- `src/npc/npc.h:350-351` — tick begin and presentation-budget exhaustion.
- `src/npc/npc.cpp:60-72` — thread-local budget state.
- `src/npc/npc.cpp:3272-3290` — 4 ms animation/aim budget and 2-tick stride.
- `src/npc/npc.cpp:3304-3311` — skip the NPC hybrid aim solve when presentation
  is not due.
- `src/network/server-npcs.cpp:1941` — reset once per server tick.
- `src/network/server-npcs.cpp:2371` — exhaust optional presentation after 10 ms
  of cumulative NPC update work.

Decision implemented:

- Idle server NPCs animate procedural pose and aim at 30 Hz.
- Server NPCs with a target animate procedural pose and aim at 60 Hz.
- Movement, collision, damage, and firing always run at 60 Hz.
- Client/local NPC simulation does not enable the budget and stays at 60 Hz.
- Only optional presentation is skipped. No whole-NPC update or movement tick is
  skipped, so NPC speed is not intentionally reduced.

Tradeoff to verify: idle NPC pose/collision meshes can be one tick stale when
presentation is skipped. Validate idle pushes, wall containment, and final-pose
contacts before accepting. If this causes visible or physical regressions, set
`kNpcAnimationStride` to `1` or stop skipping the aim solve first.

### 3. NPC physics substeps

`src/npc/npc.cpp:3280` now calls `physicsMainUpdate(..., 1, navMovement, ...)`
instead of two substeps. The shared collision solver still adds speed-derived
substeps. Validate maximum-speed NPC movement for tunneling before accepting.

### 4. Compile fixes needed by the in-progress networking split

These are unrelated to the NPC performance work but blocked the build:

- `src/engine/engine-tick-state.cpp:294` — local `using namespace MimitaNet;`
  before the new `NetworkEvents` calls.
- `src/network/server-packet-chat.cpp:1494` — peer health transport now reads
  `player.transportHandle` instead of the removed `player.transport`.
- `src/network/server-packet-chat.cpp:1596-1599` — timeout close now uses
  `mimita::game::closeSession(transportHandle)` and clears the handle.
- `src/network/server-packets.cpp:2296` — reconnect log uses
  `*claimedTransport != kNoServerTransport` instead of `claimedTransport->get()`.

Review these changes with the networking hot/cold owner. They are build fixes,
not proofs that the networking cutover is correct.

### 5. Documentation updates

- `docs/specs/performance/performance.md`
- `docs/features/performance/fps-improvement-ledger.md`
- `docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md`

Earlier work already added perf overlay modes, server RX Hz reporting, idle
final-pose cadence, 500 ms hot-reload polling, weapon AABB distance guard,
projectile catch-up caps, and navigator budgets. Do not redo those changes.

## Validation already attempted

Passed:

- `python devscripts/quality/test_manual_build_guard.py -v` — 4 tests.
- `python devscripts/quality/test_path_containment.py -v` — 6 tests.
- `python -m unittest devscripts.dev_loop.test_artifacts -v` — 4 tests.
- `python devscripts/weapon_json_sync.py --check` — pass.
- `python devscripts/gamemode_json_sync.py --check` — pass.
- `python devscripts/packet_registry_codegen.py --check` — pass, 78 packets.
- Game compile: 642/643, zero compile failures; generation 328 published.

Failed / not applicable:

- `python devscripts/quality/lint.py --gate` failed against its accepted
  baseline with aggregate increases not attributed to these files:
  damage-authority `93 > 81`, weapon branches `43 > 41`, dead-code `9 > 8`,
  duplicate windows `569 > 551`. Do not update the baseline just to pass.
- `clang-format`, `clang-tidy`, `cppcheck`, and `jscpd` are not installed.
- No C++ selftest or runtime measurement has been run against generation 328
  yet.
- `agent-gate.py` was deliberately not used because it stops the dev loop.

## Critical build hazard

`config/modules/modules.json` is absent. `build.py` therefore depends on
`build/source-manifest.json` to select the cold EXE source set. If that manifest
is absent or stale, `exe_source_relpaths()` falls back to the legacy all-source
set and the EXE link hits duplicate `Debug::*` and coordinator definitions.
That failure consumed several long builds.

Before any build check:

- confirm `build/source-manifest.json` exists;
- confirm the cold source count is about 45, not 643;
- do not delete the manifest;
- do not run a build manually to work around the manifest.

## Next steps, in order

1. Check the current state:
   - `python devscripts/dev-loop.py --status`
   - `python devscripts/dev-loop.py --list-instances`
   - inspect `.dev/state.json` and `build/progress.json`.
2. Confirm the new EXE artifact was published. Compare
   `latest_artifact_id`, `.dev/current` build identity, and the build progress.
   If state is stale but the build worker succeeded, allow the running dev-loop
   to finish/publish; do not start a competing build.
3. Once the new EXE is published, run the focused C++ selftests against that
   exact executable, for example:
   - `--npc-navigation-selftest`
   - `--collision-selftest`
   - `--canonical-contact-selftest`
   - `--team-brain-selftest`
   - `--objective-selftest`
4. Start or reconnect the payload/funworld3 client through the dev loop. Do not
   stop the live server without confirming with the owner; the EXE build policy
   intentionally keeps a running EXE in place.
5. Capture a 60 s window and verify:
   - `npc.phase-window`: `collision_ms`, `animation_ms`, `aim_ms`,
     `pose_containment_ms`, `animation_skipped`, `aim_skipped`;
   - `npc.slow-update`: per-NPC sub-stages;
   - server `actual_hz`;
   - client `performance.fps`, p50/p95/p99.99.
6. Decide whether 30 Hz idle animation is acceptable:
   - if `animation_ms` and `aim_ms` are small, restore 60 Hz;
   - if idle pose staleness affects collision or containment, restore 60 Hz;
   - if target NPCs or combat look wrong, stop skipping aim first.
7. Validate NPC substep 1 for tunneling at maximum speed and during dashes.
8. Re-check the client target and server Hz. Build success is not acceptance.

## Acceptance evidence still required

- Connected payload funworld3 client window with the intended NPC count.
- Server 60 s window: 59.9–60.1 Hz for 99.99% of windows.
- Client 60 s window: at most 4.17 ms per frame for 99.99% of frames.
- NPC movement/collision/damage/firing remain 60 Hz.
- Local keyframed animations visible for the local player and remote avatars.
- No new pose, containment, or tunneling regression.
- Runtime journals under `logs/2026-11-17/20261117_170234/`.

## Relevant files

Code:

- `src/physics/physics-mini.h`
- `src/physics/physics-mini.cpp`
- `src/npc/npc.h`
- `src/npc/npc.cpp`
- `src/network/server-npcs.cpp`
- `src/engine/engine-tick-state.cpp`
- `src/network/server-packet-chat.cpp`
- `src/network/server-packets.cpp`
- `src/perf/perf.cpp`, `src/perf/perf-overlay.cpp`,
  `src/perf/perf-accounting.h` (earlier work)

Docs:

- `docs/specs/performance/performance.md`
- `docs/specs/performance/perf-report-overlay.md`
- `docs/features/performance/fps-improvement-ledger.md`
- `docs/changelog/2026-11-17/20261117_154410-perf-report-modes-and-frame-owner-fixes.md`
- `docs/handoffs/networking-hot-boundary-split.md`

State and evidence:

- `.dev/state.json`
- `build/progress.json`
- `build/source-manifest.json`
- `build/game/progress.json`
- `build/game/generation-328/game-g328.dll`
- `logs/2026-11-17/20261117_170234/server-01/events.jsonl`
- `logs/2026-11-17/20261117_170234/client-04/events.jsonl`

## Do not do

- Do not request or run a build.
- Do not use `agent-gate.py`; it stops the dev loop.
- Do not replace a live EXE after a build.
- Do not delete lane data or `build/source-manifest.json`.
- Do not claim runtime success from a successful compile or published DLL.
- Do not update the lint baseline to hide the existing aggregate findings.
