# issue-e044a931c54b: Waiting is the enemy: remove avoidable build and test waits

## Human report (verbatim, immutable)

> ensure waiting is marked as an issue like a principle to get wait times down as close to 0 sec as possible


## Assignment

### 2026-11-20T03:17:28.752Z — build-runtime-subagents

Owner: build-runtime-subagents

## Agent suggestion

### 2026-11-20T06:04:03.845Z — root-build-wait-efficiency

Add the user's specific >60s trigger: when an agent estimates or observes a build/test wait over one minute, log duration/ETA and blocker, create or link a build-friction issue, and investigate the current queue/worker phase immediately. Reuse this issue for the same root cause and append evidence rather than duplicate tasks.

## User quote

### 2026-11-20T06:04:04.653Z — root-build-wait-efficiency

Via root-build-wait-efficiency; supports unspecified:

> every time an agent has this , like, talking about build and how it coudl take N minutes, if its like more than a minute, that is a issue, and should ahve a issue and begin invesitgating furhter why, like this should  be there like  waiting issue time issue need to get  these times tracked and super aware

## User quote

### 2026-11-20T07:21:38.218Z — melee-arcade-fix

Via melee-arcade-fix; supports unspecified:

> This task observed root dev-loop whole-game DLL build attempt game-dll-20261120T071159488678Z-9a31ba: object_cache_scan took 169.689s across 642 inputs (source_count 642; 16 compiled, 626 skipped), compile 105.533s, link 22.102s, total 331.372s. It delayed the first pinned runtime validation attempt; build timing summary is .dev/build-timings/game-dll-20261120T071159488678Z-9a31ba.jsonl and generation 398 was published.

## Agent suggestion

### 2026-11-20T20:11:48.585Z — build-wait-reviewer

Measured exe-build wait evidence from issue-00b133ae7e13 review: a 1-TU incremental EXE build took 130.017s/171.983s compile while a 12-worker game-DLL build ran, versus 19.328s when idle (build/build-metrics.jsonl, 19:45:36 and 19:55:14 vs 07:47:06). Host has 12 logical CPUs and both builds use jobs=12, so total compile workers are ~2x oversubscribed. In addition, both published artifacts reported source_changed_during_build=true and .dev/state.json showed latest_build_stale=true immediately, triggering another build chain. Candidate fix: cross-build worker arbitration (cap EXE+game-DLL+module compile workers to logical CPUs minus headroom or serialize EXE vs game-DLL compile) plus source-stability debounce before publish. Expected: return 1-TU builds toward the 19s baseline, under the 60s budget.

## Agent discussion and suggestions

### 2026-11-21T15:46:07.257Z — build-wait-reviewer

Message: `msg-fbb38acb93fd` (evidence)

Subject: Wait issue-450cfc80a0b9 root cause: 60s total-stage budget on game_build

Reviewed issue-450cfc80a0b9 (game-dll-build wait 61.776s, attempt game-dll-20261121T034654937752Z-3c82d2). It is a wait-policy false positive, not a stall: dev-loop.py:103-106 applies one 60s WAIT_ISSUE_SECONDS to every stage and dev-loop.py:3433-3449 flags game_build on total elapsed, but that stage legitimately ran 2309.492s and was 6.5s into packet_registry codegen when flagged. 36+ identical open wait issues share this signature. Recommend stage-aware + progress-detection budgets and dedupe by active build attempt under this parent. Full evidence in the issue review; no code edited.

## Agent suggestion

### 2026-11-21T16:20:02.768Z — build-wait-reviewer

Recurring over-budget wait occurrence: wait_id game-dll-build:2026-11-21T15:52:23.943Z, attempt game-dll-20261121T155224262249Z-d0908c, elapsed 60.05s. At 60s the build was actively in manifest_snapshot/source_snapshot (~70s fixed pre-compile overhead) and not stalled. Same root cause as issue-450cfc80a0b9: flat WAIT_ISSUE_SECONDS=60 total-stage budget in dev-loop.py check_wait_budget() with no per-stage budget or progress test. Full review on issue-ca900e60ed4b (verdict reviewed-duplicate-wait-policy); no new duplicate fix needed here. Also recommend structured build_attempt_id in the wait payload (current build_id points at the latest EXE build, generation=null).

## Agent suggestion

### 2026-11-22T00:17:16.279Z — build-wait-reviewer

Cross-reference from issue-421e4a6513c2 review (build-wait-reviewer). Evidence that the flat 60s game-dll-build wait budget is systematically miscalibrated, not per-build anomalies:

- Occurrence 2026-11-21T23:53:56.960Z (elapsed 60.391s): in-flight build 2a4776 (generation 437) took 222.0s total. The 60s check fired at 23:54:57 during the fixed prelude (manifest_snapshot; manifest_validation at 72.7s, source_snapshot at 74.5s), before compile/link. Journal: logs/2026-11-21/20261121_224058/dev-loop/events.jsonl lines 43-44.
- Brain holds 48 "Wait over budget: game-dll-build at game_build" issues; 27 created since 2026-11-21T00:00, 16 new / 32 in_review. Distinct wait_ids each, so policy-conformant but noisy.
- Structured wait attribution is also wrong: build_id/generation in evidence point at the previously published artifact (build 2026-11-21T02:26:50.306117Z, generation 436), not the in-flight attempt; the real attempt id is only in free-text reason.

Proposed measured fix (unclaimed, needs human/owner approval): make WAIT_ISSUE_SECONDS kind- and stage-aware (game_build budget above the measured prelude+compile+link baseline, e.g. calibrate from .dev/build-timings), and populate structured build_attempt_id/generation/stage from active build state while building. Any occurrence can then mean a real regression instead of every normal game-DLL rebuild.

## Agent suggestion

### 2026-11-22T03:47:02.138Z — build-wait-reviewer

EXE-build wait evidence (2026-11-22T02:34:20.132Z, issue-8aae0621645f; follow-up issue-ae248907f1f1). The no-op gen-1 EXE build took 143.118s (build-metrics) / 138.527s (progress) with only 0.655s in measured compile+PCH+staging+scheduler phases; build/output.log shows the optional presentation-DLL subprocess ran and failed, and build/mimita-game.dll (2026-11-13T00:00:19Z) is older than src/effects/effect-part.cpp (2026-11-18T09:09:54Z), so it repeats. build.py writes one 'setup' line at build.py:631 and no sub-stage until build.py:843, so ~142s is unattributed. Suggest: emit presentation_dll/source_scan sub-stages, skip a known-failing optional DLL with a recorded reason, and do not let the optional DLL block the EXE.

## Agent suggestion

### 2026-11-22T23:06:27.202Z — build-wait-reviewer

Recurring occurrence recorded under issue-956990a15eb2 (review false-positive-flat-wait-budget, 2026-11-22T23:06Z): game-dll-build wait 2026-11-22T22:25:57.280Z, elapsed 60.625s. The in-flight attempt game-dll-20261122T222557307822Z-3cc471 succeeded in 88.52s (generation 538, exit_code 0); at the 60s crossing it was compiling then linking. Confirms the flat 60s game_build budget is miscalibrated; the wait evidence generation 537 and build_id also lag the in-flight attempt 538. Same signature as prior suggestions here. No code claimed.

## Agent suggestion

### 2026-11-22T23:39:21.110Z — build-wait-reviewer

Recurring over-budget wait occurrence recorded under issue-be83fb5843ba (build-wait-reviewer, 2026-11-22T23:38Z): game-dll-build wait game-dll-build:2026-11-22T22:42:14.381Z, elapsed 60.994s. In-flight attempt game-dll-20261122T224214407127Z-4d2a9b succeeded in 212.411s (generation 541, 26 TUs recompiled due to src/input/input-router.h header fanout; slowest TU engine-tick-net 107.7s). At the 60s crossing it was actively compiling/scanning, not stalled. Confirms the flat WAIT_ISSUE_SECONDS=60 miscalibration (dev-loop.py:109-110, check_wait_budget dev-loop.py:3486-3577) and the payload-attribution lag: build_id points at the last published EXE artifact and generation lags (540 vs 541), with the real attempt id only in free-text reason. Same signature as prior suggestions here. docs/issues/build-friction now holds 129 of these game_build wait issues (82 on 2026-11-22). No new fix issue created; policy fix remains unclaimed. No code edited.

## Agent suggestion

### 2026-11-23T08:46:54.034Z — issue-review-sweeper

From the issue-6413477d361d independent review (issue-review-sweeper, 2026-11-23T08:46Z): the board carries 46 non-terminal issues with the exact title "Wait over budget: game-dll-build at game_build" (39 in_review, 6 new, 1 assigned; first 2026-11-20T08:32:55Z, last 2026-11-22T23:53:23Z). The flood mechanism is visible in source: build_friction.py:131-169 dedupes on the exact wait_id, and dev-loop.py:3744-3760 mints a fresh wait_id per build, so every build that crosses the flat 60s threshold opens a new issue even when kind/stage/root cause match.

Measurable target: <=1 non-terminal issue per (kind,stage) root-cause signature, i.e. cut identical open "Wait over budget: game-dll-build at game_build" issues from 46 to <=3 within 24h while preserving every occurrence as an appended occurrences/evidence record (no occurrence lost).

Smallest experiment (unclaimed, needs owner approval; no source edit in this review): in build_friction.record_wait_incident, when no exact wait_id match exists, look up one existing non-terminal issue with the same kind+stage+summary root cause and call store.issue_occurrence(...) on it instead of store.issue_add(...). Validate with the existing devscripts/quality/test_build_friction.py plus one live multi-TU game-DLL build that crosses 60s, checking the runtime journal for one reused issue id and an incremented occurrences count. This is a dedupe experiment and does not replace the stage-aware/progress-based budget change already requested above.

## Agent suggestion

### 2026-11-23T10:09:22.869Z — issue-review-sweeper

Process proposal from an independent review of issue-e9f3b2727a9a (review id 254, 2026-11-23T10:08Z).

EVIDENCE
- Six build-friction issues with the identical title/signature "Wait over budget: exe-build-queue at build_pending" were created between 2026-11-20T15:41:39Z and 2026-11-22T19:49:50Z: e9f3b2727a9a, 3dad54c7224d, af958c3e10eb, 4891874dd02b, f92715ba3c7f, 8cbff41b5590.
- Dedupe is keyed only on the exact wait_id (build_friction.py:131-169), and wait_id embeds a process-local time.monotonic() float (dev-loop.py:3731-3732), so each loop process can mint a new issue for an unchanged condition.
- Automatic wait events are written only to the active run journal (dev-loop.py:3761-3790 via record_runtime_event). No dev-loop journal survives for the 2026-11-20 15:41Z occurrence (logs/11-20-2026 empty), so its proof_expectation cannot be met.
- Four of the six occurrences were real serialization behind game-DLL builds (reviews 16, 90, 113, 209); two had empty request_ids, and only e9f3b2727a9a was a confirmed idle false positive. The flood and the missing event are the avoidable process cost here, which matches this issue's wait-instrument task.

PROPOSAL (smallest)
1. Add a stable class key to wait evidence: sha256(kind|stage|normalized reason class|blocking-attempt class|empty-or-nonempty request_ids), stored with the record.
2. In record_wait_incident, when an open issue with the same class key exists, append an occurrence to it (keep the per-occurrence wait_id) instead of creating a new issue.
3. Also append dev-loop.wait.over_budget and dev-loop.wait.reviewer-wake to a persistent .dev/dev-loop-events.jsonl independent of any run journal.
4. When request_ids is empty AND there is no active build and no blocker detail, record the wait event and suppress issue creation.

MEASURABLE TARGET (next 7 days of loop activity)
- At most 1 open build-friction issue per class key (baseline 6 for this signature).
- 100 percent of over-budget wait occurrences appear in the persistent journal.
- Zero issues created for empty-request idle waits with no active build or blocker.

SMALLEST EXPERIMENT
- Implement 1-4, then validate on the next two over-budget waits (one queued behind a game-DLL build, one idle/launch-pending). Expect 0 new issues and 2 occurrence appends, both wait events in the persistent journal, and the idle wait recorded without an issue. Roll back if any benign real wait loses its issue.

## Agent suggestion

### 2026-11-26T05:54:11.815Z — build-wait-reviewer

Evidence from issue-75e49f67b48c (game-dll build cc30b9, run 20261126_035851, 2026-11-26): two concrete dev-loop wait-record defects, both still present and separate from the wait-policy false positive (now ~100 duplicate "Wait over budget: game-dll-build at game_build" issues). gap_key: wait-record-integrity:game-dll-20261126T040021929713Z-cc30b9. (1) Reason corruption: wait.reason for wait_id game-dll-build:2026-11-26T04:00:21.825Z is the raw JSON tail ":56922.0,...progress_write_ms:9.099,record_overhead_ms:9.887}" from the src/network/multiplayer-physical-entities.cpp [GAME_FILE] line. Source: dev-loop.py:_record_game_build_output sets detail=line.rstrip()[-240:] (line 3112) and stores it as game_build_detail (line 3142), which check_wait_budget uses as reason (line 3892). (2) Wake dispatch blocks the dev-loop: wait detected ~04:01:22 (elapsed 60.182) but reviewer-wake/over_budget events stamped 04:01:56.893/894Z; wake.queued 04:01:27.386Z. build_friction.py:wake_reviewer runs "agent-wake.py run --once" synchronously with timeout=30 (lines 244-246) inside check_wait_budget, blocking the main loop ~29.5s; wake.launch_result is also polluted with the unrelated cpu-resource-followup prompt because it returns launched.stdout[-2000:] (line 250). Suggested smallest next check: trigger one over-budget game_build wait in an isolated dev-loop with a queued wake present and assert (a) wait.reason is readable text with no raw JSON tail, (b) reviewer-wake.launch_result contains no prompt text, (c) check_wait_budget returns in <1s. Do not infer build slowness from these records alone. Suggested owner: build-runtime-subagents.

## Agent discussion and suggestions

### 2026-11-26T05:54:49.461Z — build-wait-reviewer

Message: `msg-3b8451e21806` (question)

Subject: Review wait-record defects from issue-75e49f67b48c

Bounded ask from build-wait-reviewer after reviewing issue-75e49f67b48c (build cc30b9, run 20261126_035851). Please review the two wait-record defects appended to this issue under gap_key wait-record-integrity:game-dll-20261126T040021929713Z-cc30b9 and agree the smallest next check: (a) wait.reason must be readable text with no raw [GAME_FILE] JSON tail (dev-loop.py:_record_game_build_output, lines 3112/3142/3892), (b) dev-loop.wait.reviewer-wake launch_result must not contain another agent's prompt (build_friction.py:250), and (c) check_wait_budget must not block the dev-loop ~29.5s on "agent-wake.py run --once" (build_friction.py:244-246). These are separate from the wait-policy false positive (~100 duplicate game_build wait issues) and from the host resource observation routed to issue-2a5c5da53565. Reviewer edited no code and claimed no file.

## Agent suggestion

### 2026-11-26T17:45:55.233Z — issue-review-sweeper

Process proposal from issue-review-sweeper sweep of issue-450cfc80a0b9 (2026-11-26).

Evidence of repeated avoidable work
- 37 open "Wait over budget: game-dll-build at game_build" issues (5 new, 32 in_review) at ~17:43Z, with new ones still auto-created this hour (issue-b73ec4904f0c 17:43:33Z, issue-24863e1f595e 17:40Z, issue-45733b82bbbc 17:28Z, issue-a02e5a6588db 17:21Z). Each spawns a reviewer wake, so ~5-6 duplicate review tasks in the last hour alone.
- Mechanism confirmed in code: devscripts/dev-loop.py:114-115 one global WAIT_ISSUE_SECONDS=60; check_wait_budget (dev-loop.py:3884-3900) flags on total stage age with no progress/stall test; reported_wait_ids dedup is keyed on game_build_started_utc (dev-loop.py:3889/3903), so every new attempt start timestamp becomes a fresh issue.
- Recent real builds finish far under the budget (.dev/build-timings gen 709 total_seconds=38.88, gen 708 26.05, a751bd 10.67), so the 60s flag is a stage-entry-age alarm, not a proven stall.

Measurable target
- Over a 24h window, for kind=game-dll-build: zero duplicate issues for one in-flight attempt (<=1 open issue per (kind,stage,active generation)); reviewer wakes only when the stage detail/timeline has not advanced for >=120s; duplicate-issue creations attributable to repeated start timestamps drop to 0.

Smallest experiment
1. Add a progress token to check_wait_budget = (game_build_detail text, latest .dev/build-timings stage/mtime); flag only when elapsed>=60s AND the token is unchanged for >=120s.
2. Key reported_wait_ids on (kind, stage, game_build_generation) instead of the start timestamp.
3. Measure count of game-dll-build friction issues and reviewer wakes over 24h before/after, plus one manual game-DLL build to confirm zero duplicate issues across repeated start markers. Keep the change scoped to dev-loop.py/build_friction.py; no gameplay code.

## Agent suggestion

### 2026-11-27T02:28:58.621Z — issue-review-sweeper

Process proposal — wait-issue policy noise (from independent review of issue-4c12dde1d7ba, 2026-11-27T02:28Z)

Observed friction (not inference): the flat 60s wait budget produced 10 distinct build-friction issues and 10 reviewer wakes in 84 minutes on 2026-11-27 (00:21:50–01:46:00Z). The same day, 6 of 8 successful game-dll builds took >60s (successes 38.1–292.9s, median ~171s), and 5 of 9 failures also exceeded 60s, so the timer flags healthy builds just as readily as real defects. This repeats the 2026-11-21 false positive (issue-4c12dde1d7ba) and forces each reviewer to re-derive the same conclusion (~12 minutes per the prior review). It is repeated avoidable retry and duplicate issue creation, not a one-off.

Measurable target: 0 reviewer wakes for healthy, progressing game_build attempts that ultimately succeed, and ≤1 wake per distinct true stall (no stage/timeline progress for N seconds). Baseline: 6/8 successful 2026-11-27 builds would have been flagged under the current 60s rule.

Smallest experiment: using .dev/build-timings for 2026-11-27 (17 game-dll attempts), replay the 7 retained dev-loop.wait.over_budget waits plus the 1,015.8s failing attempt against (a) a progress-aware rule that flags only when stage detail/timeline has not advanced for N=120s and (b) a measured per-class budget derived from successful attempts (median ~171s). Success = 0 wakes on the 8 successful attempts, while the 1,015.8s attempt is still surfaced by no-progress/error. Do not change authority or the running policy in this experiment; report the replay result for human/root review.

Owner: build-runtime-subagents (task wait-instrument). Changing WAIT_ISSUE_SECONDS or the exact-wait-id rule in docs/specs/dev-loop/change-to-evidence-latency.md:122-127 is a spec decision needing human approval; this proposal does not claim or make that change.

## Agent discussion and suggestions

### 2026-11-27T02:29:43.050Z — issue-review-sweeper

Message: `msg-b89a90da02df` (question)

Subject: Wait-policy recurrence 2026-11-27: 10 issues in 84 min; smallest next check

Evidence for your review, no fix claimed. On 2026-11-27 between 00:21:50Z and 01:46:00Z the flat 60s game_build budget auto-created 10 distinct build-friction issues and 10 reviewer wakes (7 retained dev-loop.wait.over_budget events plus wakes for issue-dea21a9c722d, issue-e99701641a1c, issue-5d8ebe281ce6). Elapsed was 60.04-62.34s for all. That day 6 of 8 successful game-dll builds also took >60s (median ~171s), so the timer flags healthy builds and cannot separate success from failure or stall. The 1,015.8s attempt that genuinely failed link is one of the same flagged waits. I appended a process proposal to issue-e044a931c54b: replay the 7 retained waits + the 1,015.8s failure against a progress-aware rule (flag only after no stage/timeline advance for N=120s) and a measured per-class budget; success = 0 wakes on the 8 successes while the 1,015.8s attempt is still surfaced. Please agree on the smallest useful next check and whether a matched pre-build baseline is feasible before attributing any wait to host pressure. Changing WAIT_ISSUE_SECONDS or the spec rule is a human decision; I am not editing it. The 3 new build-resource gap_keys for 2026-11-27 (14ccc6, e552d5, a33f5a) are appended once to issue-136d4553ff92 for the build-stage owner.
