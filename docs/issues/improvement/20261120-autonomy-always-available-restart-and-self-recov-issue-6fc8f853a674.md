# issue-6fc8f853a674: Autonomy: always-available restart and self-recovery skill for owned processes

## Human report (verbatim, immutable)

> can u restart it and add an issue for ai agnets to figure out that  ai agnets should be able to restart things like that autonomosly that is a autonmoy goal i thin kthat should be ai agent skill always availble


## Context

### 2026-11-20T07:09:39.688Z — coordination-v2

reproduction: Edit dev-loop Python source while the daemon is live; the daemon keeps old code loaded. Restart via dev-loop --request stop, --ensure-running, then verify status and process identity.
expected_behavior: An agent can discover, perform, and verify a controlled restart of any checkout-owned stale/wedged process without asking, while preserving unrelated processes and leaving exactly one healthy dev-loop running.
acceptance: An always-available skill document plus one-command or documented runbook covers dev-loop, daemon, watchdog, owned game/test processes, and wake adapters; it records proof of old/new PID, source hash, control endpoint, status, and preserved/relaunched game pair.
paths: ['docs/skills', 'docs/ROUTER.md', 'docs/specs/dev-loop/dev-loop.md', 'devscripts/dev-loop.py', 'devscripts/dev-loop-watch.py']

## Context

### 2026-11-20T14:31:53.847Z — context-bus

risk: Restarting an unrelated or healthy process could destroy evidence or user work; authority is limited to checkout-owned processes.
proof_expectation: Recorded old/new PID, combined source hash, live control endpoint, status, and preserved/relaunched game pair.

## Agent suggestion

### 2026-11-21T00:32:47.800Z — dashboard-process-buttons-and-ai-autonomy

Specific user acceptance addition: provide a dashboard-only restart/reopen action that closes and relaunches only the owned GUI process, reconnects to the existing daemon, leaves any active build request and current game/server processes untouched, and verifies the same dev-loop PID/build IDs before and after. Preserve the existing global restart procedure for actual daemon restarts; dashboard refresh alone must not require a restart.

## Agent suggestion

### 2026-11-21T01:16:28.360Z — dashboard-process-buttons-and-ai-autonomy

Update for the current user report: link this issue to user context ctx-4c2102426c95. Add acceptance for dashboard-only close/reopen that preserves daemon PID, pending build request/build owner, and healthy game PIDs, verifies refreshed status on reconnect, and reports the old/new dashboard PID without treating a dashboard restart as a daemon restart. This session has not yet proven behavior during an active build.

## Agent suggestion

### 2026-11-21T01:24:30.306Z — dashboard-process-buttons-and-ai-autonomy

Dashboard restart trial: terminated only verified old GUI PID 28468, then ensured/reopened the dashboard. Daemon 28216, watchdog 23328, run 20261120_232200, server 33940, and client 24476 remained alive; no build was active. The new dashboard PID 36288 ran and updated its lock, but the CLI initially printed pid=0. This suggests dashboard startup/reporting can be temporarily inconsistent; acceptance during an active build remains untested.

## Scheduled independent review

### 2026-11-22T12:34:33.778Z — cpp-latency-digest-cache

A bounded independent review was queued as wake wake-2f88ad4d6436 after 286 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (reviewed - deliverables present, proof/coverage gaps remain, not closeable)

### 2026-11-22T12:45:14.952Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (wake-2f88ad4d6436). Read-only: no source edit, build, close, or fix claim. OBSERVED FACTS - Readiness: ready=true, missing=[], 5 tasks. All 5 tasks are state=open, owner=coordination-v2, evidence empty, latest_proof=null. agent-coord.py proof list --issue issue-6fc8f853a674 returned [] (no proof records for this issue). - Deliverables present in the working tree: docs/skills/dev-loop-restart-v1.md (94 lines); .opencode/skills/dev-loop-restart/SKILL.md (27 lines, always-available); route at docs/ROUTER.md:128; authoritative spec at docs/specs/dev-loop/dev-loop.md:2007 section 19.4a; changelog docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md:71-78 records the original stale-daemon restart and skill creation. - Coverage check of the skill: purpose (lines 5-6) and failure triggers (line 17) name game, test, and wake processes, but the Procedure / 'daemon stale but watcher healthy' / Proof sections (lines 23-94) give commands only for the dev-loop daemon and watchdog. There are no explicit isolated-test-run or wake-adapter stop/restart commands, and no mapping stating they reuse the daemon stop path. - Live dev-loop status at review time: lifecycle=game_build_succeeded, run=20261122_011031, visibility=visible, server pid=26748 alive, client pids=[], game-build generation 490 published (no_op, revision fbfc1ab140c1). This review did not restart anything. INFERENCE / UNCERTAINTY - The three documentation/authority tasks (autonomy-restart-skill, autonomy-restart-route, autonomy-restart-contract) appear substantially delivered in source, but because no task evidence/proof was recorded, the board cannot distinguish delivered from pending. That is a record-keeping gap more than a demonstrated capability gap. - The changelog asserts a successful restart 'verified exactly one watchdog and one daemon with a matching combined source hash' but records no old/new PID, no hash value, no control-endpoint/status line, and no explicit preserved/relaunched game-pair statement, and there is no proof record row. Therefore autonomy-restart-proof acceptance (recorded old/new PID, combined source hash, live control endpoint, status, preserved/relaunched game pair) is unmet in the durable, checkable record. CONTRARY EVIDENCE - The spec, always-available skill, ROUTER route, and changelog all indicate the authority, discovery path, and restart behavior are implemented; the gap is skill coverage of test/wake plus a missing structured proof, not absence of the capability. - Prior suggestion 2026-11-21T01:24:30 did exercise a dashboard-only restart and observed daemon 28216, watchdog 23328, server 33940, client 24476 preserved, but no build was active, so the active-build preservation acceptance remains untested. NEXT DISCRIMINATING CHECK 1. An owner performs one controlled restart via the documented path while a healthy matching pair is running (and, in a second pass, while a build/test run is active). Capture dev-loop --status before and after plus .dev/state.json daemon_pid/daemon_script_hash and watchdog/game PIDs, then proof record linked to autonomy-restart-proof and task-verify. 2. Extend docs/skills/dev-loop-restart-v1.md with explicit isolated-test-run and wake-adapter commands (or state they reuse the daemon stop path and name that command), then record doc-diff evidence for autonomy-restart-skill/autonomy-restart-route/autonomy-restart-contract. VERDICT Deliverables are largely present and the authority is well specified, but the issue is not closeable: no task has recorded evidence, the skill's procedure does not yet cover test/wake as its acceptance requires, and there is no verifiable restart proof record. State moved to in_review; keep open until check (1) and (2) are recorded.
