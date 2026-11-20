# issue-89c11130189a: Track agent work-time and phase latency

## Human report (verbatim, immutable)

> add a issue for trackign agent work times like measuring time from received message to got all things done like so we track and see what each miliseconds is to see if antthing can be cut


## Task linked

### 2026-11-20T05:59:42.706Z — work-metrics

Task: work-session-schema (step 0)

## Task linked

### 2026-11-20T05:59:44.606Z — work-metrics

Task: work-phase-cli (step 0)

## Task linked

### 2026-11-20T05:59:46.068Z — work-metrics

Task: work-metrics-board (step 0)

## Task linked

### 2026-11-20T05:59:47.790Z — work-metrics

Task: work-time-tests (step 0)

## Agent suggestion

### 2026-11-20T06:04:04.203Z — root-build-wait-efficiency

Scope agent time metrics to observable wall-clock phases (message/intent receipt, activation/phase boundaries, tool latency, and external/build waits). Keep internal model inference/thinking separate and report it only if the platform exposes a reliable measurement. Add duplicate-aware intake: link/appended evidence to an existing issue and resume its open tasks when the root cause is the same.

## User quote

### 2026-11-20T06:04:05.107Z — root-build-wait-efficiency

Via root-build-wait-efficiency; supports unspecified:

> ai thought times are tracked so we trackt howlong ai is thinkign and mkaing it more effiicent tover time

## Context

### 2026-11-20T06:23:25.242Z — work-metrics

reproduction: Run work start, work phase, and work end for one user request; inspect work status and summary.
expected_behavior: The session records received-to-done elapsed milliseconds and per-phase milliseconds.
acceptance: Unit tests pass and work status/summary show phase totals; board shows active sessions.
paths: ['devscripts/agent_coord.py', 'devscripts/quality/test_agent_coord.py', 'docs/specs/dev-loop/agent-coordination.md']
evidence: {'spec': 'docs/specs/dev-loop/agent-coordination.md', 'tests': 'python -m unittest devscripts.quality.test_agent_coord -v'}

## Context

### 2026-11-20T06:54:56.860Z — work-metrics

evidence: {'session': 'work-43d0ab1571e2', 'total_ms': 2236437, 'phases_ms': {'received': 160851, 'planning': 1020224, 'editing': 758590, 'review': 296772}, 'tests': '35 passed', 'changelog': 'docs/changelog/2026-11-20/20261120_064900-work-time-issue-context-ai-los.md'}

## Context

### 2026-11-20T07:08:16.978Z — work-metrics

acceptance: Begin opens one work session, end closes it, and dev-loop --wait-for-build records build_wait then resumes the prior phase for any agent identity; 37 tests pass.
evidence: {'session': 'work-e79b62dcfb55', 'tests': '37 passed', 'automatic': 'begin/end and dev-loop --wait-for-build', 'changelog': 'docs/changelog/2026-11-20/20261120_064900-work-time-issue-context-ai-los.md'}
