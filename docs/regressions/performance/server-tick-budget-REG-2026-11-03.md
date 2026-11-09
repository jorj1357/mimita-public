# Fixed-tick stalls from NPC containment and per-record journal flushes

Time created: 2026-11-03T19:09:00Z
Time last updated: 2026-11-05T20:10:26Z

Status: ATTEMPTED FIX (2) — fresh run confirms pre-tick build; other tick stalls and full 60 Hz review remain open

Related specification: `docs/specs/performance/performance.md`,
`docs/specs/debug-logging/debug-logging.md`
Related changelog: `docs/changelog/2026-11-05/20261105_201026-networking-tick-stall-investigation.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-11-03T19:09:55Z`

Journal: `logs/2026-11-03/20261103_191001/events.jsonl`

### Expected Behavior

The authoritative server runs at a fixed 60 Hz. A fixed tick may not spend
hundreds of milliseconds on NPC containment or logging.

### Actual Behavior

- 707 `performance.server-tick-stall` events over the run.
- Server tick windows recorded `npc_avg_ms` 112–200 ms and `total_max_ms`
  587 ms.
- The diagnostic safe-point search alone averaged 17.7 ms and reached 55.3 ms.
- Every `Important` record and every checked scenario record called `fflush`
  under the cross-process journal mutex.

### Confirmed Cause

Two compounding costs were present:

1. The NPC body-collider containment path described in
   `npc-template-body-collision-REG.md`.
2. `StructuredLogger::writeEvent()`/`writeEventChecked()` flushed the journal
   on every accepted record, so per-tick diagnostics serialized on disk and on
   the shared mutex.

### Corrected Code

- Removed the NPC template body containment path.
- Removed the per-candidate `npc.final-pose.safe-point.begin/end` diagnostics.
- `StructuredLogger` now writes rows without a per-row flush and flushes at most
  once per 100 ms through `flushEvents()`, called from the existing `tick()`.
- Scenario stop paths flush the journal explicitly.

### Proof

Source and build evidence only at this time. Runtime proof must show
`performance.server-tick-window` p99 ≤ 16.6 ms, zero
`performance.server-tick-stall`, and actual Hz ≈60 over a 60-second NPC run.

---

## Regression Occurrence 2

### Observed

Time: `2026-11-04T15:10:11.819Z`

Run: `logs/2026-11-04/20261104_151006/`

### Expected Behavior

Expensive map collision preparation completes before the first fixed server
tick; fixed-tick work stays within its 16.67 ms budget.

### Actual Behavior

The surviving server journal records 28 `performance.server-tick-stall`
events over 100 ms. At tick 0, total work took 1967.379 ms and NPC work took
1924.435 ms. At `15:10:11.708Z`, `server.npc-collision-world.lazy-built`
reports `build_ms=1856`, `npc_count=1`, and `triangle_count=5463` at
`src/network/server.cpp:1021`. This directly accounts for the dominant initial
stall. The same run also has later NPC and weapon-contact stalls, which are
separate from this map-build cost and remain unresolved here.

### Reproducibility

The surviving reproduction is run `logs/2026-11-04/20261104_151006/`, using
build `2026-11-04T15:08:17.468491Z` (manifest build ID begins `83af`). The
original Nov. 3 journal named by Occurrence 1 is missing. A fresh run of
Fun Counter Strike on `dust2cyberiav4` with NPCs exercises the same collision
world owner; with the fix, it reports the world summary before gameplay and
does not emit the lazy-build event. That fresh run does not reproduce the
remaining later NPC/contact overruns.

### Confirmed Cause

The first NPC simulation tick lazily built the NPC collision world from the
loaded map. The journal's build duration and tick duration establish the
first-tick cause. This does not establish the cause of later stalls or a
server process crash.

### Incorrect Code

Before this fix, `src/network/server.cpp` entered the fixed tick with
`npcWorldCollisionReady=false` and contained this lazy build at the NPC
simulation owner (old `src/network/server.cpp:1005`):

```cpp
if (!npcWorldCollisionReady && !npcs.empty())
{
    const auto npcWorldBuildStart = std::chrono::steady_clock::now();
    buildNpcWorldCollision(npcWorld, world);
    npcWorldCollisionReady = true;
}
```

The old block also measured and logged the build duration as
`server.npc-collision-world.lazy-built`.

### Corrected Code

`runServer()` now constructs the collision world after map load and before
`netStartup()`:

```cpp
World npcWorld;
buildNpcWorldCollision(npcWorld, world);
```

Exact current call: `src/network/server.cpp:543`. Both lazy-build branches were
removed.

### Proof

Fresh verification used dev-loop run `20261105_200615`, build
`2026-11-05T20:05:47.562559Z` (SHA-256
`9a89b2f24e8f01e6306d5847a4832cbf0b191f9156d21ff527d212003fdf0183`). The
server journal recorded `npc.collision-world.summary` at `20:06:17.542Z`,
before gameplay ticks, and zero `server.npc-collision-world.lazy-built` events.
Four `performance.server-tick-stall` events over 100 ms remained in the sampled
window. The first took 291.880 ms at tick 225 (133.346 ms contact/weapon and
121.292 ms NPC work); three further stalls at ticks 2856, 2857, and 2866 took
117.859, 126.455, and 330.257 ms. These later stalls remain unresolved. This
runtime sample does not prove a full 60 Hz minute or explain a server crash.
During the same run the server used 154.7 MB working set / 140.2 MB private
bytes, clients used 553.3/847.9 MB and 534.3/574.5 MB respectively, and Windows
reported 4531 MB free of 16025 MB. Those measurements do not support memory
pressure in this run and do not rule out pressure in the historical incident or
during a build.

## Occurrence 2026-11-08 — 19-NPC Payload/FunWorld3 navigator stall

Status: CONFIRMED RUNTIME STALL, cause narrowed to the NPC navigator update.

- Scene: Payload on FunWorld3, 19 NPCs, direct `mimita-check` server process.
- `npc.slow-update` for actor 1008: `navigator_ms=7,559.7`, `path_ms=12.1`,
  total update 7,560.9 ms.
- `npc.phase-window`: 62 updates took 7,587.9 ms.
- `npc.server-phase-window`: `ai_ms=7,590.2`, `target_selection_ms=0.096`.
- `performance.server-tick-stall`: `npc_ms=7,690`, total tick 7,700 ms.

This is a new occurrence of the unresolved later-stall group, not the earlier
map-build stall. The cause is inside `NpcNavigator`; the exact sub-stage is not
yet identified. The new NPC target/attack/dash policy is not the measured cost.

Ideal fix: instrument navigator sub-stages, cache/reuse the expensive operation,
bound per-tick work to the 60 Hz budget, and re-run the same 19-NPC scene.
