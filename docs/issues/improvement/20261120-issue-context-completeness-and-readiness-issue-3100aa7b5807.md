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
