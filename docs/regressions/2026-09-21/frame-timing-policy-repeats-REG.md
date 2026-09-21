# Frame Timing Policy Repeats

Time created: 2026-09-21T01:23:53Z
Time last updated: 2026-09-21T01:23:53Z

Status: UNRESOLVED

Related specifications:

- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/features/live-code-development/live-code-development.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-21T01:23:53Z`

The project has repeatedly revisited frame waiting, VSync, chrono timing, CPU timing units,
and performance logging. The same investigation pattern keeps returning without one permanent
runtime contract proving which work consumed each frame.

### Expected behavior

The game should keep gameplay behavior intact while minimizing frame work. Fixed-tick gameplay
must remain separate from render pacing. The JSONL should identify the largest frame contributors
using explicit units and the active executable/generation.

### Actual behavior

The repository has timing collection and a 60-frame `performance.frame_window` summary, but the
running executable may predate that code. Detailed performance collection is also controlled by
configuration, so a session can appear to have no useful frame records even though timing code
exists in the source.

### Confirmed evidence

- `src/perf/perf.cpp:978-1024` writes a structured legacy frame breakdown when enabled.
- `src/perf/perf.cpp:1032-1077` writes `performance.frame_window` every 60 frames.
- `src/engine/engine-tick.cpp:306-317` measures pacing/sleep and total frame time.
- `src/engine/engine-tick.cpp:319-321` hot-reloads logger configuration and advances the logger.
- `config/debuglogger.json` now enables allocation, asset, collision, entity, effect, and render
  performance collection.

### Important safety rule

Do not enable synchronous disk flushing for every frame or every timing scope. That makes the
logger compete with the frame it is measuring and can create artificial stutter. Timing should
be collected in memory, slow frames should be emitted immediately, and bounded summaries should
be written periodically through the logger.

### Required permanent fix

1. Every performance event must include `frame`, `simulation_tick`, `total_ms`, and explicit
   `self_ms` / `inclusive_ms` fields.
2. Every performance event must include the active executable path, generation, and logger path.
3. The runtime must emit a slow-frame event immediately above a configured threshold.
4. The runtime must emit a sorted 60-frame contributor summary.
5. VSync and frame pacing must be measured as named scopes, never silently folded into gameplay.
6. The active `versioninfo` command must print the exact logger path used by that process.
7. A human test must confirm that the live JSONL changes without replacing the running EXE.

### Proof status

Source evidence and live configuration changes exist. Human confirmation that the currently
running EXE contains the current frame-summary code is still pending.
