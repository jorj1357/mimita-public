# issue-07977478c8fc: Map editor and creative actions cause frame freezes

## Human report (verbatim, immutable)

> can u add to issues that map editor reloading saving  copy pasting has like freeze  somietmes, copy pasting  paload routes freezes for like 0.5 sec, cant have freeze at all, for anything, relates to 240fps goal all the time regardless of waht is happening, especially in map ediotr/creation interfaces, if we want to get  creation to be as fun or more fun than playing, then need to treat it like comepeitive like 240fps ensured for all creative things


## Scheduled independent review

### 2026-11-25T18:08:09.618Z — cpu-resource-optimization

A bounded independent review was queued as wake wake-74484c05f2c7 after 724 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (evidence-gap)

### 2026-11-25T18:24:20.274Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Wake: wake-74484c05f2c7. Verdict: evidence-gap; no fix claimed, no code edited, no build requested.

**Issue brief / readiness (observed).** `issue readiness` = ready, score 8/8, state `new`, owner empty, 0 linked tasks, no proof task. Acceptance requires a recorded editor workload with per-frame timing for reload, save, and copy/paste (including Payload routes), max/p99.99, and journal evidence. Proof expectation requires "bounded owner-level scopes around reload/save/copy/paste."

**Proof status (observed).** `proof status` contains no record for issue-07977478c8fc. `evidence.runtime_verified=false`; `related_build` and `related_run` are empty.

**Linked artifacts (observed).** None: no build, run, journal, or task is linked. The report contains only the human quote and a ~0.5 s duration estimate for Payload-route copy/paste.

**Latest relevant journal (observed).** Run `20261125_175329` (launch mode 45 "fun counter strike", map `dust2cyberiav4`, artifact `2026-11-25T01:21:28.987918Z`, sha256 `5dda08799cefdf4caad640a34a806c92e4a03c3886acdd4aa943be8c558eeb8b`, client pid 13052), journal `logs/2026-11-25/20261125_175329/client-01/events-0003.jsonl` (63,511 lines). `map-editor.config.loaded` fires once at t=1.010 s (seq 56). The only `map-authored-geometry.applied` is the startup map load at t=2.312 s (`block_count=0`, `applied_count=0`, `rebuild_collision=true`, seq 88). No `map-editor` operation event follows. The journal has 564 `performance.frame-budget` overruns: max `over_ms` 1913.16 at t=2.914 s (State 1424.3 ms, EngineTick::Render 232.4 ms), p99 166.31 ms, median 3.82 ms, 6 frames >100 ms, 15 >50 ms. These are startup/config/scene frames, not editor-operation frames, so they neither confirm nor refute the reported reload/save/paste freeze.

**Retained-journal search (observed).** No `logs/**/*.jsonl` contains `map-editor.reload.result`, `map-editor.autosaved`, `world-project.version-saved`, `map-payload-route.validated`, or any `map-editor.command` event. The reported reload/save/copy-paste workload has never been exercised in a retained recorded run.

**Instrumentation gap in source (observed).** `MapEditor::copySelection` (src/mapeditor/map-editor.cpp:1747) and `MapEditor::pasteClipboard` (src/mapeditor/map-editor.cpp:1908) emit no StructuredLogger event. `pasteClipboard` runs `renumberPayloadRoute()` (:1985) then `rebuildGeometry()` (:1987) synchronously on the key-handler path (Ctrl+C/V at :3926-3927; console `copy`/`paste` handlers at src/mapeditor/map-editor-commands.cpp:860-877 emit only the generic `map-editor.command` at :317). `rebuildGeometry` (:4262-4271) calls `applyAuthoredMapGeometry` with `rebuildCollision=true` and `staticFaceRemoval=true` inline. `MapEditor::reload` (:4060) emits `map-editor.reload.result` (:4067-4075) with success/revision/path but no duration, then calls `rebuildGeometry()` (:4082). Explicit `MapEditor::save` (:4017) is untimed; only `autosave` records `duration_ms` (:3374). No `Perf::ScopedTimer` or editor scope wraps these paths.

**Inference / uncertainty.** Inference, not proof: a ~0.5 s Payload-route paste stall is consistent with a synchronous full authored-geometry + collision rebuild on the input/frame thread immediately after paste. Unknown: there is no measured operation duration, so the stall could instead be in notify/UI work, allocation, route renumbering, or unrelated background work. The stall must not be attributed without operation scopes.

**Contrary evidence / limits.** (1) The runtime budget is coded as 4.0 ms (src/perf/perf-frame.cpp:978-1003), stricter than the issue's 4.167 ms; `over_ms` against 4.167 must be recomputed, not read directly. (2) The latest run is a gameplay launch, not an editor workload; its 1913 ms startup stall is not evidence about editor actions. (3) No retained run exercised editor reload/save/paste, so the absence of reported freezes in logs is not evidence the defect is gone.

**Duplicate report.** `issue-ba939787ffa3` carries the identical immutable human quote and was created 16 s after this issue (2026-11-21T14:54:56Z vs 14:55:12Z). It holds the triage, scope extension, and an earlier trace hypothesis; this issue holds none. Do not close or merge here; flag for human/owner dedupe.

**Next discriminating check (single).** In a pinned dev-loop run with the editor open on a Payload map, drive the existing command surface (`mapeditor 1`, `select_kind route`/`select_route`, then repeated `mapeditor copy`/`paste`, `mapeditor reload`, `mapeditor save`) with route/object counts recorded, and inspect that exact client `events.jsonl` for `performance.frame-budget` sequencing. Before this measurement, add bounded `duration_ms` + count scopes to copy/paste, reload, and explicit save; without them the proof expectation cannot be met. Suggested evidence owner: unassigned (follow-up, not claimed here).
