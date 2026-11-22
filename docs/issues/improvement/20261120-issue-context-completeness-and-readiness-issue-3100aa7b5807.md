# issue-3100aa7b5807: Issue context completeness and readiness

## Human report (verbatim, immutable)

> can u do these as much as possible like as a test to ensure that each issue has all releveant info for it


## Task linked

### 2026-11-20T05:59:49.390Z — work-metrics

Task: issue-context-schema (step 0)

## Task linked

### 2026-11-20T05:59:51.411Z — work-metrics

Task: issue-readiness-cli (step 0)

## Task linked

### 2026-11-20T05:59:52.658Z — work-metrics

Task: issue-backfill (step 0)

## Task linked

### 2026-11-20T05:59:53.878Z — work-metrics

Task: issue-context-tests (step 0)

## Context

### 2026-11-20T06:23:57.238Z — work-metrics

reproduction: Run issue needs-info against the open issue list; observe missing reproduction, expected behavior, acceptance, paths, or evidence.
expected_behavior: Every active issue exposes complete context and is ready or explicitly needs_info with named missing fields.
acceptance: issue readiness returns ready=true for backfilled active issues; incomplete issues list exact missing fields.
paths: ['devscripts/agent_coord.py', 'devscripts/quality/test_agent_coord.py', 'docs/specs/dev-loop/agent-coordination.md', 'docs/workflows/issue-intake-and-triage.md']
evidence: {'spec': 'docs/workflows/issue-intake-and-triage.md', 'command': 'agent-coord.py issue needs-info'}

## Context

### 2026-11-20T14:47:27.209Z — context-bus

reproduction: Run gent-coord issue lint over the live brain and review every returned issue.
expected_behavior: Every new issue is rejected until all required fields exist; legacy issues are either completed from evidence or explicitly marked needs-info.
acceptance: Issue creation rejects each missing field; lint reports zero incomplete open issues after backfill.
paths: ['devscripts/agent_coord.py', 'devscripts/agent-issue.py', 'docs/specs/dev-loop/agent-coordination.md', 'docs/workflows/issue-intake-and-triage.md']
evidence: {'implementation': 'strict issue_add rejects missing quote, summary, reproduction, expected behavior, acceptance, paths, evidence, proof expectation, risk'}
risk: Automated placeholder text would create false readiness; require real context or explicit needs-info.
proof_expectation: Unit rejection tests plus live issue lint output before/after backfill.

## Agent suggestion

### 2026-11-22T01:26:42.522Z — issue-review-sweeper

Process proposal (issue-review-sweeper, wake-67b1c7c3ad91, 2026-11-22T01:26Z): this sweep found one concrete missing-evidence completion. Session coordination-region-leases marked task disjoint-file-patch-handoff implementation complete (`docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md:19`) and committed it in `ce0009d5`, yet issue-c55b45bd457f still has readiness 1/8, empty task evidence and method, no proof row, and zero `coord.proposal.*`/`coord.shared.*` events in `.dev/agent-coord/events.jsonl` (despite 507 lease acquisitions). The code exists, but the acceptance-required real workflow was never recorded. Whether this is a repeated pattern is not yet measured.

Measurable target: `agent-coord issue lint` (or the board) reports zero issue-linked tasks in state complete/done that have empty task evidence and no proof record. Start by making this issue's case visible. Smallest experiment: add one read-only lint query over tasks joined to proof rows, run it on the live brain, and confirm it flags issue-c55b45bd457f / disjoint-file-patch-handoff while proven tasks are not flagged. No source or build change; if the count is nonzero, decide separately whether session end or task completion should warn or block.

## Agent suggestion

### 2026-11-22T01:29:02.219Z — issue-review-sweeper

Clarification to the process proposal appended at 2026-11-22T01:26Z (issue-review-sweeper, wake-67b1c7c3ad91): the observable is a session/changelog claim of completion, not a task in state complete. In this case the linked task `disjoint-file-patch-handoff` is still `open` even though the owner session changelog says status complete; a query filtered on task state alone would miss it. Use this measurable target instead: `agent-coord issue lint` reports zero issue-linked tasks that have empty evidence, no proof row, and an owner session that has ended, starting with issue-c55b45bd457f. Smallest experiment: a read-only query over tasks, proof rows, and ended work sessions that flags this issue/task while leaving tasks with proven or exempt proof unflagged. No source or build change.

## Agent suggestion

### 2026-11-22T02:21:44.954Z — issue-review-sweeper

Process proposal (issue-review-sweeper, wake-28e67b8e79f8, 2026-11-22T02:2xZ): this sweep found a human-only completion gate with no durable request/decision artifact. Issue-93bd29a36d24 (architecture, spec-only) already contains the requested target contract at docs/specs/networking/contact-authority.md:76-160 and no src/ contact path changed, yet the issue is ~46h old, readiness 1/8, state new until this review, task contact-spec open with empty evidence, and no recorded human pre-review. The review scheduler selected it at readiness 1/8. This overlaps the existing proposal for complete tasks with empty evidence but adds a distinct gate: spec-only acceptance names human pre-review with no recorded request timestamp, owner, or decision.

Measurable target: 100% of spec-only tasks whose acceptance names a human pre-review have a recorded request timestamp and an explicit decision (approve/edit/defer) before a second review sweep; the board shows the count of such tasks with no decision. Secondary: 0 scheduled reviews of issues with readiness score < 4 unless the issue is explicitly flagged review-only.

Smallest experiment: add one read-only lint over issue-linked tasks whose acceptance/summary contains human pre-review or spec only, joined to reviews/quotes/clarifications; run it on the live brain and confirm it flags issue-93bd29a36d24/contact-spec while approved tasks are not flagged. No source or build change.
