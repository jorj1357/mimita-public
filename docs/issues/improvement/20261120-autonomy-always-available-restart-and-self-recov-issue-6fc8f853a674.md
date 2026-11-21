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
