# issue-3e601947fc16: Wait over budget: exe-build at compiling

## Human report (verbatim, immutable)

> exe-build wait at compiling exceeded the wait budget


## Review (confirmed-expected-build-duration)

### 2026-11-23T19:40:34.473Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence exe-build:4:2026-11-23T19:21:01.652Z (issue evidence: kind=exe-build, stage=compiling, elapsed=61.168s, budget=60.0s, build_id=2026-11-23T18:39:16.832027Z, generation=4, source_generation=4, reason='compiling 0/45 [CXX] src\replay\replay-factory-worker.cpp').

FINDING (high confidence): Expected-duration build, not a stall or failure. The wait was a normal EXE build still compiling 45 TUs. It crossed the flat 60s budget by 1.168s while making progress, then published a valid EXE.

EVIDENCE
- Journal (run 20261123_184017, logs/2026-11-23/20261123_184017/dev-loop/events.jsonl): exactly one dev-loop.wait.over_budget (created=true, line 583) and one dev-loop.wait.reviewer-wake (line 582) for this wait_id. No duplicate occurrence. A second over_budget in that run belongs to a different wait (game-dll-build:2026-11-23T19:13:41.970Z, issue-88caca2905d1).
- Owning build succeeded: dev-loop.build.completed at 2026-11-23T19:22:24.620Z, build 2026-11-23T19:22:15.804284Z, source_generation=4, duration=76.41s (setup 0.032s, build_script 71.151s, post_build 5.034s, publish 2.319s), source_changed_during_build=false. .dev/current/mimita.exe mtime 2026-11-23T19:22:15Z. The 60s cross happened ~61s into a 71.15s compile step, ~15s before success.
- Timing artifact .dev/build-timings/exe-build-20261123T142101-1a05e345.resource.json: status=partial, sample_count=0. There is no host CPU/memory sample for this occurrence, so build-pressure/contention is unknown (not measured), not zero.
- Reviewer wake: exactly one request .dev/agent-wake/requests/wake-bb0cc5a8b942.json exists for this issue. Wake queue: queued 2026-11-23T19:22:03.365Z, claimed/launched to codex 19:22:03.9Z, stale-retried to opencode 19:34:23.7Z, launched 19:34:24.4Z (this session). Acceptance 'reviewer wake queued exactly once' holds. The reviewer-wake event's synchronous launch_error (WinError 5 renaming wake-bb564fe4680a.json.tmp) is the known request-store write race already tracked on issue-6907072bb155 and did not lose this wake; the ~12-minute dispatch delay matches issue-84c584957e65.

ROOT CAUSE / POLICY: WAIT_ISSUE_SECONDS is a single flat 60s budget (devscripts/dev-loop.py:114) applied to every wait kind/stage including exe-build. check_wait_budget (devscripts/dev-loop.py:3821-3912) files one issue per wait_id without considering that the same build later succeeds. A 45-TU EXE compile measured 71.15s here, so such builds routinely cross 60s mid-compile and spawn a high-priority issue plus reviewer wake.

ASSESSMENT: The wait is correctly recorded and deduped (one issue, one wake); no build defect is implied. This is expected-duration policy noise, not a regression. No source edits made; no files leased by another agent claimed.

SUGGESTION (separate fix, not applied): make the wait budget kind/stage-aware (for example scale the EXE compile budget to TU count) or defer the issue and only file it when the build is not progressing or does not succeed within a grace window. Requires human approval and its own claimed task. Related: issue-9a4503f6ed68 (automatic over-budget waits), issue-136d4553ff92 (build lifecycle/latency).
