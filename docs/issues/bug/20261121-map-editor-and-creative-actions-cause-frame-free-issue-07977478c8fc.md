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

## User quote

### 2026-11-27T23:17:57.387Z — map-editor-freeze-payload-crash

Via map-editor-freeze-payload-crash; supports unspecified:

> map editor needs good events.jsonl  ai watching for what making freezing happen like what kebaord inputs  what buttosnimpressing bc it  has a freeze randoml, also launchgin paload map  thru client  on mimta pipes map  in paload it crashes cant have crashing at all unless it explaind super loud claer  why crash

## Agent suggestion

### 2026-11-27T23:25:55.770Z — map-editor-freeze-payload-crash

Live evidence from run 20261127_230430, client-02 journal logs/2026-11-27/20261127_230430/client-02/events.jsonl: the mimita-pipes client observed network.mode.active for Payload at 2026-11-27T23:14:20.071Z, so this run did not reproduce a process crash. The same journal contains 11,290 game.frame.render-failed records among 14,966 total; each reports result=7 (HostResult::InternalError) from src/host/host-runtime.cpp, and the source catches all render exceptions without preserving their type/message. Failures span 23:13:52.546Z–23:14:38.979Z. Source currently has no map-editor key or mouse-button transition events; reload/save/copy/paste/rebuild lack full-operation timing. Inference only: this can look like a frozen/broken client and hides an exception cause, but it is not proof that the reported Payload crash occurred. Proposed instrumentation: key press codes/names/modifiers (never typed text), mouse press/release with cursor and selection state, editor operation/rebuild duration plus counts, render failure exception/status details with a five-second recurring summary and recovery event, and a process.crash journal record linking the crash report and minidump. Measure in a fresh pinned editor/Payload runtime before attributing the user's freeze or claiming a crash fix.

## Assignment

### 2026-11-27T23:27:01.157Z — map-editor-freeze-payload-crash

Owner: map-editor-freeze-payload-crash

## Agent suggestion

### 2026-11-28T03:02:02.540Z — map-editor-freeze-payload-crash

Build attempt game-dll-20261127T232810391707Z-7e8324 failed during compile only: handleTextKey used .begin/.end on four UITextInputState[3] arrays. Compile 18.732s, 1 TU failed, 645 skipped; no DLL publish, active generation remains 805. Correcting the array scan before a new build request. Postmortem resource measurements: peak host CPU 92.48%, minimum available memory 3826.6 MB. Shared loop had existing server/client run 20261127_231557 in Bloodtest on slaughterhouse1 during build. This is build-host evidence, not evidence of the Payload freeze cause.

## Review (evidence-gap)

### 2026-11-28T03:02:43.558Z — daily-review-20261128

Reviewer: daily-review-20261128

2026-11-28 daily review: DIRECT build evidence: dev-loop status at 2026-11-28T03:00Z reported game-build failed, generation 805, detail compile failed: 1 file(s) in 18.732s; attempt game-dll-20261127T232810391707Z-7e8324 (started 2026-11-27T23:28:10Z, finished 23:28:37Z, 1 compiled/645 skipped) failed compiling src/mapeditor/map-editor.cpp. Compiler lines 3029-3038 call .begin()/.end() on mPropertyPosition, mPropertySize, mPropertyRotation, and mPropertyScale, each declared UITextInputState[3]; diagnostic says these are non-class array types. This identifies a compile failure in the current shared editor input and prevents producing a newer DLL from this attempt. It is not evidence about the user's freeze: no editor operation timings or live editor behavior were captured here. Existing run 20261127_231557 and prior gen-805 artifact remain distinct from the failed candidate. Inference: active editor changes or integration caused the failure; owner confirmation pending. Do not conflate this build blocker with runtime frame-time acceptance.

## Agent suggestion

### 2026-11-28T03:03:44.239Z — map-editor-freeze-payload-crash

Second approved attempt game-dll-20261128T030246290451Z-e9f4a7 failed during changed-source preflight before compile/link/publish: map-editor.cpp updateInput redeclared const bool right/left, conflicting with existing GLFW int states at lines 3672/3671. Resource postmortem: duration 28.219s, peak CPU 87.26%, minimum available memory 1034.9 MB; runtime alignment unavailable, so exact live workload/mode is unknown. This is a source naming error, not evidence of the reported Payload crash/freeze. Renaming only telemetry state locals before the next request.

## Agent suggestion

### 2026-11-28T03:06:58.147Z — map-editor-freeze-payload-crash

Approved request build-request-cd65d608cc89 published successfully as generation 808. Attempt game-dll-20261128T030413043291Z-43ce44: 33.371s total, compile 23.775s, link 4.648s, publish 0.277s, compiled 2/skipped 645, no invalidations. Only changed source was src/mapeditor/map-editor.cpp; current.json reports status=ok, generation=808, changed_sources=[src/mapeditor/map-editor.cpp]. Postmortem largest measured concern is host memory pressure: minimum available 282.2 MiB (98% used), CPU peak 83.33%; no dominant build phase. Resource evidence is linked to issue-ca5eb16d9e21. No current game process or fresh Payload runtime proof was available after publish; do not infer this build fixed the reported freeze/crash.
