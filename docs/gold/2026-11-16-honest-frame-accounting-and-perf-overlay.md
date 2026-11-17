// 2026-11-16T20:45:00Z
/* purpose
* preserve the method that replaced a false UNACCOUNTED number with complete
* frame accounting, complete with server Hz and bounded overlay modes
* show how a visible performance counter can be made trustworthy before using it
* connect source changes, build evidence, runtime evidence, and remaining work
* this file DOES NOT claim 240 FPS or human visual acceptance
*/

# Gold behavior: honest frame accounting before optimization

- Timestamp: `2026-11-16T20:45:00Z`
- Scope: perf overlay, completed-frame accounting, server Hz, overlay modes
- Human result: `perf_report 1` became visibly useful; the false unaccounted time
  dropped from about 60.91 ms to 0.04 ms in the reported example
- Regression: `docs/regressions/performance/unaccounted-frame-time-REG-2026-11-16.md`
- Changelog: `docs/changelog/2026-11-16/20261116_192902-honest-frame-accounting-and-host-poll-watcher.md`

## The symptom

The overlay showed:

```text
FRAME: 62.39ms  BUDGET: 1.00ms
CPU: 1.49ms
UNACCOUNTED: 60.91ms
```

Audio and PostFX looked significant only because nearly all real time was
missing. The number was not a game stall measurement.

## The first mistake

The overlay was drawn during `engineTickUI`, before `Swap`, `Sleep`, and
`Perf::endFrame()`. It compared:

- `gFramePacer.frameTimeMs()` — the **previous full cycle**, with
- `s.current.total` — the **current, partial frame** legacy timers.

It also read the legacy `PerfTimes` set, which did not contain most
`MIMITA_PERF_SCOPE` stages such as `Setup`, `Rendering`, `UI`, `Swap`, and
`Sleep`. `FramePacing::BeforeSwap` and `FramePacing::PacerSleep` were not mapped
to `swapTime`/`sleepTime`.

## The correction

At the next `Perf::beginFrame()`, before `perfResetScopes()`:

1. Read the completed frame's `gPerfScopes`.
2. Sum non-crossed `EngineTick::*` roots as `GAME`.
3. Read the completed full cycle from `gFramePacer`.
4. Compute `HOST GAP = cycleMs - frameMs`.
5. Compute `UNACCOUNTED = max(0, frameMs - GAME)`.
6. Copy the top scopes by self time.
7. Copy the completed `PerfTimes` for the legacy timer list.
8. Publish the record for the next frame's overlay.

The overlay now shows last-completed-frame data:

```text
HOST GAP: 1.61ms (4.9%)  GAME: 31.12ms (95.0%)
SWAP: 0.65ms  SLEEP: 0.00ms  UNACCOUNTED: 0.04ms
```

The sum is coherent: host gap + game time + residual = cycle. The remaining
work is now visible as game scopes such as `ActorPairCollision`,
`Simulation::Lifecycle`, `Ragdoll::WorldCollision`, and
`Rendering::Actors::NetworkEntities`.

## Server Hz

The client cannot know a remote server's outer-loop rate. It can measure the
authoritative tick rate it receives:

```text
SERVER RX Hz = delta(latestServerTick) / wall_seconds
```

The report labels this as RX. Actual server outer-loop Hz is shown only when a
local/listen server genuinely exposes it; otherwise it is `n/a`. A stale server
is marked stale instead of silently showing an old rate.

## Overlay cost and modes

A profiler that redraws dozens of lines every frame becomes part of the problem.
The report therefore has modes:

- mode 1: live/full, every frame, diagnostic only;
- mode 2: compact, 1000 ms wall-time sample, top 5 scopes and top 5 timers,
  **default**;
- mode 3: full detail, 1000 ms wall-time sample, cached lines redrawn every
  frame.

The overlay measures itself with `Perf::Overlay` and shows `OVERLAY` in ms. Mode
2's target is p95 <= 0.20 ms.

## JSONL loop

The existing 1 Hz records stay:

- `performance.fps` — always-on compact FPS;
- `PERFORMANCE_FRAME` — richer 1 Hz frame summary.

The report adds `performance.report` at 1 Hz while enabled, with mode,
host/game/swap/sleep/unaccounted, overlay ms, server RX/actual/tick/stale,
generation, and top scopes. `performance.frame-budget` is limited to 1/s so
over-budget frames do not create per-frame log traffic.

This produces the desired live loop:

```text
edit .cpp
-> dev-loop builds a new game DLL generation
-> same PID activates it
-> performance.report shows the new FPS and top scopes
```

## What is proven

- The old `UNACCOUNTED` was a measurement-window bug, not 60 ms of invisible
  game work.
- The completed-frame accounting produces a small residual and names the real
  frame owners.
- The client-side server RX Hz calculation has a clear, honest meaning.

## What is not proven

- 240 FPS at 4.17 ms or less in 99.99% of frames.
- 60 Hz server at 59.9–60.1 Hz in 99.99% of windows.
- Mode 2 overlay p95 <= 0.20 ms.
- Human visual/feel acceptance.

Those require steady runtime windows and later optimization work.
