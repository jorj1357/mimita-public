# issue-8bdc83a5708d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (true-positive; evidence-gaps-tracked)

### 2026-11-24T08:32:39.893Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed exact wait occurrence game-dll-build:2026-11-24T07:37:44.064Z (issue-8bdc83a5708d). Read-only: no source edited, no build/run started, no fix claimed.

VERIFIED (direct evidence)
- Wait is real and the build completed successfully. Build attempt game-dll-20261124T073744074582Z-faf01b, generation 586, revision 69a02bd62412ca58c9383b6f194cf4aec6a4fc2f7297f3c71bc2414a69989d04, success=true, total 136.225s; started 07:37:44.074Z, completed 07:40:00.299Z. Source: .dev/build-timings/game-dll-20261124T073744074582Z-faf01b.json. So 61.052s at the crossing is expected for a build that needs ~136s; the worker was not stuck.
- Stage cost (phase_durations_ms): changed_source_preflight 39048.6, compile 57999.1, link 29089.9, object_cache_scan 2702.5, publish 529.8. compile_sum 161991.8ms over jobs=10; longest TU src/vehicles/vehicle-system.cpp 55251.1ms (invalidated only because "dependency newer than object: src/input/gamepad-input.h"), then vehicle-commands.cpp 38872ms, gamepad-input.cpp 35732ms, input-commands.cpp 29429ms. The single longest TU (55.25s) alone nearly exhausts the 60s budget before preflight and link, so this wait is structural for this workload, not a stuck process.
- Resource contention during the build (build resource sampler, 120 samples @1s): mean host CPU 99.977%, peak 100%, minimum available memory 308785152 bytes (~294 MiB). A live bloodtest server (pid 29932) and client (pid 34840), run 20261124_072002, were aligned to actual_mode=bloodtest while the build ran (resource_sampling.runtime_alignment.same_run_server_client=true). Inference: CPU saturation and low free memory are consistent with slower compile, but the 55.25s single-TU floor is present regardless.
- Exactly one reviewer wake was queued for this occurrence: wake-f89090687c35 created 2026-11-24T07:38:46.200Z, launched 2026-11-24T07:45:20.268Z, attempts=2, status=launched (.dev/agent-wake/requests/wake-f89090687c35.json; wake.queued in .dev/agent-wake/events.jsonl at 07:38:46.207Z).

GAPS (first divergence first)
1. Required dev-loop wait event is not retained. No dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake record for wait_id game-dll-build:2026-11-24T07:37:44.064Z exists in any logs/**/dev-loop/events.jsonl, and no dev-loop journal contains any runtime event with wall_time in 07:30-07:39Z. This matches the silent stale-events_path failure tracked in issue-6e2d7b91e7d9: .dev/dev-loop-console.log records repeated ENOENT for logs/2026-11-24/20261124_053435/dev-loop/events.jsonl (that run has client-01/server-01/run.json but no dev-loop dir), though that log's last write is 07:34:11Z, so the 07:38 loss is inference, not a directly captured failure line. The daemon restarted as run 20261124_075114 (07:51:14Z) and does record wait events (issue-816c05804889 wait 07:53:14.620Z has both rows), so this is a pre-restart window loss. Proof expectation "Dev-loop wait event plus the issue record and reviewer wake record" is 2 of 3 satisfied.
2. reason is not a readable stage summary. evidence.reason is a mid-JSON tail: ":50296.649,\"build_attempt_id\":\"game-dll-20261124T073744074582Z-faf01b\",...,\"record_overhead_ms\":4.275}". It is the last 240 chars of the translation_unit.completed row for module-identity-provider.cpp at 07:38:34.477Z (.dev/build-timings/game-dll-20261124T073744074582Z-faf01b.jsonl line 768), from dev-loop.py:3112 detail=line.rstrip()[-240:]. Tracked in issue-6e08d89b55e3; a 4th occurrence was appended there.
3. build_id and generation in the wait record do not identify the waiting build. evidence.build_id=2026-11-24T07:18:06.640256Z is self.latest_build (a prior EXE build), generation=null, while the actual attempt is game-dll-20261124T073744074582Z-faf01b / generation 586. Cause: check_wait_budget reads self.latest_build (dev-loop.py:3896) and game_build_generation is only set at completion. The reviewer must reconstruct the attempt id from the corrupted reason. Same defect noted on issue-7d3dcfd7433b and issue-957d8656f167.

DUPLICATION / BUDGET
- This is one of 4 near-identical build-friction issues in ~40 min (issue-957d8656f167 exe-build 07:16:48; this game-dll 07:37:44; issue-91e93b7e2098 exe-build 07:44:14; issue-816c05804889 game-dll 07:53:14). Measured whole-build durations in the window were 81.5s (EXE), 136.2s and 62.3s (game DLL) against a single 60s WAIT_ISSUE_SECONDS budget. Recommend a per-kind/per-workload budget or N-consecutive-occurrence throttle, and folding repeated game_build waits under issue-7d3dcfd7433b.

ACCEPTANCE
- wait_id/stage/elapsed/request_ids/evidence present; reason and build identity are low quality (gaps 2-3); exactly one reviewer wake queued. Formal acceptance is met only in form.

NO CODE CHANGE MADE. Not closing. Smallest useful next checks: (a) durably write the wait event when no active run dir exists (mkdir or fallback journal) per issue-6e2d7b91e7d9; (b) record the active game-DLL attempt id + generation in wait evidence; (c) derive reason from the parsed [GAME_STAGE]/[GAME_BUILD] text per issue-6e08d89b55e3; (d) per-kind budget/throttle so a 55s single-TU warm rebuild is not a false-positive high-priority issue.
