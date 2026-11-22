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

## Scheduled independent review

### 2026-11-22T09:10:12.484Z — cpp-latency-pressure-profile

A bounded independent review was queued as wake wake-860d3bae0baf after 268 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (needs-evidence)

### 2026-11-22T09:22:57.188Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review by issue-review-sweeper (work-721bba8b1690). Read-only; no source edit, build, or close.

OBSERVED FACTS
- Core tooling exists: work_sessions/work_phases tables (devscripts/agent_coord.py:504,527); work_start/phase/wait/resume/end/status/list/summary (devscripts/agent_coord.py:2837-3069); begin opens a session and end closes it (devscripts/agent_coord.py:790-825); board includes active work (devscripts/agent_coord.py:3261,3334,3922); dev-loop build requests record build_wait and resume (devscripts/dev-loop.py:8125-8135, 7781-7810).
- Unit evidence passes today: python -m unittest devscripts.quality.test_agent_coord -v -> Ran 41 tests, OK, including test_work_sessions_measure_received_to_done and test_build_wait_is_distinct_and_auto_resumes. The issue's "37 passed" note is stale, not contradicted.
- Live-data gap: all 27 work_sessions in the coordination DB have exactly one phase, received (27/27); 0/27 carry issue_id; 0/27 carry task_id; 0/27 contain build_wait/test_wait/waiting/blocked. Current review session work-721bba8b1690 is identical (elapsed 556111 ms at check, phase=received, issue_id/task_id empty, received_message_id empty).
- Cause is visible in code: begin creates the automatic session with work_start(agent, notes="activation: ...") (devscripts/agent_coord.py:817). The CLI --task goes to agents.task; it is not persisted as the work task/issue/message. Only manual work start --issue ... populates those columns. Likewise there is no automatic phase change except the dev-loop build_wait path.
- No proof record is linked: agent-coord proof list --issue issue-89c11130189a returns empty.
- Readiness: score 6, ready=false, missing proof_expectation and risk; state was new with 4 open tasks and empty summary.
- The referenced changelog (docs/changelog/2026-11-20/20261120_064900-work-time-issue-context-ai-los.md:31-37) claims sessions record task/issue/message and that board/status expose them. The schema supports it, but the automatic begin path does not write those fields, and the wake prompt I received lists only begin/sync/observe, not work phase.

INFERENCE / UNCERTAINTY
- Received-to-done total wall time is captured for every activation, but per-phase latency is not captured for normal activations unless an agent voluntarily calls work phase or uses dev-loop --wait-for-build. The user goal ("see what each milliseconds is to see if anything can be cut") is therefore only partially realized in live operation.
- I did not issue a build request (review-only), so I could not confirm live build_wait auto-resume; the unit test covers the transition, but no live session shows it.
- I cannot separate "wake prompt omits phase calls" from "agents ignore the spec"; the prompt I received omits them, which is consistent with the former.

CONTRARY EVIDENCE
- The phase mechanism works when used deliberately: work-e79b62dcfb55 has received/planning/editing/review phases and the changelog's 2236437 ms total.
- Board does expose the active session: agent-coord board --agent issue-review-sweeper returned work[0]=work-721bba8b1690, phase=received, live elapsed. The board task is implemented.

NEXT DISCRIMINATING CHECK
One real activation: persist issue/task/message in begin's automatic work_start, record planning/editing/review at phase boundaries, end the session, then verify work status <session> shows more than one phase and work summary --issue issue-89c11130189a includes it. That distinguishes "tooling exists but is unwired" from "agents skip phases" with one bounded run.

## Context

### 2026-11-22T09:23:25.321Z — issue-review-sweeper

risk: Metrics can appear implemented while empty or unlinked: begin currently drops task/issue/message and phase updates are manual-only, so per-issue and per-phase totals may be missing or misattributed; a false done could hide repeated delays.
proof_expectation: Tooling-only change. Required evidence: the agent_coord unit suite passes, and one real activation shows a work session with persisted issue/task/message plus received and at least one additional phase; work summary --issue <id> totals that session. No live game event applies; if no live activation is run, an exemption with that reason is acceptable but the per-phase claim stays unproven.
