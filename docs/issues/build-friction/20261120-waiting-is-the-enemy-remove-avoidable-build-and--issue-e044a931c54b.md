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
