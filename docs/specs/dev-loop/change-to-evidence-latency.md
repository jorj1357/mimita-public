// 2026-11-20
/* purpose
* define the measured time from an idea or edit to runtime proof in events.jsonl
* make build, queue, activation, and test latency first-class improvement issues
* keep measurement honest: zero is the direction, not a completion claim
* this file DOES NOT define gameplay behavior or build authority
* this file DOES NOT make a build result proof that a feature works
*/

# Change-to-evidence latency

## Audience and authority

This specification is for the root dev-loop, its EXE and candidate game-DLL
builds, isolated agent test runs, the shared coordination brain, and any agent
trying to prove a change.

It supplements [build-time observability](build-time-observability.md),
[agent coordination](agent-coordination.md), and the build authority in
[agent lanes and build authority](agent-lanes-build-authority.md). The root
dev-loop remains the only builder. This document does not grant direct compiler
access.

## Definition

`change_to_evidence_ms` is measured from the moment a change intent becomes
concrete to the moment the expected runtime event is observed in the exact
pinned executable's `events.jsonl`.

Record these monotonic stage boundaries:

1. `intent_utc` — user request or agent idea/decision.
2. `edit_saved_utc` — the source or config change is saved.
3. `build_requested_utc` — an agent or the auto-watcher requests a build.
4. `queue_wait_ms` — time waiting for a bounded build slot or source coherence.
5. `worker_started_utc`.
6. `built_utc` — compile/link finished.
7. `published_utc` — immutable artifact or candidate generation published or
   ready.
8. `activated_utc` — the pinned runtime activated that exact build.
9. `evidence_utc` — the required event and field predicate were observed.
10. `stopped_utc` — the run reached its decisive result and was stopped.

Report both `intent_to_evidence_ms` and `edit_to_evidence_ms`. Build time is one
stage, not the whole metric.

For agent work-time measurement, record observable wall-clock boundaries for
message/intent receipt, activation and task phases, tool calls, and completion.
Report tool, build/test wait, and other external wait intervals separately so
they are not attributed to agent work. Do not label elapsed activation time as
model thinking time: internal inference duration is recorded only when the
platform exposes a reliable measurement. Use the phase data to find and remove
avoidable work over time.

## Change classes

Measure classes separately so an unavoidable cold link is not hidden inside a
fast hot-edit average:

| Class | Examples |
|---|---|
| `docs` | documentation-only edits; evidence is a doc/diff check, not a gameplay event |
| `config` | hot-reloadable JSON/config changes; evidence is the owning config/runtime event |
| `hot_source` | one game-owned translation unit |
| `hot_header` | header change with fan-out |
| `candidate_dll` | explicit per-agent candidate game-DLL build |
| `cold_exe` | resident/bootstrap/launcher/ABI change requiring an EXE |
| `warm_scenario` | scenario run against an already-warm pinned runtime |

Store the class, agent, request ID, build ID, game generation, run ID, and
journal path on every sample. A sample without a matching journal can record
build latency but cannot prove runtime latency.

## Budgets and outliers

- Report rolling median and p95 per class, per agent, and per build stage.
- Use measured per-class budgets. Start from the existing no-change objective
  (at most one second from worker start to a validated no-op result) and tighten
  budgets only after baselines exist.
- There is no hard zero-second requirement. The objective is to drive the
  measured value toward zero without making a false claim about compile, link,
  storage, or process startup.
- A value above the current class budget or a large negative trend is an
  outlier and creates a `build-friction` issue.
- A human may set or approve a budget change. Agents may propose changes; they
  do not lower a budget to make a result pass.

## Build-friction issues

Every build and test request records queue wait. The dev-loop and agent test
tooling must surface:

- why the request waited;
- which slot, source coherence, memory guard, disk guard, or process retained it;
- build stage durations and the dominant stage;
- selected artifact/generation and whether the source changed during the build;
- the expected class budget and the observed value;
- the smallest proposed change.

Rules:

- Auto-create a `build-friction` issue when a change requires an EXE build,
  when a classification is `COLD`/`COLD_ABI`/`HEADER_REVIEW`, when a build's
  `build_cause` is `header_fanout`/`rebuild`/`identity`, or when a measured
  stage exceeds its class budget.
- Record every wait, including waits below budget. Auto-create an issue for an
  avoidable wait or a wait above budget. An agent may also create an issue for
  any wait it considers unnecessary.
- When an agent estimates or observes that a build/test wait will exceed 60
  seconds, treat it as an immediate investigation trigger: record the elapsed
  wait or estimate, current phase, blocker, and evidence; create or link a
  `build-friction` issue; and inspect why the queue/worker is waiting rather
  than only promising to wait. Continue independent review while the wait is
  investigated.
- Before creating an issue or starting work from a repeated user report, search
  open issues for the same request, build/run, or root cause. If one already
  exists, append the new quote/evidence and continue its existing open tasks;
  do not create a duplicate issue or repeat completed work. Create a separate
  issue only for a distinct cause or independently actionable work.
- Auto-created issues begin in `needs_triage`, not `done`. They contain the
  exact build/run IDs and timing references.
- `python devscripts/build_friction.py --create --agent <name>` performs the
  initial detection and duplicate check. The root dev-loop or an agent test
  should invoke it with the requesting agent identity after a build/test result;
  until that hook is wired, agents run it when they observe friction.
- Any agent may triage, reproduce, review, or propose a solution. A human makes
  the final decision on a resolved build-friction issue.
- The issue doc is append-only; the original observation is preserved.

## Build and run shape

- One EXE build at a time. Agent requests are accepted and queued.
- Candidate game-DLL builds may be requested as often as needed, bounded by
  available CPU and memory. They must not overwrite the normal published
  `build/game/current.json` or `ready.json`.
- Each test run pins one EXE artifact and one game-DLL generation/path. A pinned
  run does not need to wait for another agent's build to finish.
- One checkout and one source tree are current policy. Per-agent source
  isolation is deferred and must be specified separately before worktrees or
  source overlays are introduced.
- A warm per-agent runtime is the preferred fast path: start once, keep the
  mode active, activate candidate DLLs at the hot-reload safe point, then run
  bounded event assertions. A fresh isolated run remains available when process
  isolation is required.

## Evidence classes

A change is complete only with the evidence its class can produce:

| Change | Required evidence |
|---|---|
| Gameplay/network behavior | pinned runtime journal proving the expected event, fields, and absence of forbidden behavior |
| Config/hot-reload behavior | config load/apply event and the owning runtime event |
| Build/test tooling | tooling test plus a real command/run showing the new path |
| Documentation only | doc/diff check and links; no gameplay event is fabricated |

Build success is never runtime proof. A timeout is a failure reason, not
evidence of success.

## Improvement loop

Every build-friction issue is reviewed by at least one agent other than its
owner where practical. The owner may run isolated probes and tests to gather
evidence for or against a proposed fix. The final solution is presented to a
human reviewer. Approved work becomes tasks, not automatic policy changes.

See [agent self-improvement](agent-self-improvement.md).

## Acceptance

- A hot change records edit, build, activation, and event stages with exact
  identities.
- A no-op build records its queue and worker stages and does not create a new
  generation.
- A queue wait has a reason and is visible in the activity board.
- An over-budget or EXE-required change creates a triageable build-friction
  issue.
- A pinned candidate test can run while another EXE or candidate build is
  pending without using a stale unpinned executable.
- The spec contains a measured budget and outlier rule, not a zero-second
  promise.
