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

VSync adds 16-29ms of driver-level stall per frame (measured at the Swap scope). The engine uses a software frame pacer (`FramePacer::endFrame()`) with `maxFrames` to control frame rate. VSync is incompatible with the performance target of <1ms frame times.

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
rate. A 240 FPS target corresponds to about 4.17 ms per rendered frame; 999 FPS
is an aspirational headroom goal of about 1.00 ms per frame, not a universal
support promise or a reason to change gameplay tick rate. Record the target
hardware and whether the measurement is CPU, GPU, or frame-pacing limited.

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
