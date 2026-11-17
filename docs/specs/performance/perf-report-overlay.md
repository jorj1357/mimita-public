# Performance report overlay

## Purpose

Define how the in-game performance report captures, displays, and journals
frame data. The report is a diagnostic tool. It must not become the reason a
frame misses its budget, and its numbers must name the owner of measured time.

This document is a desired-behavior specification. Current generated build
state belongs in `build/hot-reload/status.json` and the live `events.jsonl`.

## Ownership and hot boundary

- `src/perf/perf.cpp`, `perf-overlay.cpp`, `perf-commands.cpp`, and
  `perf-frame.cpp` are game-owned and hot-reloadable.
- The report may read resident mechanisms, but it must not require a cold host
  change for ordinary display, sampling, or journal fields.
- The report never writes the EXE and never controls gameplay.

## Modes

`perf_report [0|1|2|3]` selects the mode. `perf [0|1|2|3]` sets the report mode.
With no argument, both toggle between off and the last nonzero mode. The
default when enabling is **mode 2**.

| Mode | Data refresh | Display | Intended use |
|---|---|---|---|
| `0` | none | off | normal play |
| `1` | every frame | full detail every frame | short interactive inspection |
| `2` | 1000 ms wall time | compact: summary + top 5 scopes + top 5 timers | **default sustained measurement** |
| `3` | 1000 ms wall time | full detail from cached lines, redrawn every frame | full detail without per-frame formatting |

Rules:

- Sampling is wall-time based. It must not scale with render FPS or server tick
  count.
- The sample interval is `perf_sample_ms [100..10000]`, default `1000`.
- Mode 1 is diagnostic only. Do not use mode 1 to claim that the game itself is
  slow; the profiler is part of the frame.
- Mode 2 is the measurement mode for performance work and the default for
  sustained play.
- Mode 3 caches its formatted lines at the sample interval and redraws them
  every frame so the panel stays visible. Drawing cost remains visible in
  `OVERLAY`.

## Required fields

The report shows, at minimum:

- FPS, average FPS, frame ms, budget ms, and worst frame since the report
  opened.
- `HOST GAP`: full cycle time minus the game frame; includes host loop,
  post-frame logging, command queue, generation watcher consumption, and
  deferred host work.
- `GAME`: inclusive sum of non-crossed `EngineTick::*` roots.
- `SWAP`, `SLEEP`, and true `UNACCOUNTED` residual.
- `OVERLAY`: the report's own measured inclusive time.
- `SERVER RX Hz`: observed authoritative tick rate from
  `latestServerTick` deltas over a wall-time window.
- `SERVER ACTUAL Hz`: actual server outer-loop rate only when a local/listen
  server genuinely exposes it; otherwise `n/a`.
- `SERVER TICK`: latest received authoritative tick.
- `GEN`: best-effort published/ready game generation.
- Top scopes from the completed frame, sorted by self time.
- Nonzero legacy timers, sorted by time.

Server state rules:

- Not connected: `SERVER: off`.
- Connected but no recent tick: `SERVER RX: 0.0 Hz (stale)`.
- Never fabricate actual server Hz. The client cannot know a remote server's
  outer-loop rate; RX Hz is the contract.
- A local/listen server may show actual loop Hz only from a real local
  measurement.

## Journal contract

The report must be readable live from `events.jsonl` without per-frame writes.

- `performance.fps` remains an always-on compact 1 Hz event.
- While any report mode is enabled, emit one `performance.report` event per
  second with: mode, cycle/frame/host/root/swap/sleep/unaccounted, overlay ms,
  server RX/actual/tick/stale fields, published generation, and the top scopes.
- `PERFORMANCE_FRAME` remains the richer 1 Hz frame summary.
- `performance.frame-budget` is rate-limited to at most one record per second,
  even when every frame is over budget.
- No performance record may be emitted once per frame.

This is the AI feedback loop:

```text
edit source -> root dev-loop builds a new game DLL generation
-> same PID activates it -> performance.report shows new FPS/top scopes
```

## Budgets and targets

- Client target: 240 FPS minimum, 4.17 ms per frame or less, 99.99% of frames.
- Server target: 60 Hz, 59.9–60.1 Hz, 99.99% of measured windows.
- Mode 2 overlay p95 target: `<= 0.20 ms` CPU.
- The overlay must report its own cost. If mode 2 misses its overlay target,
  reduce the compact line count before changing game systems.
- A profiler screenshot is not proof that a gameplay performance target is met.

## Evidence

A performance claim requires:

- executable/build identity, process ID, run ID, and journal path;
- active game generation and report mode;
- a steady window with frame count and duration;
- FPS/percentiles and over-budget counts;
- host/game/swap/sleep/unaccounted split;
- top scopes and overlay cost;
- server RX Hz and, when available, actual local server Hz;
- separate build, runtime, and human-review claims.

## Non-goals

- Per-frame logging or per-frame journal flushes.
- Hiding unaccounted time inside an "other" bucket.
- Treating a synthetic self-test as user-visible performance evidence.
- Using the overlay as gameplay acceptance.
