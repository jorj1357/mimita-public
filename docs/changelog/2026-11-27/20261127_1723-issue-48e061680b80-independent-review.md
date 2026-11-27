# issue-48e061680b80 independent review (2026-11-27)

Read-only review, no source, build, test-run, close, or fix.

## What changed
- Appended one independent review to `issue-48e061680b80` (verdict
  `needs-evidence`, review id 553).
- Appended one process proposal to `issue-91e3384b219b` (make the coordination
  panel's live state agent-observable; document the coordination journal path).

## Evidence used
- `.dev/agent-coord/brain.db` `messages` row `msg-838aa615087a`:
  sender `human-dashboard`, body `hi write "hi this works" if u can see this`,
  created `2026-11-20T03:32:40.424Z`, `read_at 2026-11-20T03:34:10.463Z`.
- `.dev/agent-coord/events.jsonl` seq 253: `coord.message.sent`, `"human":true`,
  `2026-11-20T03:32:40.424Z`.
- Live dashboard PID 24360 (`pythonw ...\dev-loop.py --dashboard
  --no-terminal-relaunch`, started `2026-11-27T01:14:09Z`).
- `.dev/dashboard-ui.json`: `collapsed.coordination=true` (mtime
  `2026-11-27T14:13:36Z`).
- `devscripts/agent_coord.py:3909` (`coordination_snapshot`), `:3981`
  (`send_human`); `devscripts/dev_loop/gui.py:1587`, `:1743-1770`;
  `devscripts/quality/test_agent_coord.py:500`, `:552`.
- `devscripts/dev-loop.py --status`: `lifecycle=launch_failed`,
  `run=NOT RUNNING`, generation 805, `visibility=visible`.

## Finding
The human-send half of acceptance is journal-observed (send + 90s read receipt)
and was missed by the 2026-11-21 review, which searched `logs/` only. The
visual render half is still unproven, and the 11-20 send event's pid (2132) cannot
be tied to a dashboard process, so GUI origin remains inferred.

## Residuals
- The live COORDINATION section is collapsed in `dashboard-ui.json`; a human
  must expand it and send once on the current generation to close the gap.
- Process proposal on `issue-91e3384b219b` recommends a rate-limited
  `dashboard.coordination.rendered` event with row counts so live render is
  agent-observable.
