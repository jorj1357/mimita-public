# Tick Rate Rule (Hard Rule)

All gameplay collision, damage, and physics MUST run at the fixed 60Hz tick rate, never per-frame. The game uses `constexpr double kClientFixedDt = 1.0 / 60.0` in `engine-tick-combat.cpp`. Running collision per-frame means:
- Low-FPS devices get fewer collision checks (unfair disadvantage)
- High-FPS devices get more collision checks (unfair advantage)
- Gameplay behavior varies by hardware

Weapon collision, NPC overlap detection, damage application, and knockback MUST use the tick accumulator pattern:
```cpp
const float tickDt = 1.0f / 60.0f;
const uint32_t ticksThisFrame = std::max(1u, (uint32_t)std::round(dt / tickDt));
for (uint32_t t = 0; t < ticksThisFrame; t++) {
    // collision + damage logic here
}
```

Never run collision logic in the raw render loop without tick quantization.

# VSync Policy (Hard Rule — Never Override)

VSync is **forced OFF globally** and must never be re-enabled. This is a permanent performance constraint.

## Rules

1. **Never call `glfwSwapInterval(1)` anywhere in the codebase.** Every call must be `glfwSwapInterval(0)`.
2. **Never write `mVSync = true`** in any file (Renderer, VideoSettings, FramePacer).
3. **Never add a UI toggle, slider, or terminal command that enables VSync.**
4. **Never read `"vsync": true` from config and apply it.** Config loads must override any stored vsync value to `false`.
5. **Never add command-line flags that control VSync.**
6. **Never skip the software frame pacer** (`FramePacer::endFrame()`) by claiming "VSync handles pacing."
7. **Fullscreen/window transitions must always call `glfwSwapInterval(0)`** after `glfwSetWindowMonitor`.
8. **Any code path that calls `Renderer::setVSync(true)` must be blocked** with a `[VSYNC] BLOCKED` log.

## Why

VSync adds 16-29ms of driver-level stall per frame (measured at the Swap scope). The engine uses a software frame pacer (`FramePacer::endFrame()`) with `maxFrames` to control frame rate. VSync is incompatible with the 240 FPS working target (about 4.167 ms per frame) and with consistent frame pacing.

## Enforcement Points

- `Renderer::setVSync()` — always calls `glfwSwapInterval(0)`, logs `[VSYNC] BLOCKED` if `on==true`
- `Renderer::applyVideoMode()` — always calls `glfwSwapInterval(0)` after monitor changes
- `Renderer::forceVSyncOff()` — explicit force-off with reason logging
- `VideoSettings::setVSync()` — forces `mVSync = false`, logs blocked attempt
- `VideoSettings::load()` — overrides JSON `"vsync": true` to `false`
- `VideoSettings::save()` — always writes `"vsync": false`
- `FramePacer::setVSync()` — forces `mVSync = false`, logs blocked attempt
- Settings menu — shows "VSync: OFF (forced)" with no toggle
- Terminal `vsync` command — logs blocked, refuses to enable
- Boot sequence — loud `[VSYNC] FORCED OFF` log confirming state

## If You Think You Need VSync

You don't. Use `maxFrames` in the terminal or settings to cap FPS. If the frame rate is unstable, fix the frame pacer or reduce render cost — do not add VSync.

# Performance Rules

## Device range and frame-time goals

The engine should remain usable on low-power computers, including systems
where the player cannot replace or upgrade hardware. Performance decisions
must be based on measured frame-time and simulation costs on representative
devices, not total CPU utilization alone.

The fixed 60 Hz gameplay simulation remains authoritative regardless of render
rate. The working performance target is 240 FPS, about 4.167 ms per rendered
frame. 999 FPS / about 1.00 ms is an aspirational headroom goal, not a gate, a
support promise, or a reason to change gameplay tick rate. Record the target
hardware, workload, frame count, and whether the measurement is CPU, GPU, or
frame-pacing limited.

### Performance acceptance goals

Client frame acceptance is per-frame:

- At least 99.99% of rendered frames must complete within the 240 FPS budget,
  at or below 4.167 ms.
- Report `p50`, `p95`, `p99`, `p99.99`, max, and the exact count and percentage
  of frames over budget.
- Do not use an average FPS or a one-second-window average as the acceptance
  gate. One-second `performance.fps` records remain useful diagnostics and must
  not hide a frame spike.
- The aspirational 1 ms budget is reported separately. It must not replace the
  240 FPS gate or make ordinary frames look like failures before the project
  reaches that headroom.
- Do not lower visual quality to satisfy the target. If the measured hardware
  ceiling is below 240 FPS, report the ceiling and the largest measured scopes.

Server tick acceptance is separate:

- The authoritative simulation remains fixed at 60 Hz.
- At least 99.99% of fixed ticks must complete at or below 16.667 ms under the
  stated workload. Over 10 minutes that permits only a few late ticks.
- Zero hard snapshot timeouts and zero `performance.server-tick-stall` entries
  above 100 ms.
- Server correctness wins over client frame rate whenever they conflict.

The one-second-window wording in older handoffs and archived hot-reload notes
is superseded by the per-frame client gate above. Runtime evidence still records
the workload, duration, hardware, and build/generation identity.

Before a multi-core redesign, profile per-thread CPU use and the critical path.
Low total CPU use does not prove that only one core is used: one saturated
thread can be a small fraction of total utilization on a many-core device, and
GPU or frame-pacing limits can also constrain frame rate. Parallelize only
work with independent ownership and deterministic inputs; preserve fixed-tick
collision, damage, and physics ordering.

Prefer:

* predictable execution
* cache-friendly structures
* simple data flow
* low allocation counts
* low memory usage

Measure before optimizing.

But when two implementations are equal:

prefer the simpler and faster one.

Avoid architecture that exists only for abstraction.

Abstractions should earn their cost.


## Grenade hazards and NPC rush budget (2026-11-01)

NPC smoke/fire/darkbang/frag hazard awareness uses one reusable per-tick hazard
snapshot. Do not rescan all collision triangles or all projectiles per NPC per
query. The snapshot is built once per fixed tick, reused by all NPC perception
and utility calls, and must avoid hot-loop allocations.

Smoke line-of-sight blocking reuses the existing LOS ray and a bounded volume
segment test. It must not become a second full collision query.

Fun Counter Strike rush reuses the existing navigation route and shared
movement/dash/jump inputs. It must not add a parallel mover, per-tick heap
allocation, or an extra physics pass.

## Pending performance validation — 2026-11-08 lane pass

Status: `CODE ONLY` / `NEEDS RUNTIME`. These are known blockers, not accepted
results.

### Server 60 Hz with 19 NPCs

- Observed: one fixed tick took 7,700 ms; `npc.phase-window` recorded 7,587.9 ms
  for 62 NPC updates.
- First divergence: the NPC navigator. `npc.slow-update` for actor 1008 shows
  `navigator_ms=7,559.7`, while `path_ms=12.1` and
  `target_selection_ms=0.096`.
- Cause: one navigator update is doing roughly 7.5 seconds of work per NPC. The
  new NPC target/attack/dash policy is not the measured cost.
- Ideal fix: add sub-stage timing inside `NpcNavigator`, identify the 7.5 s
  operation, cache/reuse it instead of rebuilding per NPC, and bound the
  per-tick navigator work to the 60 Hz budget.
- Required evidence: 19 NPCs, FunWorld3, Payload, sustained 60 Hz window;
  `npc.slow-update`, `npc.phase-window`, and
  `performance.server-tick-stall` in the lane journal.

### Client 240 FPS

- Status: `NEEDS RUNTIME`. It cannot be measured honestly while the server is
  stalled at 0.2 Hz.
- Ideal fix: after the server holds 60 Hz, measure client frame slices and fix
  the largest frame scopes first. Target consistent frame times, not a peak
  number.

### Connection and hit-authority impact

The 0.2 Hz server also blocks client join and hit-claim validation. See the
networking and contact-authority specifications for the separate acceptance
items.


## Measured client frame budget — 2026-11-15

Current client runs still miss the 240 FPS / approximately 4.17 ms frame target.
Two steady 60-frame windows in run `20261115_005633` showed:

- 100% of frames over 4 ms on both measured clients;
- average frame times of about 10–50 ms;
- p95 of about 15–128 ms;
- max of about 64–242 ms.

The largest measured scopes were `Simulation::Lifecycle`,
`ActorPairCollision`, `Setup::HotReloadDLL`, `Combat::PostWeaponContainment`,
shadow rendering, UI, and swap. `Rendering::Actors::Cosmetics` was about 0.1 ms
CPU in those windows, so cosmetics are not the measured primary CPU cost.

Required next evidence: a steady 60-second sample after the hot client frame
stops stalling, with GPU timing when available, plus an A/B test with cosmetics
disabled. Do not treat a cosmetic-only change as proof that the FPS issue is
fixed. The acceptance calculation is per-frame; the sample duration is not a
window-average gate.


## Always-on 1 Hz FPS journal event (2026-11-15)

`PERFORMANCE_FRAME` remains the rich 1-second frame record. Add a compact
`performance.fps` event at the same boundary containing at least: `fps`,
`avg_frame_ms`, `p50_frame_ms`, `p95_frame_ms`, `p99_frame_ms`,
`max_frame_ms`, `frame_budget_ms`, `frames_over_budget`, `npcs_active`, and
`effects_alive`.

Tradeoff: per-frame FPS logging would add I/O and distortion. The event is
aggregated over one second and rate-limited to one record per second; it is not
a substitute for a frame profiler. It is always emitted at `Important` under
the `performance` category so a live reader can see FPS change over time.

## NPC decision cadence and server tick budget (2026-11-17)

Server NPC movement, collision, damage, and authoritative physics remain on the
fixed 60 Hz tick. NPC sensing, target selection, navigation replanning, and
state decisions may be staggered across ticks so each NPC thinks every third
tick while every NPC still advances physics every tick. This spreads decision
work without lowering the authoritative simulation rate.

Staggering is a server budget tool, not a license to run collision, damage, or
physics below 60 Hz. Evidence must show `actual_hz` near 60 and separate NPC
phase costs (`sense_ms`, `npc_ms`, `navigator_ms`, `physics_ms`) so a stalled
tick is attributed to its owner.

Server NPC physics reports its own sub-stages: `physics_config_ms`,
`move_pre_ms`, `collision_ms`, `move_post_ms`, `facing_ms`, `animation_ms`,
`aim_ms`, and `pose_containment_ms`. When a server tick exceeds 10 ms of NPC
presentation work, optional animation and aim for remaining idle NPCs may be
skipped; movement, collision, firing, and target-facing NPC animation remain
on the 60 Hz path. Idle server NPCs may animate at 30 Hz, while NPCs with a
target keep 60 Hz animation.

## FPS improvement ledger (2026-11-17)

Confirmed runtime improvements are tracked in
[`docs/features/performance/fps-improvement-ledger.md`](../../features/performance/fps-improvement-ledger.md).
A change is added only with before/after runtime evidence and a named owner.
Partial improvements and profiler-only cost reductions are labeled separately.

## Performance report overlay modes (2026-11-16)

The in-game performance report is specified in
[perf-report-overlay.md](perf-report-overlay.md). Mode 2 (compact, sampled at
1000 ms) is the default for sustained measurement. Mode 1 is live/full and is
diagnostic only. Mode 3 is full detail sampled at 1000 ms and redrawn from
cached lines. The report must expose its own `OVERLAY` cost and must emit a
1 Hz `performance.report` journal event while enabled.

## Payload live performance investigation start (2026-11-18)

The current user report is Payload on Fun World 3: client frame rate needs to
stay high while the authoritative server sustains 60 Hz with the active NPC
workload. Treat 240 FPS as the measured client target, not a promise that every
machine can reach it. Begin with a clean, identity-matched live run and record
the executable/build identity, map and mode, client/server instance IDs,
hardware, NPC count, sample duration, and journal paths.

Investigation order:

1. Establish a stable Payload baseline with the user's normal combat workload.
   Capture per-frame client timing and the compact `performance.fps` event,
   plus server `actual_hz`, fixed-tick durations, `performance.server-tick-stall`,
   and NPC phase/sub-stage timings. Use the compact performance report mode
   during sustained measurement; enable full detail only to inspect a named
   outlier. The client target is the existing 99.99% at-or-under-4.167-ms
   frame gate; the server target is 99.99% of ticks at-or-under 16.667 ms.
2. Identify the first over-budget client frame scope or server tick stage from
   that same run. Do not infer the cause from average FPS, total CPU use, an
   idle client, or one subsystem's cost alone. Attribute work before changing
   it, then compare an equivalent workload and retain the journal evidence.
3. Once server ticks sustain 60 Hz, remeasure client performance in connected
   Payload combat. Keep simulation correctness, collision, damage, and physics
   on the fixed 60 Hz path while reducing measured costs. A client-only idle
   result does not establish the target under gameplay.

This baseline is the first step for OGI-035; no new performance result or cause
is claimed by this plan. The related combat reports (NPC effectiveness and
stuck geometry, apparent low direct-hit damage, and floating character impact
marks) have separate behavior owners and should be diagnosed alongside the
performance sample without conflating their pass/fail evidence.
