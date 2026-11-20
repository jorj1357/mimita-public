# Juggernaut 60 Hz and all-NPC movement handoff

## Purpose

Audience: the next MiMITA agent continuing the Juggernaut server-tick investigation.

This handoff captures the requested acceptance target, current checkout/run
evidence, the generation-352 collision scratch change, known performance
findings, and the next runtime steps. It does not claim that 60 Hz or movement
for all 67 NPCs has been achieved. It does not authorize bypassing the root
dev-loop, reducing the authoritative tick rate, reducing the NPC roster, or
discarding concurrent checkout changes.

## User goal and acceptance

Keep the original goal intact:

> Launch Juggernaut through the dev-loop and keep it running for at least one
> minute with 67 NPCs moving, while 99.99% of tick-rate observations remain
> between 59.9 and 60.1 Hz. Use the live `events.jsonl`, change code based on
> that evidence, run the changed executable, and repeat until the target is met.

Also satisfy the repository performance specification in
`docs/specs/performance/performance.md`: authoritative simulation stays at
fixed 60 Hz; at least 99.99% of fixed ticks complete within 16.667 ms, and
there are zero `performance.server-tick-stall` events above 100 ms. Report
both the requested tick-rate band and the tick-duration distribution. Keep the
67-NPC workload and prove movement for every NPC; current sampled movement
events do not cover the full roster.

## Current checkout and run

- Repository: `C:\mimita-v9`
- Branch: `checkpoint/one-checkout-condensation-20261111`
- HEAD observed: `214849f07f9a4bc045d1461b1dbaf463a2084a85`
- Run: `20261119_014024`
- Mode/map: Juggernaut mode 9 on `dust2cyberiav4`
- Server journal: `logs/2026-11-19/20261119_014024/server-01/events.jsonl`
- Dev-loop journal: `logs/2026-11-19/20261119_014024/dev-loop/events.jsonl`
- The server journal repeatedly reported 67 NPCs.

The latest `python devscripts/dev-loop.py --status --json` observation returned
`status=starting`, `game_build.status=idle`, no queued game-DLL requests, and
the persisted server PID 29972 marked alive. Its state `updated_at` was
`2026-11-19T04:39:27.498Z`, while the machine clock was already
`2026-11-19T14:38Z`; treat that status as stale and verify ownership with the
allowed `--list-instances` and `--inspect` commands before acting. The status
also referred to the old EXE build ID
`2026-11-19T00:53:35.016823Z:eabcdcd2e22ec7eb301a8e858578833b98188c88dad26c9bd04e50106e34022d`.

Do not copy or publish the `control_token` field from `--status --json`.

## Build and runtime identity

The earlier `auto-24` request did finish; do not treat it as still building:

- `dev-loop/events.jsonl` records `dev-loop.game-generation.ready` at
  `2026-11-19T03:27:38.497Z`, generation 352, request `auto-24`, duration
  2020.218 seconds.
- `build/game/current.json` and `build/game/ready.json` report generation 352,
  status `ok`/`ready`, DLL `build/game/generation-352/game-g352.dll`, revision
  `9a16616133eda700233a3da75d4e31935c09cbd7e9cadd2dee994b707b89f712`.
- The current server journal contains `game.hotness.summary` at
  `2026-11-19T04:39:26.985Z` with `generation=352`, `npcs=67`, `server_path=dll`,
  `server_failures=0`, and `violations=0`. This confirms that generation 352
  was active in the server process at that time.
- Later `npc.phase-window.module_generation=345` values refer to the
  `npc-behavior` module generation, not the whole-game DLL generation. Do not
  use them to claim DLL generation 345 was active.
- The current dev-loop status and process ownership are stale/inconsistent;
  establish the live PID, EXE ID, and loaded DLL/module identity again before a
  new trial.

The scratch-reuse build note `2f30fbec0e70` and a separate dev-loop note
`deeb0baadd60` were still visible under `.dev/build-notes/pending` at the last
filesystem check. Preserve those notes; do not delete or claim either was
consumed without checking the matching build attempt and timing summary.

## Performance evidence

### Before generation 352

An earlier one-minute read of this same run, `2026-11-19T03:02:45Z` to
`03:03:45Z`, reported 285 fixed ticks (4.75 ticks/s), an average 66.48% of
window samples over 100 ms, maximum NPC average cost 519.63 ms, and maximum
per-window NPC cost 1012.157 ms. The phase-window maxima were 810.584 ms total,
385.697 ms physics, 317.241 ms collision, 228.451 ms navigation, and
246.947 ms decision. The 60-second output was captured during the earlier
investigation; the current server journal has since been rotated/truncated and
no longer contains those rows.

That interval had 89 rate-limited collision samples: median total 21.30 ms,
maximum 110.51 ms. Sweep/slide reached 110.50 ms and depenetration solve
reached 94.60 ms. A worst collision sample had only two broadphase candidates,
so candidate count alone does not explain its wall time. The interval also had
29 navigation-cost samples, a 116.24 ms maximum, and 24 failed-plan events.
Movement decisions sampled 8 distinct NPCs; 30/31 samples showed more than
0.1 m displacement. This is evidence that sampled NPCs moved, not all 67.

### After generation 352

The current journal shows a major timestamp discontinuity. It has a
33,546-second gap between `2026-11-19T05:08:13.561Z` and
`2026-11-19T14:27:19.862Z`. The later burst contains an
`npc.phase-window.total_ms` around 33.5 million ms and similarly huge server
tick costs. Treat these as suspension/clock-gap contaminated evidence, not as
a representative steady one-minute benchmark.

In the final wall-clock minute before the earlier gap,
`05:07:13.561Z`–`05:08:13.561Z`, only three `performance.server-loop-window`
rows appeared. Their `actual_hz` values were 0.0182, 3.1407, and 6.8776 Hz;
the first reported an 822,757 ms elapsed interval. All were below target, but
this is not a clean continuous 60-second acceptance sample because the log
contains large scheduling/time gaps. The same interval reported 67 NPCs,
several 100+ ms tick stalls, and 4 movement-decision samples for 4 distinct
NPCs, all with displacement over 0.1 m. It does not prove all-NPC movement.

In the later `14:27` burst, all 26 recorded collision substage samples had
`body_part_count=0`; max collision total was 280.78 ms and median 63.65 ms.
This burst follows the 9.3-hour gap and is not a clean benchmark either.

## Evidence-backed bottleneck chain

1. Server tick diagnostics show 67 NPCs and large `npc`/`npc_ai` costs while
   gameplay tick processing is synchronous in the server path.
2. `doCollisions()` calls `doGLBTriangleCollisions()` for the NPC collision
   step. The actor triangle solver is configured on, but
   `runActorTriangleCollisionStep()` returns false when both
   `physicalBody.parts` and `physicalBody.extraParts` are empty.
3. Live `npc.collision.substage-sample` rows repeatedly report
   `body_part_count=0`, after which `doGLBTriangleCollisions()` continues into
   the legacy capsule sweep/depenetration/floor-recovery path. This is a
   confirmed fallback path and a strong optimization target; it is not yet
   proven to explain every long tick.
4. Navigation has separate expensive paths. `npc.nav.plan-cost` attributes
   time to `global_graph_ms`, `local_plan_ms`, and `recast_ms`; the earlier
   interval had a 116.24 ms planning sample. Juggernaut behavior includes
   `juggernaut_swarm` (Recast) and `juggernaut_limited` (custom/default path).
   Verify the currently loaded profile/backend and active config before
   changing it. Prior Recast failures do not prove that switching the whole
   mode to another backend fixes movement or tick rate.
5. Individual collision and planning wall times sometimes greatly exceed the
   work counts. Do not assume this is all algorithmic CPU cost. The performance
   spec requires per-thread profiling before proposing a multi-core redesign.

Relevant owners to read before editing:

- `src/network/server-npcs.cpp`: NPC iteration and `npc.tick-cost` owner.
- `src/npc/npc.cpp`: `NpcSystem::updateOneWithTarget` and phase-window
  attribution.
- `src/npc/npc-navigator.cpp`: navigation plan decision and route costs.
- `src/physics/movement/physics-collision-dispatch.cpp`: shared collision
  dispatch.
- `src/physics/movement/physics-collision-glb-main.cpp`: actor-triangle
  attempt and legacy pipeline fallback.
- `src/physics/movement/actor-triangle-solver.cpp`:
  `runActorTriangleCollisionStep` geometry preconditions.
- `src/physics/movement/physics-scratch.h` and
  `physics-collision-glb-sweep-slide.cpp`: generation-352 allocation-reuse
  change.

## Changes already in the shared checkout

Do not overwrite or revert these concurrent edits. Inspect `git status --short`
and diffs before editing:

- `src/physics/movement/physics-scratch.h`
- `src/physics/movement/physics-collision-shared.h`
- `src/physics/movement/physics-collision-body.cpp`
- `src/physics/movement/physics-collision-glb-sweep-slide.cpp`
- `src/physics/movement/physics-collision-glb-main.cpp`
- `src/npc/npc-nav-graph.cpp`, `src/npc/npc-nav-graph.h`,
  `src/npc/npc-navigator.cpp`
- `devscripts/dev-loop.py`, `devscripts/dev_loop/game_build.py`,
  `devscripts/dev_loop/gui.py`, `devscripts/game_build_worker.py`, and
  `devscripts/source_manifest.py`
- `config/analytics.json`, `src/engine/engine-tick-camera.cpp`,
  `src/input/input-poll.cpp`, `devscripts/blender-ai` (submodule state), and
  multiple unrelated documentation/config edits shown by `git status`.

The collision scratch change reused thread-local vectors/buffers in the legacy
capsule pipeline. It intentionally did not alter movement equations, collision
rules, NPC count, or tick frequency. Generation 352 includes that change, but
there is no clean before/after minute proving its runtime effect.

## Required next steps

1. Re-read the current `AGENTS.md`, `docs/ROUTER.md`, and
   `docs/specs/dev-loop/agent-lanes-build-authority.md`. Use only the shared
   checkout and root dev-loop. Do not start a second loop, build worker,
   compiler, build request, or lane. Agents edit and observe; the dev-loop is
   the build authority. Do not poll or wait for a build.
2. Reconcile the stale dev-loop state with `--status`, `--list-instances`, and
   `--inspect`. Confirm whether the logged server process is currently alive,
   which EXE it runs, and whether generation 352 is still the active DLL.
   Preserve any live/user process and all pending notes.
3. Begin a fresh, uninterrupted one-minute run only through the existing
   dev-loop when its ownership/status permits. Keep Juggernaut mode 9,
   `dust2cyberiav4`, and all 67 NPCs. Record exact run ID, PID, EXE build ID,
   DLL generation, UTC start/end, and target-machine CPU hardware.
4. Use a continuous journal interval with no long event timestamp gaps. Count
   server-loop and tick samples explicitly. A stale snapshot or a
   multi-hour-suspended process is not acceptance evidence.
5. Establish per-actor movement evidence for all 67 stable NPC IDs over the
   minute. Existing `npc.movement-decision` rows are sampled and insufficient.
   If a diagnostic change is needed, keep it rate-limited and at the movement
   owner; do not add a synthetic self-test or unmanaged debug file.
6. Continue tracing the legacy collision fallback and expensive navigation
   through their current owners. Preserve the cached broadphase and avoid
   per-query allocation. Keep collision/damage/physics at fixed 60 Hz and do
   not solve the target by reducing the roster or silently freezing NPCs.
7. Before the first new DLL-source edit, queue its build note with
   `python devscripts/build-note.py --changes "..." --files ...`. Save source
   and let the root dev-loop detect it; never request/build directly. Do not
   use `build.py`, `game_build_worker.py`, compilers, `ccache`,
   `--request build`, `--request game-dll`, `--wait-for-build`, or a second
   dev-loop. If the daemon/build authority is absent or unusable, record the
   exact status and hand the build recovery decision to the human owner.
8. After each published candidate, verify its build result separately from
   runtime activation, then repeat the identical live workload and compare
   tick-rate distribution, per-tick duration, NPC phase costs, collision and
   nav costs, stalls, and all-actor movement.

## Routed references

- Desired performance and tick acceptance: `docs/specs/performance/performance.md`
- NPC behavior: `docs/specs/npc-behavior/npc-behavior.md`
- Shared NPC movement ownership: `docs/architecture/player-npc-systems/npc-movement.md`
- Movement/collision requirements: `docs/specs/movement/movement.md`,
  `docs/architecture/collision/collision.md`
- Live executable/journal workflow: `docs/workflows/runtime-scenario-validation.md`
- Structured diagnostics: `docs/specs/debug-logging/debug-logging.md`
- Dev-loop controls and runtime ownership: `docs/specs/dev-loop/dev-loop.md`
- Build authority: `docs/specs/dev-loop/agent-lanes-build-authority.md`
- Prior evidence: `docs/changelog/2026-11-19/20261119_015658-juggernaut-server-hz-investigation.md`,
  `docs/changelog/2026-11-19/20261119_022044-juggernaut-collision-hot-path-followup.md`,
  `docs/changelog/2026-11-19/20261119_025627-juggernaut-collision-scratch-reuse.md`
- Existing confirmed overload record: `docs/regressions/performance/server-tick-budget-REG-2026-11-03.md`

