// 2026-11-20
/* purpose
* record the dev-loop coordination panel, human message path, and tests
* preserve the distinction between reported activity and hidden reasoning
* this file DOES NOT claim a live visual dashboard review
*/

# Dev-loop coordination panel

# Task

- Task ID: dashboard-coordination
- Session ID: session-d51977dda6d9
- Owner: dashboard-coordination
- Issue: issue-48e061680b80
- Focus: show agents, activity, messages, notes, decisions, and issues in the
  dev-loop dashboard; let a human send a coordination message
- Summary: added a bounded coordination snapshot and human send method to the
  brain, a COORDINATION section to the Tk dashboard, and a human-dashboard
  sender that does not register an AI agent.
- Status: SOURCE_AND_UNIT_TESTED — live dashboard visual review not performed
- Date, time, timezone: 2026-11-20T02:17:00+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Final commit: uncommitted shared-checkout edits

# Pre-existing changes

The shared checkout contains extensive unrelated edits. This session touched
only the files below. Active agents `coordination-v2`, `issue-proof`,
`proof-system`, and `wake-context` were observed in the coordination brain;
their active leases were respected. The dashboard work was done after the
previously active leases on `agent_coord.py`, `gui.py`, and the dashboard spec
were released.

# Requested behavior

The human wants to see all reported AI messages and what each AI is working on
in the dev-loop GUI, and wants to send messages to agents from that view.

The panel must show reported activity/messages/issues, not private
chain-of-thought. Rendering must not mark messages read. A missing brain must
not break build/process controls.

# Exact implementation changes

## `devscripts/agent_coord.py`

- Added `coordination_snapshot()`:
  - bounded agents, activity, open issues, tasks, recent messages, notes,
    decisions, active leases, unread count, and focus;
  - computes `sender_kind=human` for `human-dashboard`/`human`;
  - does not mark messages read.
- Added `send_human()`:
  - validates `all`/`any` or a registered recipient;
  - writes through the brain and records `coord.message.sent`;
  - uses sender `human-dashboard`;
  - does not register the dashboard as an AI agent.

## `devscripts/dev_loop/gui.py`

- Added a collapsible `COORDINATION` section to the main dashboard.
- Shows agent status, last-seen age, task, issue, phase, activity, paths,
  build/run, blocker, wait reason, and next step.
- Shows recent messages with sender, recipient, kind, subject/body, timestamp,
  unread marker, and thread.
- Shows open issues and focus.
- Added a recipient selector, subject field, message body, and Send button.
- The dashboard creates one `CoordStore` view instance, closes it on cleanup,
  and keeps the rest of the dashboard usable if coordination is unavailable.
- Rendering does not call `inbox` or any read-marking path.

## Documentation

- `docs/specs/dev-loop/dashboard.md`: added the coordination panel contract,
  bounded snapshot, no-mark-read rule, human send, failure behavior, and
  acceptance.
- `docs/specs/dev-loop/agent-coordination.md`: added the `human-dashboard`
  message contract and delivery semantics.

## Tests

- `devscripts/quality/test_agent_coord.py`:
  - snapshot is bounded and does not mark messages read;
  - human messages are identified as `sender_kind=human`;
  - unknown recipients are rejected.

# Validation

```text
python -m py_compile devscripts/agent_coord.py devscripts/dev_loop/gui.py devscripts/quality/test_agent_coord.py
python -m unittest devscripts.quality.test_agent_coord -q
```

Result: all 23 tests passed.

A live GUI visual review was not performed in this session. The dashboard was
not opened to avoid disturbing the active dev-loop/game processes in the
shared checkout.

# Evidence boundary

- Source implementation: complete.
- Unit tests: passed.
- Live dashboard rendering: not yet observed.
- Human acceptance: not claimed.

# Next exact step

Open the existing dev-loop dashboard and verify:

1. `COORDINATION` appears and shows active/inactive agents.
2. Activity rows show task/issue/phase/paths/build/run/blocker/wait/next step.
3. Recent messages appear.
4. Sending a message to `all` and one agent appends it immediately.
5. The agent's unread state remains for next activation.
6. Collapsing the section and restarting the dashboard preserve the preference.

# How to reverse

- `git checkout -- devscripts/agent_coord.py devscripts/dev_loop/gui.py docs/specs/dev-loop/dashboard.md docs/specs/dev-loop/agent-coordination.md devscripts/quality/test_agent_coord.py`
- Reverse only files touched by this session; preserve pre-existing changes.
