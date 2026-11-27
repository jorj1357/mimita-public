# issue-778fe2977118: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed)

### 2026-11-27T07:58:27.211Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Occurrence reviewed: wait_id `game-dll-build:2026-11-27T07:23:16.007Z`, build attempt `game-dll-20261127T072316157011Z-06c929`, run `20261127_072104`.

Acceptance check (this issue): PASS for visibility/wake.
- Issue record carries wait_id, stage=game_build, reason, elapsed=60.335s, budget=60.0s, build_id, generation=783, source_generation=25, request_ids=[build-request-c6fd97046c97], and evidence.
- Exactly one reviewer wake was queued: wake-5377afd8a3ae, wake.queued 2026-11-27T07:24:17.449Z (`.dev/agent-wake/events.jsonl` L3898); issue-778fe2977118 opened 07:24:16.751Z. No duplicate issue was created.
- Both dev-loop wait events exist in `logs/2026-11-27/20261127_072104/dev-loop/events.jsonl`: `dev-loop.wait.over_budget` L52 and `dev-loop.wait.reviewer-wake` L51, both wall_time 07:24:28.72Z. So the journal half of the proof expectation is met (contrast with issue-6e2d7b91e7d9 missing-journal occurrences).
- The reviewer-wake event embeds the correct wake-5377afd8a3ae (pid 9236, log 2026-11-27T07-24-17.444Z); no launch_result misattribution this time.

Underlying build was real work, not a stalled build: attempt game-dll-20261127T072316157011Z-06c929 succeeded, generation 784, total_seconds 68.594 (postmortem compile 42.94s, link 8.44s, object_scan 5.55s; 10 TUs compiled, header fanout 8 from src/combat/weapon-config.h). At the 60.335s trigger the compile phase was still running (slowest TU src/network/server-physical-contact-actors.cpp 39.639s). Resource pressure: peak host CPU 100.0%, minimum free memory 23 MB, mean 97.47% (resource sampling 66 samples). This wait is expected under the current 60s budget, not a defect in build execution.

Two recurring defects observed, each appended to its existing issue (no duplicates):
1. evidence.reason is a mid-JSON tail `6085.139,"build_attempt_id":...,"record_overhead_ms":7.447}` rather than a readable stage summary, from dev-loop.py _record_game_build_output detail=line.rstrip()[-240:]. -> occurrence on issue-6e08d89b55e3.
2. Late reviewer dispatch with a duplicate launch: queue 07:24:17.444Z, codex claimed/launched 07:24:28.556/28.649Z (pid 9236) then exited without a review, wake.retry to opencode 07:52:52.248Z, opencode claimed/launched 07:53:02.722/02.744Z (pid 8608). Queue-to-final-launch 765.3s (~12m45s), attempts=2, two wake.launched for one wait_id, above the <=5 min target. -> occurrence on issue-84c584957e65.

This issue's own acceptance (visibility + one queued reviewer wake) is satisfied; no source fix claimed here. Do not close as behavior-proven: the value is the record and wake, which are present in the journal.
