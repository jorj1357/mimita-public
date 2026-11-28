# Map editor input and Payload crash diagnostics

- Task: Improve `events.jsonl` evidence for random map editor freezes and explain client crashes, with special attention to Payload on `mimita-pipes`.
- Agent: `map-editor-freeze-payload-crash`
- Date: 2026-11-28 UTC
- Status: Source instrumentation is present in the shared branch and DLL generation 808 published. Fresh runtime acceptance remains unproven.

## Findings and implementation state

The existing live client journal `logs/2026-11-27/20261127_230430/client-02/events.jsonl` showed the client reached `network.mode.active` on `mimita-pipes` in Payload. It also contained 11,290 `game.frame.render-failed` events over about 46.4 seconds, all reporting `HostResult::InternalError`; the previous path omitted the caught exception text. No process crash was reproduced. The old editor input events did not identify which keys or mouse buttons were pressed.

Current shared-branch instrumentation records map-editor key names/codes/modifiers (without typed text), mouse button transitions/modifiers/cursor/selection state, and durations/counts for copy, paste, save, reload, and geometry rebuild. `HostRuntime::driveRender` includes the caught exception summary, rate-limits repeated failures, and emits a recovery event. The crash handler appends a `process.crash` JSONL event with process/build/launch identity, reason, exception code/address, and crash report/minidump paths and write status. These existing changes were in branch commit `d1d1df34`; this task corrected compile blockers in the editor TU.

### Exact source edits in this task

File: `src/mapeditor/map-editor.cpp`

- Lines 3029, 3032, 3035, 3038: replaced each invalid C-array `.begin()`/`.end()` call with `std::begin(array)` / `std::end(array)`.
  - Before: `mPropertyPosition.begin(), mPropertyPosition.end(),`
  - After: `std::begin(mPropertyPosition), std::end(mPropertyPosition),`
  - The same replacement applies to `mPropertySize`, `mPropertyRotation`, and `mPropertyScale`.
- Lines 3773-3801 and 3806-3843: renamed the input booleans from `right` and `left` to `rightDown` and `leftDown` so they do not collide with the existing GLFW button-state locals in `updateInput`.
  - Before: `const bool right = InputRouter::mouseDown(GLFW_MOUSE_BUTTON_RIGHT);`
  - After: `const bool rightDown = InputRouter::mouseDown(GLFW_MOUSE_BUTTON_RIGHT);`
  - Before: `const bool left = InputRouter::mouseDown(GLFW_MOUSE_BUTTON_LEFT);`
  - After: `const bool leftDown = InputRouter::mouseDown(GLFW_MOUSE_BUTTON_LEFT);`

## Build evidence

- `game-dll-20261127T232810391707Z-7e8324`: failed compilation because C arrays do not have `.begin()`/`.end()`; no publish.
- `game-dll-20261128T030246290451Z-e9f4a7`: failed changed-source preflight because telemetry locals conflicted with earlier `left`/`right`; no publish.
- `game-dll-20261128T030413043291Z-43ce44`, approved request `build-request-cd65d608cc89`: published generation 808 successfully. `build/game/current.json` reports status `ok` and changed source `src/mapeditor/map-editor.cpp`. Total 33.371 seconds; compile 23.775, link 4.648, publish 0.277; 2 compiled, 645 skipped, no invalidations.
- Latest postmortem recorded peak CPU 83.33%, minimum available physical memory 282.2 MiB (98% used). Resource samples cover 33.363 seconds with 12 logical CPUs; the largest process was `FL64.exe` (PID 3744, classifier role `other`, 4.63 GB private bytes). Runtime alignment was unavailable; no exact game mode/map/workload can be attributed to that process. Evidence appended to `issue-ca5eb16d9e21`.

## Runtime and acceptance limits

The active dev-loop had no game process after the build, and `evidence-watch.py list --json` found no processes. Available memory was about 1.14 GiB immediately afterward. I did not start a new client under that pressure. Therefore generation 808 proves compilation/link/publish only. It does not prove the editor remains within 4.167 ms, isolate the freeze trigger, confirm `mimita-pipes` Payload runs without a crash, or prove a crash event appears in a fresh journal. The issue remains open/unproven. The existing journal shows a repeated render failure with no exception details rather than a confirmed process crash.

## Changed files

- `src/mapeditor/map-editor.cpp`: compile corrections listed above; this source is part of generation 808.
- `docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-07977478c8fc.md`: appended observed journal evidence, build postmortems, and current proof limits through coordinator issue discussion.
- `docs/issues/improvement/20261124-dev-loop-launch-reliability-needs-loud-diagnosab-issue-a5ca672b6628.md`: linked the user's crash-explanation request to the existing launch/crash reporting work.
- `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md`: linked as a related investigation; no confirmed connection to this Payload symptom.
- `docs/issues/improvement/20261124-multi-agent-dev-loop-memory-pressure-functionally-issue-ca5eb16d9e21.md`: appended resource sample evidence.
- `docs/changelog/2026-11-28/20261128_0305-map-editor-input-payload-crash-telemetry.md`: this session record.

No map/config data was edited. No running process was stopped or replaced.

