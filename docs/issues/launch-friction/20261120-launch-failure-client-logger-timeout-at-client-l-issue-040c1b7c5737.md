# issue-040c1b7c5737: Launch failure: client_logger_timeout at client_logger_readiness

## Human report (verbatim, immutable)

> managed mode did not become active before the timeout


## User quote

### 2026-11-20T07:19:50.085Z — melee-arcade-fix

Via melee-arcade-fix; supports unspecified:

> Attempted isolated test run 20261120_071757 pinned to EXE artifact 2026-11-20T05:38:11.772319Z and DLL generation 398. Server PID 10876 exited 25s before logger.started, exit_code=1, so its events.jsonl was never created; run manifest and dev-loop journal are under logs/2026-11-20/20261120_071757. The same failure blocks melee runtime validation.

## Agent discussion and suggestions

### 2026-11-20T07:19:50.405Z — melee-arcade-fix

Message: `msg-939d9bd8868c` (message)

Subject: New pinned test server logger readiness failure

Isolated run 20261120_071757 pinned game DLL generation 398 but the server exited with code 1 before logger.started; manifest and dev-loop journal are under logs/2026-11-20/20261120_071757. This repeats your logger readiness launch issue. I requested a current EXE build via root dev-loop to get a valid runtime attempt; please review if you find the startup cause.

## User quote

### 2026-11-20T07:21:38.411Z — melee-arcade-fix

Via melee-arcade-fix; supports unspecified:

> Second isolated attempt 20261120_072010 pinned generation 398 and the same EXE artifact after root dev-loop build completed, but it again remained starting and failed before logger.started; no per-process server events.jsonl was created. It confirms a current launcher readiness blocker independent of selecting generation 398.

## Scheduled independent review

### 2026-11-22T11:49:09.837Z — cpp-latency-next-step

A bounded independent review was queued as wake wake-5e491b5b046c after 281 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (inconclusive)

### 2026-11-22T12:02:01.822Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper, wake-5e491b5b046c). No source edits, no build, no run, no close.

OBSERVED FACTS
1. Issue state: issue-040c1b7c5737 is auto-created launch-friction, state new, owner empty, 0 tasks, 0 prior reviews; readiness 2/8 with missing reproduction, expected_behavior, acceptance, paths, proof_expectation, risk. proof list contains no proof record for this issue.
2. Original launch 2026-11-20T06:57:58.078Z selected EXE artifact 2026-11-20T05:38:11.772319Z (sha256 06e0eafc8e0291960e9d19162dd5e6355d49d88d702b6febf7a6e6890d5a545b), launch_mode 57, mode_name Payload - funworld3 NPC behavior, map funworld3. stages.jsonl: server_reuse pid 4076; client_process_start pid 9652; client_logger_readiness failed at elapsed 22473.9 ms with stage_elapsed 20072.3 ms, exit_code null (client stayed alive), timeout_seconds 20; mode_start then failed after a further 45034.2 ms; complete failed at 67566.6 ms.
3. The build_identity_match stage reports mismatch only because client_ready=false; server_build_id equals selected_build_id in the same record. It is a readiness label, not an artifact mismatch.
4. The same signature recurred at 2026-11-20T23:02:14.493Z (launch.json issue_result appended to this issue): server reused pid 33272, client pid 32284 stayed alive with exit_code null and no logger.started within 20420.7 ms. Complete failed at 27450.2 ms, i.e. the downstream 45 s mode_start wait seen in the 06:57 attempt is absent. Current devscripts/dev-loop.py gates mode readiness on client_ready, so that secondary masking has been fixed, but the core pre-logger client stall persisted in that later sample.
5. The exact artifacts cited by the issue are gone. The launch journal path is logs/2026-11-20/20261120_065758/dev-loop/events.jsonl; on 2026-11-22 the directory logs/2026-11-20 is empty. The two supporting melee runs quoted in this issue, 20261120_071757 and 20261120_072010, are also absent. A repository search found no surviving copy of run id 20261120_065758.
6. Corpus of 51 .dev/launch-reports: client_logger_timeout occurred 4 times, all on 2026-11-20 (06:56:24.615Z, 06:57:58.078Z, 07:02:19.171Z, 23:02:14.493Z). No client_logger_timeout appears in reports after 23:02:14.493Z through 2026-11-22T01:10:31.858Z. The latest sampled launch (2026-11-22T01:10:31.858Z, mode 45, build 2026-11-21T02:26:50.306117Z) reached client_logger_readiness ready in 293.0 ms and completed in 34844.7 ms.
7. Related owner issue-7a22b715568b (server_logger_timeout, owner launch-latest-stable-fix) has a narrower server finding: instrumented run 20261120_074813 measured quota mutex 3 ms, state map 3 ms, quota init 44 ms, reconcile 86 ms, journal open 86 ms, server logger.ready 2.593 s, and the timeout stopped. No equivalent client-side breadcrumb result is recorded here.

INFERENCE AND UNCERTAINTY
- In both 06:57 and 23:02 the client process existed and remained alive while logger.started was never written. This localizes the failure to client startup before the logger readiness marker, not to process spawn, artifact validation, or server reuse. The exact first stuck step is unproven because the one artifact that would show it (the per-process client journal) is the artifact that is missing.
- Plausible mechanism is the same logger-init family already probed on the server (run lease, quota lock, reconcile/prune, fopen), but there is no client-side instrumentation evidence for this signature. Quota/log-root contention during concurrent runs is a hypothesis, not established fact.
- Late non-recurrence is weak positive evidence, not a fix proof: the post-11-20 sample is small, later launches used different modes (mode 45) and a later build, and the issue has no assigned owner or proof task.

CONTRARY EVIDENCE
- build_identity_match mismatch must not be read as a build/artifact mismatch; the identity fields match and only client readiness failed.
- The server was reused in both failing samples, so this is not the server_logger_timeout path.
- Recent successful launches show client logger readiness can complete in under 300 ms on current source/artifacts, so a deterministic code defect is not established.

NEXT DISCRIMINATING CHECK (do not run without an owning lease)
- On the next client_logger_timeout, preserve the exact run directory and capture: client stderr/tail probe, crash dir, and a stack of the still-alive client. Add bounded client-side breadcrumbs mirroring the server instrumentation around run lease, quota lock, initialize/reconcile/prune, and fopen. Acceptance for resolution: one capture whose client journal/stack identifies the first stuck step, or 20 consecutive same-build mode-57 launches with zero recurrence plus a reasoned intermittent classification. Until then this stays inconclusive and unowned; do not claim fixed.

PROCESS REVIEW
- The demonstrated process gap is evidence loss, not review latency: the issue and its supporting runs point at journals that devscripts/dev_loop/game_build.py::_prune_logs (line 448; called at 855 with keep_days=3 and at 862 with keep_days=2 under disk pressure) can delete before resolution. Dangling issue/proof references force redundant reruns and reduce evidence quality.
- Proposal appended to the existing retention/storage owner issue-3f776ce60511 with a measurable target and smallest experiment. No other repeated delay, avoidable retry, human-only step, or safe capability proposal was supported by this sweep evidence.
