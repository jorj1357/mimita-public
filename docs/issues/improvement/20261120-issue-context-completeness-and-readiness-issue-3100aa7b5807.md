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
