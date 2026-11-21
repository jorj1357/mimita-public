# issue-48e061680b80: Dev-loop coordination panel: show agents/messages and send human messages

## Human report (verbatim, immutable)

> can we also make it so i can see the ai thoughts like all the ai messages and what each AI is working on in the dev-loop gui


## Triage

### 2026-11-20T02:16:18.495Z — dashboard-coordination

Summary: Implement bounded coordination snapshot, GUI section, human send; prove snapshot tests and live dashboard render.
Owner: dashboard-coordination
Priority: high

## Task linked

### 2026-11-20T02:16:19.667Z — dashboard-coordination

Task: dashboard-coordination-panel (step 0)

## Review (needs-evidence)

### 2026-11-20T02:19:42.422Z — dashboard-coordination

Reviewer: dashboard-coordination

Implemented the bounded coordination snapshot, COORDINATION Tk section, and human-dashboard send path. All 23 agent-coord unit tests pass. Live dashboard rendering and a human send from the GUI were not observed in this session; those remain the acceptance step.

## Triage

### 2026-11-20T03:33:49.179Z — melee-networking

Summary: Panel implemented and unit-proven; live dashboard visual/send acceptance remains; add visibility keepalive separately.
Owner: dashboard-coordination
Priority: high

## Scheduled independent review

### 2026-11-21T21:24:16.702Z — cpp-subsecond-build-plan

A bounded independent review was queued as wake wake-7475714ac0b3 after 176 completed work sessions (trigger=work_end). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Context

### 2026-11-21T21:27:19.250Z — issue-review-sweeper

reproduction: Open the dev-loop dashboard on the current artifact, expand the COORDINATION section, and confirm it lists active agents, open issues, and recent messages. Then send a message to all from the panel and inspect the resulting message row and coordination journal.
expected_behavior: The dashboard COORDINATION section shows each agent current task/issue/phase/paths, recent AI and human messages with sender/recipient/read receipts, and open issues; a human can send a message to all or a registered agent without consuming any agent unread state.
acceptance: Unit: coordination_snapshot and send_human tests pass and snapshot reads do not mark messages read. Live: on the pinned dashboard artifact the panel renders non-empty agent/issue/message content, a human send appears with sender human-dashboard and no read receipt until an agent reads it, and a coord.message.sent journal entry records it.
paths: ['devscripts/dev_loop/gui.py', 'devscripts/agent_coord.py', 'devscripts/quality/test_agent_coord.py', 'docs/specs/dev-loop/dashboard.md']
risk: Acceptance depends on human visual inspection, so the panel can remain unproven or silently regress without any agent-visible journal signal.
proof_expectation: Live dashboard render and human-send round trip correlated to a dashboard/coordination journal and the current artifact identity; unit tests alone do not satisfy acceptance.

## Review (needs-evidence)

### 2026-11-21T21:27:33.182Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Bounded independent review (issue-review-sweeper, wake-7475714ac0b3). OBSERVED: readiness moved from 2/8 to 8/8 after I added reproduction/expected-behavior/acceptance/paths/risk/proof-expectation from the existing quote, task, and artifact; all prior text preserved. The only evidence record is evidence-e25a07fd276e: proof_kind=unit, result=proven, run_id empty, journals empty, matched_events empty, so it is not live proof. The working tree already contains the panel: agent_coord.py coordination_snapshot :3267 is bounded/read-only and send_human :3339 writes sender human-dashboard with no receipt; gui.py :1430-1608 builds the COORDINATION section, async reader, recipient menu, and Send; test_agent_coord.py :500 and :552 cover snapshot no-mark-read and unknown-recipient rejection. No dashboard.coordination, COORDINATION, or coord.message.sent event exists under logs/2026-11-21; .dev/dashboard-keepalive/events.jsonl last write is 2026-11-20T14:29:05Z (dashboard.start pid 6268). dev-loop --status reports run=RUNNING visibility=visible with client pids=[] and only server-01 journal in run 20261121_144350. INFERENCE/UNCERTAINTY: the data path and widget construction are coherent and unit-tested, but nothing observed proves live rendering or a human GUI send; the send path is only guarded by try/except display. The keepalive journal being about 31h stale means its silence cannot prove the dashboard is absent. CONTRARY EVIDENCE: none that the panel is broken; the implementation review and tests agree, and dev-loop reports a visible running dashboard. The gap is missing live evidence, not a demonstrated failure. NEXT DISCRIMINATING CHECK: with the live dashboard on the current artifact, expand COORDINATION and confirm at least one active agent, one open issue, and one recent message render; send from the panel to all; confirm the row shows human-dashboard with no read receipt until an agent reads it and that coord.message.sent appears in .dev/agent-coord/events.jsonl; attach that journal to task dashboard-coordination-panel. If it fails, capture the first coordination_summary or coordination_send_status error. No source edit, build, fix, or close is claimed.
