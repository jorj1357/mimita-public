# FCS NPC context-wake review (bounded activation)

## Task

- Task ID: `wake-1f9e04a64eae`
- Session ID: `wake-1f9e04a64eae`
- Owner: `fcs-npc-live-fix`
- Focus: review high-priority user context `ctx-afcad8cdbcdb`
- Summary: verified the delivered prompt was truncated, recovered the intended
  context wake, checked routing, acknowledged the context, and recorded one
  wake-delivery recurrence
- Status: complete, no source edits, no build request, no runtime proof claimed
- Date, time, timezone: 2026-11-21T05:00:00Z, ISO 8601
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `d651e133`
- Final commit: uncommitted

## Pre-existing changes

- The worktree was already modified by other agents (config, devscripts, spec,
  issue, source, and changelog files).
- This session did not modify or revert any of them.

## Requested behavior

The delivered first user line was only
`You were woken by MiMITA agent-wake. This is a bounded test activation.`
The real assignment was in `.dev/agent-wake/requests/wake-1f9e04a64eae.json`:
review new high-priority user context `ctx-afcad8cdbcdb`, review its linked
issue, then acknowledge it after routing or repairing ownership.

## Specification alignment

- Read `AGENTS.md`, `docs/ROUTER.md`, and `docs/specs/dev-loop/agent-wake.md`.
- A woken agent may investigate and produce evidence but does not inherit
  build, merge, or policy authority.
- Routing is already correct, so no ownership repair was made.

## Findings

- `ctx-afcad8cdbcdb` is linked `primary` to `issue-91e3384b219b` (Agent
  awareness gaps and continuous issue progress) and `related` to
  `issue-0e981ef03d32` (desktop inspection bridge timeout) and
  `issue-80dfb9ab9c5c` (dev-loop daemon alive, control endpoint unreachable).
- The user's Python-fallback direction is already captured: a supported
  fallback that inspects real MiMITA state without misrepresenting sandbox
  evidence (`issue-0e981ef03d32`), plus control-path recovery that preserves
  the active server (`issue-80dfb9ab9c5c`).
- Acknowledged `ctx-afcad8cdbcdb` for `fcs-npc-live-fix`.
- Wake delivery reproduced `issue-62ab3daa615d` again:
  `MIMITA_WAKE_ID=wake-1f9e04a64eae`, `MIMITA_WAKE_ISSUE` empty,
  `MIMITA_AGENT_ID=fcs-npc-live-fix`; only the hardcoded banner reached the
  model while the request JSON held the full requested work.
- The `.dev/agent-wake/requests` queue currently holds mixed-agent wakes
  (`root`, `build-wait-reviewer`, `rocket-impact-investigation`, and others),
  while `dev-loop.py --status` also showed an unrelated active agent named
  `bounded-wake-test`. This makes the hardcoded "bounded test activation" label
  ambiguous for a real context wake.

## Validation

- `agent-coord sync`, `agent-coord context list --high`, `agent-coord inbox`,
  `dev-loop.py --status`, and wake request-file inspection.
- Coordination: one `issue review` reproduction on `issue-62ab3daa615d`, one
  bounded review on `issue-91e3384b219b`, `context ack ctx-afcad8cdbcdb`, and
  `activity set`.
- Dev-loop status at review time: `lifecycle=game_build_failed
  build=published_stale run=20261121_022840`; no server or client running.

## Not proven

No gameplay, desktop-inspection, or dev-loop control behavior was changed or
proven. The linked issues still need their owners' live evidence.

## Automatic evidence

- `.dev/agent-wake/requests/wake-1f9e04a64eae.json`
- `.dev/agent-wake/logs/2026-11-21T00-23-01.423Z-fcs-npc-live-fix.prompt.txt`
- `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md`
- `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md`
