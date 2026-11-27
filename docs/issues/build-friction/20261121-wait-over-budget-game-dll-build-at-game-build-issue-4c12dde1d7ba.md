# issue-4c12dde1d7ba: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-needs-policy-fix)

### 2026-11-21T16:23:37.996Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed occurrence: wait_id game-dll-build:2026-11-21T15:54:27.410Z (build attempt game-dll-20261121T155427526457Z-6240d5, source_generation 4, revision fc35e6fc2eae1c97170f8af1fa4ce7c214e2195039a3f324f645f6f27eec08f5, request auto-4). Issue opened 2026-11-21T15:55:27.899Z by dev-loop PID 33892; reviewer wake wake-8360523c37ab was queued once at 15:55:28.299Z.

Evidence
- .dev/build-timings/game-dll-20261121T155427526457Z-6240d5.json: started 15:54:27.526Z, completed 16:07:57.840Z, total_seconds 810.257, success=false, failed_phase=link, worker_error: link failed in 47.206s (undefined references to mimita::game::replayRecorderPtr(), npcSystemPtr(), mpContextPtr(), gameStatePtr(), replayPlayerPtr(), queryTransportSwapState()).
- Matching .jsonl timeline: codegen resident_state 0.394s, codegen packet_registry 3.923s, manifest_snapshot 11.320s, source_snapshot 16.771s, changed_source_preflight 21.471s completed 15:55:26.631Z, object_cache_scan started 15:55:26.631Z at elapsed 59.105s. The wait fired at 60.421s, i.e. the build had just entered object_cache_scan and was advancing. It was not stalled.
- Wake record .dev/agent-wake/requests/wake-8360523c37ab.json: attempts=2. The first codex adapter failed with: The model gpt-6-luna is not supported when using Codex with a ChatGPT account; the scheduler retried opencode at 16:07:34Z, and this review is that retry, about 12 minutes after the issue.

Findings (first divergence)
1. The wait policy, not the build, produced this issue. devscripts/dev-loop.py:108-109 sets one flat WAIT_ISSUE_SECONDS=60, and :3391-3407 flags game-dll-build on total elapsed in the stage with no progress/stall test. A 60s budget is far below the normal duration of game_build (this attempt 810s), so every real DLL build logs an over-budget wait while it is working. This occurrence is a policy false positive. docs/specs/dev-loop/change-to-evidence-latency.md:122-127 currently mandates the exact-wait-id rule, so changing the budget is a spec/self-improvement decision, not a code bug.
2. The structured wait record does not identify the in-progress attempt. check_wait_budget sets build_id to self.latest_build (last published artifact, here 2026-11-21T02:26:50.306117Z) and leaves generation null; the active attempt id is only inside the free-text reason. dev-loop.py:2701 keeps only the last 240 characters of the worker line, so the stored reason starts mid-JSON at 59068.684,... and drops the stage/event prefix. Acceptance asks for exact build IDs; add active attempt id, stage, source_generation, and last-progress time as structured fields.
3. Reporting identity/observability. The issue was opened by dev-loop PID 33892, a same-checkout duplicate daemon (docs/changelog/2026-11-21/20261121_161330-dll-build-and-daemon-inventory-investigation.md records daemons 19168, 13660, 33892). No retained dev-loop events.jsonl contains a dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event for this wait_id; the surviving daemon journal logs/2026-11-21/20261121_152950_01/dev-loop/events.jsonl (PID 13660) has none. The wait cannot be traced to a run journal, and each duplicate daemon reports its own waits.
4. Scale: 41 open Wait over budget build-friction issues (20 new, 21 in_review) as of this review, all produced by the same flat-budget behavior.

Real friction in this attempt (separate owner, already reported)
- The link failure of attempt ...6240d5 is recorded append-only under issue-136d4553ff92 (root-dll-status-review, 2026-11-21T16:09:59Z) with the same undefined symbols. Duplicate-daemon/concurrent object-cache writes remain an unproven hypothesis there. No action for this reviewer.

Missing fields
None. issue readiness is ready=true, missing=[], score 8/8; reproduction, expected behavior, acceptance, paths, proof expectation, and evidence are present.

Recommendation (report only; reviewer made no edit and claims no runtime proof)
1. In dev-loop.py replace the flat game_build budget with a stage-aware budget plus progress detection: flag only when the stage detail or timeline has not advanced for N seconds, so an actively compiling build does not page a reviewer. Amend change-to-evidence-latency.md accordingly.
2. Put active attempt id, stage, source_generation, and last_progress_utc into the wait dict and issue evidence, and stop keeping only the 240-character tail of the stage line (dev-loop.py:2701).
3. Let only the daemon that owns the checkout lock report waits, so duplicate daemons such as 33892 cannot create parallel issues; retain the wait runtime event in the owning run journal.
4. Fold these near-identical wait issues into parent issue-e044a931c54b task wait-instrument instead of leaving one issue per timer expiry. Related measuring issue: issue-136d4553ff92.

Verdict: reviewed-needs-policy-fix. The root cause is the wait policy and wait evidence, not this build's progress; the code fix belongs to the wait-instrument owner under human approval. No source or leased file edited.

## Agent discussion and suggestions

### 2026-11-21T16:23:54.262Z — build-wait-reviewer

Message: `msg-9df720dc53c0` (evidence)

Subject: Wait issue-4c12dde1d7ba: false positive; wait record lacks active attempt id

Reviewed wait issue-4c12dde1d7ba (wait game-dll-build:2026-11-21T15:54:27.410Z). It fired at 60.421s while attempt game-dll-20261121T155427526457Z-6240d5 was advancing: changed_source_preflight finished and object_cache_scan started at elapsed 59.105s; the attempt later ran 810.257s and failed link in 47.206s. So this is a wait-policy false positive, matching the flat WAIT_ISSUE_SECONDS=60 at dev-loop.py:108-109 and the total-elapsed test at dev-loop.py:3391-3407, not a stall. The issue was opened by duplicately running daemon PID 33892, and no retained dev-loop journal contains the over_budget or reviewer-wake event for this wait_id. The wait evidence also omits the active attempt id (build_id points at the last published build; the attempt id survives only in the truncated reason because dev-loop.py:2701 keeps the last 240 chars). Requested under parent issue-e044a931c54b task wait-instrument: (1) stage-aware, progress-aware budget; (2) structured attempt id/stage/last-progress in wait evidence; (3) only the lock-owning daemon reports waits; (4) fold the 41 open per-timer wait issues into the parent. Reviewer made no code edit; code change needs human approval.

## Scheduled independent review

### 2026-11-27T02:17:50.862Z — map-authored-parity

A bounded independent review was queued as wake wake-3f30ae257296 after 820 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed-needs-policy-fix)

### 2026-11-27T02:28:32.429Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent review — 2026-11-27 recurrence sweep (issue-review-sweeper)

Verdict: reviewed-needs-policy-fix (unchanged root cause; recurrence confirmed and quantified). Report only: no source edited, no build/test started, no fix claimed, issue not closed.

Observed facts (fresh, 2026-11-27 UTC)
- Readiness: issue readiness ready=true, score 8/8, missing=[]; state in_review. Proof expectation is "Dev-loop wait event plus the issue record and reviewer wake record"; this is a build-friction instrument, not a gameplay/runtime behavior claim.
- Prior review stands: build-wait-reviewer (2026-11-21T16:23:37Z) found the 2026-11-21 occurrence fired at 60.421s while attempt game-dll-20261121T155427526457Z-6240d5 was advancing, and that attempt later ran 810.257s and failed at link. The real link defect is tracked separately (issue-136d4553ff92).
- Retained 2026-11-27 dev-loop journals contain 7 `dev-loop.wait.over_budget` events from 00:22:08Z to 01:46:16Z: 6 game-dll-build at game_build (elapsed 60.04–62.34s) and 1 exe-build at compiling (60.86s). Each created its own issue. Three additional game-dll-build wait issues are evidenced by reviewer wakes: issue-dea21a9c722d (01:34, 62.229s), issue-e99701641a1c (01:39, 60.319s), issue-5d8ebe281ce6 (01:24, 60.492s).
- Net: 10 distinct build-friction issues were auto-created 00:21:50–01:46:00Z (84 minutes), all within ~2.3s of the 60s threshold: issue-ee02d14f4ccd, issue-2aa79d9fe477, issue-551c405dc80c (exe), issue-51b425637ea9, issue-a9f9f82c92f7, issue-a996e6c1bfc2, issue-dea21a9c722d, issue-e99701641a1c, issue-5d8ebe281ce6, issue-f6177f624ba9.
- The waited-on builds kept progressing well past the timer: game-dll-...002049 total 112.0s (compile failed), ...002302 total 1,015.8s (link failed), ...005142 total 90.9s (link failed), ...014456 total 166.0s (success).
- Build-duration baseline for 17 game-dll attempts started 2026-11-27 (.dev/build-timings): 8 successes at 38.1, 51.4, 109.3, 166.0, 173.8, 186.7, 190.6, 292.9s; 9 failures at 33.9–1,015.8s. 6 of 8 successes and 5 of 9 failures exceeded 60s. A flat 60s budget therefore flags healthy builds and does not separate success from failure or stall.

Inference / uncertainty
- Root cause is unchanged: a single flat `WAIT_ISSUE_SECONDS` (dev-loop.py, default 60) applied to game_build by total elapsed in check_wait_budget, with no progress/stall test. The spec (docs/specs/dev-loop/change-to-evidence-latency.md:122-127) mandates one issue per exact wait occurrence with wait_id dedupe, so widening/altering the budget is a spec/self-improvement decision, not a code regression. This directly reconfirms the prior verdict.
- Duplicate-daemon reporting is still plausible but unproven this sweep: at least two daemons reported waits (PID 36076 run 20261127_001718; PID 15304 run 20261127_014331). `reported_wait_ids` is per-daemon in-memory, so it cannot dedupe across daemons. I did not verify which daemon held the checkout lock.
- Acceptance "exact build/run IDs" is only partly met for this issue: evidence carries build_id and source_generation but generation=null and no run_id; build_id points at the last published artifact, not the attempt. Newer events populate generation (750 at 01:46) but logged source_generation=0 there.

Contrary evidence (what improved)
- The prior review item #2 is partially fixed in the newer daemon: run 20261127_014331 logs generation=750 and a clean `[GAME BUILD] revision=... sources=...` reason, not a mid-JSON tail. The older daemon run 20261127_001718 still logged generation=null and truncated-JSON reasons, so the fix is not uniform across daemons.
- Observability is better than at the 2026-11-21 review: retained dev-loop journals now contain the over_budget and reviewer-wake events for the newer runs.
- Wake for this exact occurrence was created once (wake-8360523c37ab, 2026-11-21T15:55:28Z), but its record ends status=failed error="adapter process exited" even though review id 48 was appended at 16:23. "Reviewer wake queued exactly once" is met; wake-completion status is not a reliable success signal.
- Not all waits are false positives: the 1,015.8s attempt genuinely failed link. The flat timer still did not localize that failure; it fired at 60s. Real defects remain owned by issue-136d4553ff92 and the compile/link issues.

Next discriminating check (cold-handoff ready)
1. Fold the 10 new 2026-11-27 issues into parent issue-e044a931c54b task wait-instrument rather than leaving one issue per timer expiry.
2. Using .dev/build-timings, derive a per-class baseline for successful game_build (2026-11-27 set median ~171s, range 38–293s) and either (a) a stage-aware + progress-aware budget that flags only when stage detail/timeline has not advanced for N seconds, or (b) a measured per-class budget. Replay the 7 retained 2026-11-27 wait events plus the 1,015.8s failing attempt against the new rule. Success = 0 reviewer wakes on the 8 successful attempts, while the 1,015.8s attempt is still surfaced by no-progress/error, not by the flat 60s timer.
3. Verify the wake-completion status path so a completed review does not leave the wake at status=failed.
Owner: build-runtime-subagents (wait-instrument). Changing the budget/spec needs human approval; not implemented in this review.

Resource-gap note: brain_check build_resource_gaps for game-dll-20261127T021242231192Z-14ccc6, ...T021015165489Z-e552d5, and ...T020057521108Z-a33f5a are captured but all missing matched_prebuild_resource_baseline, host_commit_charge_and_limit, and journal_aligned_live_mode_workload_and_frame_tick_metrics. These are appended once to the build-stage observability umbrella (issue-136d4553ff92); absence of a sample is unknown, not zero, and I do not claim host pressure caused any wait.
