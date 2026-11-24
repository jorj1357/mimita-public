# issue-723d66575ccf: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (needs_work)

### 2026-11-24T22:03:35.596Z — build-wait-reviewer

Reviewer: build-wait-reviewer

BUILD-WAIT REVIEW (build-wait-reviewer) — wait game-dll-build:2026-11-24T21:12:25.383Z / issue-723d66575ccf.

ACCEPTANCE CHECK
- wait_id/stage/reason/elapsed/budget/request_ids present; issue created once (coord.issue.opened 21:13:27.260Z) and dev-loop.wait.over_budget recorded once (logs/2026-11-24/20261124_205956/dev-loop/events.jsonl:69).
- "reviewer wake queued exactly once": CONFIRMED. One wake.queued (wake-745d58660240, 21:13:34.638Z); one claimed/launched to codex (21:13:37.006Z / 21:13:39.157Z); then retry->opencode (21:24:13.296Z) claimed/launched 21:45:07.385Z / 21:45:08.314Z (.dev/agent-wake/events.jsonl:2878-2906). No second queue for this wait_id.
- "build/run IDs": PARTIAL. related_build and related_run are empty. Evidence build_id=2026-11-24T19:27:02.742428Z is the last *completed* build, not the in-flight attempt (dev-loop.py:3896-3898 write self.latest_build/self.game_build_generation, assigned only from the result). The actual attempt is game-dll-20261124T211225830289Z-dcfa7e and the dev-loop run is 20261124_205956; neither is in the wait evidence. Same stale-identity defect already tracked in issue-9a4503f6ed68 (occurrence seq 16420).

THIS WAIT IS A FAILED BUILD, NOT JUST SLOW
- .dev/build-timings/game-dll-20261124T211225830289Z-dcfa7e.json: success=false, failed_phase=worker_timeout, worker_error="worker timed out after 300s", total_seconds=322.968, generation=621, dll generation-621.
- Timeline: changed_source_preflight (src/main-systems.cpp) phase completed at 162.4s (single TU compile 162.26s, 183101ms elapsed); link.lld_linker ran 129.32s and reported outcome=failed exit_code=1 staged_dll_bytes=0; worker tail says "[GAME BUILD] link failed in 129.32s"; supervisor stage.failed reason=timeout at 300s, exit_code=124. So the first concrete failure is the link (exit 1), but the 300s supervisor budget expired and the attempt is classified worker_timeout. The over-budget wait signal therefore masked a build failure.

RESOURCE PRESSURE (direct observation, pinned to dcfa7e)
- resource_sampling: mean_host_cpu_percent=99.986, peak=100.0, minimum_available_memory_bytes=8,781,824 (~8.4 MiB), sample_count=186 @1s.
- Concurrent live workload at 21:17:43Z: client FPS 44.97 (48/48 frames over 4.167ms budget, p95 58.33ms, max 114.65ms); server bloodtest with 1 NPC, 55 ticks / 85.78ms, run_id 20261124_210558. This is plausible contention for both the 162s single-TU compile and the 129s link, but resource causation of the link exit-1 is INFERENCE, not proven (progress.log was overwritten by a later gen-625 build, so the linker error text is unavailable).

REVIEWER-WAKE DISPATCH LATENESS
- ~31.5 min from wake.queued (21:13:34.638Z) to opencode launch (21:45:08.314Z) because codex was the primary adapter and exited before fallback. Direct new evidence for issue-84c584957e65 ("Over-budget reviewer wakes dispatch late").

SECONDARY TRACEABILITY DEFECT
- The dev-loop.wait.reviewer-wake event for this wait (events.jsonl:68, 21:13:40.696Z) captured a launch_result whose prompt belongs to issue-d6a3c6c35682 / wake-f3dc6eef636d, because build_friction.wake_reviewer runs agent-wake.py run --once (build_friction.py:244-246), which drains any pending request, not necessarily the one just queued. The recorded launch evidence can therefore name the wrong wake.

VERDICT: needs_work for the acceptance gaps (in-flight build/run IDs absent; failed-build cause not surfaced), while the one-issue/one-wake invariant held. Root cause and fix are owned by issue-9a4503f6ed68 (flat 60s budget below real game-DLL build cost; per-attempt wait_id flood; stale identity). Follow-ups left unowned: (a) confirm whether the link exit-1 was resource-induced; (b) classify worker_timeout vs reported link failure correctly; (c) include build_attempt_id/run_id in wait evidence; (d) fix run --once drain misattribution. No source edit or claim made here.
