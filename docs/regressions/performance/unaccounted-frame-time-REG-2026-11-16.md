# Unaccounted frame time was computed from the wrong frame

Date: 2026-11-16
Time last updated: 2026-11-16T20:45:00Z
Status: SOURCE CORRECTED — live accounting observed; 240 FPS target remains open
Owner: performance report / frame accounting

Related:

- `docs/specs/performance/performance.md`
- `docs/specs/performance/perf-report-overlay.md`
- `docs/gold/2026-11-16-honest-frame-accounting-and-perf-overlay.md`
- `docs/changelog/2026-11-16/20261116_192902-honest-frame-accounting-and-host-poll-watcher.md`

## Symptom

The performance overlay reported a large `UNACCOUNTED` value while the game
clearly had work to show. Observed example:

```text
FRAME: 62.39ms  BUDGET: 1.00ms
CPU: 1.49ms
UNACCOUNTED: 60.91ms
Audio: 0.06ms
PostFX: 0.34ms
```

The unaccounted number looked like a hidden 60 ms stall and made small timers
look important.

## Expected behavior

`UNACCOUNTED` means time in the completed frame that is not covered by named
owners. The report should split the full cycle into:

- host gap (outside the game frame),
- game roots (`EngineTick::*`),
- swap and sleep,
- a small residual.

The categories should approximately add to the total. Missing time must not be
hidden.

## Actual behavior

The overlay was rendered inside `engineTickUI`, before `Swap`, `Sleep`, and
`Perf::endFrame()`. It computed:

```text
unaccounted = gFramePacer.frameTimeMs() - s.current.total
```

That compared the **previous full cycle** to the **current partial frame**, and
it used the legacy `PerfTimes` set. Most top-level stages use
`MIMITA_PERF_SCOPE` and live in `gPerfScopes`, not `PerfTimes`:

- `Setup`
- `Rendering`
- `UI`
- `Swap`
- `Sleep`
- `Setup::HotReloadDLL`
- `Setup::ConfigPolling`

`Perf::addTime` also did not map `FramePacing::BeforeSwap` or
`FramePacing::PacerSleep`, so swap and sleep stayed at zero.

## Wrong code

```cpp
float ms = gFramePacer.frameTimeMs();
const double unaccounted = std::max(0.0, (double)ms - s.current.total);
```

## Corrected code

The next `Perf::beginFrame()` finalizes the completed frame before resetting
scopes:

```cpp
next.frameMs = s.lastFrameTimeMs;
next.cycleMs = gFramePacer.frameTimeMs();
next.hostGapMs = max(0.0, next.cycleMs - next.frameMs);
next.rootAccountedMs = sum(non-crossed EngineTick::* roots);
next.unaccountedMs = max(0.0, next.frameMs - next.rootAccountedMs);
```

The overlay displays the completed record and real top scopes. Swap and sleep
names are mapped in `addTime`/`addChildTime`.

## Evidence

After the correction, the same kind of scene showed:

```text
HOST GAP: 1.61ms (4.9%)  GAME: 31.12ms (95.0%)
SWAP: 0.65ms  SLEEP: 0.00ms  UNACCOUNTED: 0.04ms
```

The residual is now small, and the real owners are visible:
`ActorPairCollision`, `Simulation::Lifecycle`, `Ragdoll::WorldCollision`,
`Rendering::Actors::NetworkEntities`, and others.

## Remaining open issue

This record proves the accounting, not the frame target. `GAME` was still about
31 ms in the observed scene. The client 240 FPS / 4.17 ms target and the server
60 Hz target remain open and require optimization evidence.

## Rules

- Never compare a previous cycle to a partial current frame.
- Never use `PerfTimes.total` as the only frame account while
  `MIMITA_PERF_SCOPE` owns the top-level stages.
- Report the residual instead of hiding it in an "other" bucket.
- Keep the overlay's own cost visible with `Perf::Overlay`.
