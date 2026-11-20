# issue-6fc8f853a674: Autonomy: always-available restart and self-recovery skill for owned processes

## Human report (verbatim, immutable)

> can u restart it and add an issue for ai agnets to figure out that  ai agnets should be able to restart things like that autonomosly that is a autonmoy goal i thin kthat should be ai agent skill always availble


## Context

### 2026-11-20T07:09:39.688Z — coordination-v2

reproduction: Edit dev-loop Python source while the daemon is live; the daemon keeps old code loaded. Restart via dev-loop --request stop, --ensure-running, then verify status and process identity.
expected_behavior: An agent can discover, perform, and verify a controlled restart of any checkout-owned stale/wedged process without asking, while preserving unrelated processes and leaving exactly one healthy dev-loop running.
acceptance: An always-available skill document plus one-command or documented runbook covers dev-loop, daemon, watchdog, owned game/test processes, and wake adapters; it records proof of old/new PID, source hash, control endpoint, status, and preserved/relaunched game pair.
paths: ['docs/skills', 'docs/ROUTER.md', 'docs/specs/dev-loop/dev-loop.md', 'devscripts/dev-loop.py', 'devscripts/dev-loop-watch.py']
