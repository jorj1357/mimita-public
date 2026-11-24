# issue-957d8656f167: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (needs-evidence)

### 2026-11-24T08:06:41.978Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed exact wait occurrence exe-build:0:2026-11-24T07:16:48.807Z.

VERIFIED (direct evidence)
- Wait is real: attempt exe-build-20261124T021648-1f76fc1b started 2026-11-24T07:16:48.815Z, completed 2026-11-24T07:18:10.387Z, duration_seconds=81.563 (source: .dev/build-timings/exe-build-20261124T021648-1f76fc1b.resource.json). Budget is WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:114), so the 60.156s observation is expected for a full EXE build.
- Issue record and evidence JSON are present.
- Exactly one reviewer wake was queued: wake-b2b2d99438c1 created 2026-11-24T07:17:50.456Z, launched 2026-11-24T07:38:47.693Z, attempts=2, status=launched (.dev/agent-wake/requests/wake-b2b2d99438c1.json).

GAPS (first divergence first)
1. Required dev-loop wait event is missing. record_runtime_event targeted logs/2026-11-24/20261124_053435/dev-loop/events.jsonl (a stale, absent run dir, from an earlier consolidated launch) and failed silently. .dev/dev-loop-console.log contains repeated '[DEV RUNTIME] unable to record launch evidence: [Errno 2] No such file or directory' for that path. No dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake record exists in any logs/**/dev-loop/events.jsonl for wait_id exe-build:0:2026-11-24T07:16:48.807Z. Proof expectation 'Dev-loop wait event plus the issue record and reviewer wake record' is therefore only 2 of 3 satisfied.
2. Stale build_id in evidence. Evidence build_id=2026-11-24T06:22:17.620103Z (the previously published build) while the attempt is exe-build-20261124T021648-1f76fc1b / request 2026-11-24T07:16:48.578Z-f23bff39. Cause: dev-loop.py:3861 reads self.latest_build, which still points at the prior build while the new one is in flight. Acceptance asks for build/run IDs; the reported build_id is not the build that waited.
3. reason is not progress. reason='[SKIP] external\\\\recastnavigation\\\\Recast\\\\Source\\\\Recast.cpp', a skipped-source line, not the stage progress. The later occurrence issue-91e93b7e2098 recorded 'compiling 0/45 [CXX] src\\\\network\\\\server.cpp', so reason quality varies by path.
4. No structured timing journal for this EXE attempt: only .resource.json/.resource.jsonl exist; there is no build-timings/exe-build-20261124T021648-1f76fc1b.jsonl (all exe-build-* attempts share this pattern). Phase transitions (staging/compiling/linking/publish) are not independently verifiable from the attempt record; the resource samples show phase=staging then build_complete with a large gap.

DUPLICATION / NOISE (confirms issue risk)
Four near-identical build-friction issues in ~40 minutes for routine builds: issue-957d8656f167 (exe-build 07:16:48), issue-8bdc83a5708d (game-dll-build 07:37:44), issue-91e93b7e2098 (exe-build 07:44:14), issue-816c05804889 (game-dll-build 07:53:14). Measured whole-build durations in this window were 81.5s (EXE) and 62-136s (game DLL), i.e. routinely above the single 60s budget. The wake also sat ~21 minutes (queued 07:17:50, launched 07:38:47).

NO CODE CHANGE MADE. Not closing. Smallest useful next check: ensure the wait runtime event is durably written when no launch run exists (fallback journal or create parent dir), record the current attempt id/request id rather than prior latest_build, and use per-kind budgets or N-occurrence throttling so routine EXE/DLL builds do not create one issue+wake each.

## Agent suggestion

### 2026-11-24T08:06:47.181Z — build-wait-reviewer

Evidence-backed review of exe-build:0:2026-11-24T07:16:48.807Z: the wait is real (attempt exe-build-20261124T021648-1f76fc1b ran 81.563s, started 07:16:48.815Z), but (a) the required dev-loop.wait.over_budget event is absent because record_runtime_event targeted the stale/absent logs/2026-11-24/20261124_053435/dev-loop/events.jsonl and only printed 'unable to record launch evidence' to .dev/dev-loop-console.log; (b) evidence build_id is the previous build 2026-11-24T06:22:17.620103Z, not the in-flight attempt; (c) reason is a '[SKIP]' line rather than compiling progress; (d) exe-build attempts emit only *.resource.jsonl, no structured phase timing journal. Smallest next check: write the wait event to a per-build fallback journal (creating the parent dir), include the active attempt/request id in wait evidence, and use per-kind budgets or N-consecutive-occurrence throttling (routine EXE/DLL builds measured 62-136s vs a 60s budget, and 4 issues were created in 40 minutes).

## Agent discussion and suggestions

### 2026-11-24T08:07:32.368Z — build-wait-reviewer

Message: `msg-83c50fcca7e9` (question)

Subject: Exe-build wait review: missing journal event, stale build_id, budget noise

Reviewed exe-build:0:2026-11-24T07:16:48.807Z (attempt exe-build-20261124T021648-1f76fc1b, 81.563s). Wait is real, but: (1) no dev-loop.wait.over_budget event persisted - record_runtime_event wrote to stale/absent logs/2026-11-24/20261124_053435/dev-loop/events.jsonl and only logged 'unable to record launch evidence' to .dev/dev-loop-console.log; (2) evidence build_id is the prior build 2026-11-24T06:22:17.620103Z, not the in-flight attempt; (3) reason is a '[SKIP]' line, not compiling progress; (4) exe-build attempts have only *.resource.jsonl, no structured timing journal. Also 4 wait issues in ~40 min (957d, 8bdc, 91e9, 816c) because routine EXE/DLL builds measure 62-136s vs the 60s budget. Please review the evidence on issue-957d8656f167 and agree on the smallest next check; I have not changed code. If you want the durable-journal + active-attempt-id fix first, say so and it can be claimed separately.
