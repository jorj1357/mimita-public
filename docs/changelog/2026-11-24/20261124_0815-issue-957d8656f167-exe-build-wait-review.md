# exe-build wait review (issue-957d8656f167)

Reviewed wait `exe-build:0:2026-11-24T07:16:48.807Z` (60.156 seconds, stage `compiling`) against attempt `exe-build-20261124T021648-1f76fc1b`, request `2026-11-24T07:16:48.578Z-f23bff39`.

Measured from `.dev/build-timings/exe-build-20261124T021648-1f76fc1b.resource.json`: started `2026-11-24T07:16:48.815Z`, completed `2026-11-24T07:18:10.387Z`, duration 81.563 seconds; 32 samples at 1 second, mean host CPU 94.703%, peak 100%. The 60-second budget (`devscripts/dev-loop.py:114`) therefore sits below the routine EXE build duration, so the wait itself is expected rather than anomalous.

The issue record and evidence JSON exist and exactly one reviewer wake was queued (`wake-b2b2d99438c1`, created `2026-11-24T07:17:50.456Z`, launched `2026-11-24T07:38:47.693Z`, status launched). Findings against the acceptance criteria:

- The required `dev-loop.wait.over_budget` event was not persisted. `record_runtime_event` targeted `logs/2026-11-24/20261124_053435/dev-loop/events.jsonl`, which does not exist; `.dev/dev-loop-console.log` contains repeated `[DEV RUNTIME] unable to record launch evidence: [Errno 2] No such file or directory` for that path. No wait event exists in any `logs/**/dev-loop/events.jsonl` for this wait id. The preceding review of issue-302ab3824727 found the same missing journal, so this is recurring, not a one-off.
- The evidence `build_id` is `2026-11-24T06:22:17.620103Z`, the previous published build, not the in-flight attempt; `devscripts/dev-loop.py:3861` reads `self.latest_build` before the new build publishes.
- `reason` is `[SKIP] external\recastnavigation\Recast\Source\Recast.cpp`, a skipped-source line rather than compiling progress.
- EXE attempts emit only `*.resource.json`/`*.resource.jsonl`; there is no primary `build-timings/exe-build-*.jsonl`, so phase transitions cannot be independently reconstructed.

Noise is confirmed: four near-identical wait issues were created in about 40 minutes (issue-957d8656f167, issue-8bdc83a5708d, issue-91e93b7e2098, issue-816c05804889) while whole EXE/DLL builds in the window measured 62-136 seconds against one 60-second budget.

The review (verdict `needs-evidence`), a fix suggestion, and a build-coordinator question are appended to issue-957d8656f167. Three new `build-resource-sampling` gap keys were appended to issue-136d4553ff92 and build-coordinator was asked to agree on the smallest next check. No source edit, build, runtime change, process control, or runtime proof; the issue remains `in_review` and unowned.
