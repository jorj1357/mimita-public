# Juggernaut collision scratch reuse

# Task

- Task ID: session-20261119-juggernaut-scratch-reuse
- Session ID: session-20261119-juggernaut-scratch-reuse
- Owner: Codex
- Focus: Reduce NPC capsule-collision hot-path allocation cost while preserving fixed 60 Hz physics
- Summary: Reused thread-local collision buffers in the legacy capsule sweep/depenetration path, guided by live Juggernaut collision-stage timings. Runtime impact is not yet verified.
- Status: in-progress
- Date, time, timezone: 2026-11-19T02:56:27+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85

# Evidence before change

- The live run was `20261119_014024`, Juggernaut mode 9 on `dust2cyberiav4`, server PID 29972 with 67 NPCs. At 02:42 UTC, its latest one-minute `performance.server-tick-window` sample recorded 465 simulation ticks (7.75 Hz), zero windows at exactly 60 ticks, 192 ticks over 100 ms, and a maximum tick of 1,258.48 ms.
- The same interval's `npc.phase-window` records reached 861.5 ms total, including 478.0 ms physics, 344.0 ms collision, and 218.0 ms navigation. The newly loaded collision diagnostic recorded 72 slow capsule steps: median 15.64 ms, maximum 141.33 ms. Sweep/slide was the dominant measured stage in 39 samples and depenetration solve in 23.
- In that interval, Recast comparison failed 88 times with no successes, there were 67 failed NPC plans, and the periodic trace sampled 8 of 67 NPCs, 7 marked stuck. This does not establish movement for the full roster.
- Generation 351 published at 02:36:21 UTC. New `npc.collision.substage-sample` rows began at 02:36:31 in the same server process, confirming that the diagnostic code was active without a server restart.

# Specification alignment

- Relevant requirements: preserve fixed 60 Hz authoritative gameplay; reuse collision owners and the cached broadphase; avoid per-query allocations in hot loops; use the root dev-loop and its live journal for behavior evidence.
- Why this change follows them: the legacy capsule sweep used value-returning broadphase calls and created temporary vectors in each collision substep. This patch routes those buffers through the existing thread-local `PhysicsScratch` storage and keeps the same sweep, contact, and correction calculations.
- Scope decision: no tick rate, collision distance, NPC count, or movement behavior was reduced.

# Exact implementation changes

## Files

- `src/physics/movement/physics-scratch.h`: add reusable candidate-index, body-sample, delta, and sweep-hit buffers.
- `src/physics/movement/physics-collision-shared.h` and `src/physics/movement/physics-collision-body.cpp`: add an output-buffer form of `collectPlayerBodyCollisionSamples`; retain the existing value-returning API for other callers.
- `src/physics/movement/physics-collision-glb-sweep-slide.cpp`: reuse the thread-local candidate, sample, delta, pre-move, body-union, slide, and TOI buffers. Use the output-buffer `gatherGLBTriangles` overload to preserve capacity across queries.
- `src/physics/movement/physics-collision-glb-main.cpp`: reuse candidate buffers for depenetration and emergency checks; pass the emergency candidate list directly rather than copying it.

# Build and validation

- Queued build note ID `2f30fbec0e70` for these five compiler inputs.
- The root dev-loop emitted `hot.build.plan` generation 24 at 02:53:45 UTC with `game_auto: true` and these five paths. A subsequent `--status` reported `game_building`; `build/game/progress.json` still described completed generation 351. No terminal result for this source change was observed in this session.
- `git diff --check` passed for the five edited files. No tests or manual build were run. The change is not yet compiled or runtime-verified; build identity and NPC movement/Hz acceptance remain open.
