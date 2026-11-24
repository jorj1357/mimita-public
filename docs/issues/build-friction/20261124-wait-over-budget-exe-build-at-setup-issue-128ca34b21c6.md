# issue-128ca34b21c6: Wait over budget: exe-build at setup

## Human report (verbatim, immutable)

> exe-build wait at setup exceeded the wait budget


## Review (acceptance-met; dev-loop-journal-proof-gap; stage-desync)

### 2026-11-24T09:50:20.969Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review of wait occurrence exe-build:0:2026-11-24T07:49:35.810Z. No source edits; no files claimed.

ACCEPTANCE (issue-128ca34b21c6):
1. Required fields present. Evidence JSON carries wait_id, kind, stage, reason, elapsed_seconds, budget_seconds, build_id, generation, source_generation, started_utc; issue reproduction/expected/acceptance/paths/proof_expectation are all set; readiness ready=true score=8/8 missing=[]. Caveat: no run_id field — acceptable for an EXE-build wait because no runtime run exists at wait time; build_id is the applicable identity.
2. One issue per wait occurrence. No other issue has evidence.wait_id = exe-build:0:2026-11-24T07:49:35.810Z. The nearby issue-91e93b7e2098 is a different wait (exe-build:8:2026-11-24T07:44:14.544Z), not a duplicate.
3. Reviewer wake queued exactly once. Exactly one wake request, wake-ebca4f95b51c, references this issue (queued 2026-11-24T07:50:37.870Z). No duplicate wake exists for this issue.

DIRECT OBSERVATION — the wait is a real slow build, not a stall, and the recorded stage is stale:
The wait maps to EXE build attempt exe-build-20261124T024935-5bf13f69. Its 1 Hz resource capture starts 2026-11-24T07:49:35.882Z (72 ms after the issue active_build_started_utc 07:49:35.810Z) and completes 07:51:09.617Z: 93.735 s, 77 samples. At the 60 s trigger the build was in phase presentation_dll, not setup: sample 07:50:35.476Z = build.elapsed_seconds 58.343 phase presentation_dll; sample 07:50:36.507Z = 59.346 presentation_dll. The issue records stage=setup / reason="preparing build", which matches only the first ~2 s of the attempt (phase setup 07:49:37.7-07:49:38.8Z). The wait occurred, but the issue stage/reason does not reflect the actual phase at the 60 s trigger.
Artifacts: .dev/build-timings/exe-build-20261124T024935-5bf13f69.resource.json (+ .resource.jsonl).

DIRECT OBSERVATION — host was heavily oversubscribed during the build:
mean_host_cpu_percent=99.99, peak_host_cpu_percent=100.0 across logical_cpu_count=12; minimum available memory 142,413,824 B (~135.8 MiB) of 16,802,918,400 B total; first sample 07:49:37.719Z shows 98% memory used and pagefile 39,175,987,200/45,511,356,416 used. Tracked mimita.exe clients each consumed ~150-164% of one logical CPU (~12.5-13.7% whole host). Capture inconsistency: runtime_alignment status is "unavailable" (fresh_server_and_client_journal_evidence_not_matched), so the exact live mode/workload is not aligned for this capture — a capture gap, not evidence of zero load. The build's own 93.735 s duration is the primary cause of the >60 s wait; resource saturation is a plausible contributing factor (inference, not proven by a controlled comparison).

EVIDENCE GAP — dev-loop journal half of the proof expectation is missing:
Proof expectation requires "Dev-loop wait event plus the issue record and reviewer wake record." The issue record and the single wake record exist, but the exact wait_id appears in no logs/**/dev-loop/events.jsonl; the only retained hit is .dev/agent-wake/requests/wake-ebca4f95b51c.json. This matches known defect issue-6e2d7b91e7d9 (pre-restart journal loss): the wait fired 07:50:35-36Z in the pre-restart window (daemon restarted ~07:48:51); retained run journal 20261124_075114 only records the later 07:53:14 game-dll wait. No dev-loop.wait.over_budget / dev-loop.wait.reviewer-wake row exists for this wait_id. This is an observability gap, not a failure of issue creation or wake uniqueness. Occurrence appended to issue-6e2d7b91e7d9.

OCCURRENCE / DUPLICATE CHECK: wait_id is unique; this is the correct single issue. No occurrence on this issue is needed and no duplicate issue should be created.

DISPATCH NOTE (separate known issue): codex claimed/launched wake-ebca4f95b51c at 07:50:40.208Z/07:50:40.324Z then exited; fallback opencode retried 07:54:24.919Z and claimed/launched 09:20:38.881Z/09:20:38.912Z — about 90 min after queue. This belongs to late reviewer dispatch (issue-84c584957e65) and does not change wake-uniqueness acceptance.

NEXT STEP: Acceptance for this wait occurrence is met except the dev-loop journal proof line, blocked by the existing journal-loss defect (issue-6e2d7b91e7d9, unowned). Build-latency owner (root-mode3-latency) asked to review the stage-desync + resource evidence and agree the smallest next check. No code fix claimed; no source edits.

## Agent discussion and suggestions

### 2026-11-24T09:50:22.366Z — build-wait-reviewer

Message: `msg-fc8d825dc171` (message)

Subject: EXE-build wait stage desync and host pressure evidence

Bounded evidence handoff on EXE build exe-build-20261124T024935-5bf13f69 (wait exe-build:0:2026-11-24T07:49:35.810Z, issue-128ca34b21c6). The wait recorded stage=setup / preparing build, but the 1 Hz resource capture shows phase=presentation_dll at the 60 s trigger (build.elapsed 59.3 s) and success at 93.735 s. Host was oversubscribed: mean CPU 99.99% across 12 logical CPUs, min available 142 MB of 16.8 GB, pagefile 39.2/45.5 GB; runtime_alignment=unavailable. Question: is the stage=setup snapshot consistent with your build-phase reporting path, or does it indicate a stale/duplicated in-memory active_build_phase during the ~07:48:51 daemon restart? Smallest useful next check: compare state.json active_build_phase progression against the resource sampler for one post-restart EXE build. The dev-loop wait event for this wait_id is also missing (known issue-6e2d7b91e7d9; occurrence appended). No source edits made.
