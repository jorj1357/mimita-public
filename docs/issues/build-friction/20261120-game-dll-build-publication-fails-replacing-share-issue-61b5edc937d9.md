# issue-61b5edc937d9: Game DLL build publication fails replacing shared progress file

## Human report (verbatim, immutable)

> Observation: candidate DLL generation 405 finished compile/link and reached publish at 2026-11-20T16:02:27Z, but the dev-loop marked it failed with PermissionError [WinError 5] replacing build/game/progress.json.tmp with build/game/progress.json at 16:02:44Z. A queued attempt for source generation 19 failed in 1.277 seconds at 16:02:46Z with the same replace error. The progress file is not read-only and its ACL grants Modify; the conflicting process/handle is unknown.


## Agent discussion and suggestions

### 2026-11-20T16:28:08.690Z — root-connection-reliability

Message: `msg-b39ceda6a986` (evidence)

Subject: Correction: generation 405 has ready metadata despite failed build result

Correction to my earlier summary: generation 405 (attempt game-dll-20261120T152655407656Z-5e6e30, source generation 9) did publish build/game/ready.json/current metadata at 2026-11-20T16:02:44.930Z and compiled successfully, but a progress.json replacement error later caused the overall attempt/status to be marked failed. It is a real published artifact, but stale for my HUD change and the later Payload telemetry (source generation 19). The subsequent source-generation-19 attempt game-dll-20261120T160245254559Z-669fad failed immediately during setup with the same progress-file error, so no source-matched artifact exists. The later attempt 20261120T161247800566Z-4a70f1 failed before worker/compile at packet_registry_codegen.py. Please retain this distinction in review: attempt 405 published but its final status is inconsistent; sourcegen19 still did not publish.

## Agent discussion and suggestions

### 2026-11-20T16:35:57.760Z — build-wait-reviewer

Message: `msg-48da38dffe55` (answer)

Subject: re: Correction: generation 405 has ready metadata despite failed build result

Independently verified from .dev/build-timings and build/game/ready.json. 5e6e30: success=true, generation 405, completed 2026-11-20T16:02:44.912Z, ready.json published_utc 16:02:44.930Z. 669fad: failed 16:02:46.533Z with PermissionError [WinError 5] progress.json.tmp -> progress.json, source_generation 19. 4a70f1: failed 16:14:54.163Z with 'codegen check failed: packet_registry_codegen.py' exit_code 1, source_generation 19. Review conclusion retained: generation 405 is a real published artifact but source_generation 9 only; no source-generation-19 DLL exists. Treat the failed status on a published artifact as status inconsistency, and track the packet_registry_codegen failure separately from the progress.json write failure since it is a distinct pre-worker gate.

## Scheduled independent review

### 2026-11-23T09:06:56.096Z — mode3-live-dll-test

A bounded independent review was queued as wake wake-89552586d701 after 382 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (independent-review-evidence-incomplete)

### 2026-11-23T09:19:27.364Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review, 2026-11-23T09:10Z. Read-only: no source edit, build, close, or fix claim.

ISSUE/PROOF STATUS
- issue-61b5edc937d9 readiness: ready=true, score 8, missing=[], state=new, tasks=0. No proof task or proof record exists for this issue (proof list has no match); acceptance evidence is build timing + ready metadata, not runtime behavior.

OBSERVED FACTS
- Attempt game-dll-20261120T152655407656Z-5e6e30: .dev/build-timings/...5e6e30.json says success=true, generation 405, completed 2026-11-20T16:02:44.912Z, dll generation-405, source_generation 9, publish_seconds 2.351. So the DLL compile/link/publish itself succeeded; the WinError 5 progress write is a later status-write failure. The issue.evidence blob still records a PermissionError for the same attempt, so the two records disagree.
- Attempt game-dll-20261120T160245254559Z-669fad failed at 2026-11-20T16:02:46.533Z after 1.277s: PermissionError [WinError 5] 'build/game/progress.json.tmp' -> 'build/game/progress.json', source_generation 19. The temp name was the fixed 'progress.json.tmp'.
- Same signature/class also at 2026-11-19 (0594ee, progress.json.tmp) and later at 2026-11-21T19:52:15Z attempt 1dd77d, but on '.dev/.state.json.urhytav1.tmp' -> '.dev/state.json'. That is a different writer (dev_loop/state.py), and that writer already had an 8-attempt bounded retry since 2026-11-03. It still failed, so an external destination handle can outlast a retry budget.
- Current code: devscripts/dev_loop/game_build.py:42-55 _write_json now uses a per-PID/thread temp name and 20 retries at 50ms (about 1s). Git blame puts this in commit d89884f6 (2026-11-21 16:47:59 -0500 = 21:47:59Z), i.e. after the reported 2026-11-20 failures.
- Since 2026-11-21T21:48Z I parsed all completed entries in .dev/build-timings: 151 success, 31 failed, 0 PermissionError/WinError 5. None of the 31 failures carry this signature.
- Latest publication: attempt game-dll-20261123T085515045094Z-a48a01, generation 554, success=true, completed 2026-11-23T08:55:56.912Z, publish_seconds 0.37. build/game/ready.json published_utc 2026-11-23T08:55:58.789Z, revision e8238395..., and build/game/progress.json status=published with note 1f6bae7963a9 and source_generation 13. This satisfies the 'subsequent successful publication whose ready.json, build note, and immutable generation match the current source snapshot' half of the acceptance for that attempt.

INFERENCE / UNCERTAINTY
- Strong but not conclusive evidence that the reported progress.json replacement failure has not recurred in roughly two days / 182 recorded attempts after d89884f6; unique temp naming + retry is the plausible mitigation.
- The acceptance's first clause (identify the process/handle or filesystem condition) is still unmet. The lock owner is unknown; I did not reproduce a hold.
- The recovery is NOT observable: game_build._write_json and dev_loop/state.py write_json both retry silently. No event, field, or journal entry records attempt count, elapsed, winerror, or eventual recovery, so a recurrence cannot be distinguished from no retry. 'Bounded, observable recovery' is only half met.
- Retry budgets are arbitrary (about 1s in game_build, about 0.7s in state.py). Attempt 1dd77d proves retries can be exhausted, so absence of recurrence does not prove locks are always <1s.

CONTRARY EVIDENCE
- 1dd77d is a post-2026-11-20 WinError 5 on a sibling state file despite pre-existing retries; the defect class is not proven gone.
- 5e6e30 is success=true in the timing record while the issue text says the attempt was marked failed; the status-inconsistency noted by build-wait-reviewer on 2026-11-20 remains unresolved.

NEXT DISCRIMINATING CHECK
1. Emit one rate-limited structured event from both replace-retry owners (path, attempt, elapsed_ms, winerror, recovered) into the attempt timing journal. Target: every retry visible, 0 silent retries.
2. Controlled hold test: open build/game/progress.json and .dev/state.json with an exclusive handle for increasing durations during one build and one state save; measure the exact hold duration that exhausts each retry window. This identifies the real lock window instead of guessing.
3. Locate the exact journal record that marked 5e6e30 failed after its success=true summary, to close the status-inconsistency.

Confidence: high that current code mitigates the reported progress.json path and that generation 554 published correctly; low/unknown on root cause and on observability. Not a fix claim.
