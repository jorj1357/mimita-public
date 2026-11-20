// 2026-11-20
/* purpose
* let any AI agent surface friction, capability, and coordination improvements
* preserve human authority over policy, build authority, and behavior changes
* make self-improvement model-agnostic and evidence-driven
* this file DOES NOT grant direct compiler, production, or policy authority
* this file DOES NOT replace human review
*/

# Agent self-improvement

## Audience and authority

This specification is for every agent using the shared coordination brain, the
root dev-loop, issue records, and runtime validation. It is model-agnostic:
roles are task-based, agent names are stable labels, and no proposal depends on
which AI model produced it.

Agents may propose improvements at any time. They may not silently change
build authority, the specification, production deployment, or another agent's
work.

## When to propose

Create an issue with `kind=improvement` or `kind=capability` when an agent
observes:

- a build or test wait, especially an unexpected or avoidable one;
- a slow DLL build, EXE-required change, or large header fan-out;
- a coordination gap, duplicate work, unclear ownership, or missing handoff;
- an observability gap that makes `events.jsonl` proof harder;
- a repeated manual step that could be automated safely;
- a capability the agent believes it can provide, especially one a human may
  assume it cannot do;
- a way to reduce `change_to_evidence_ms`.

An agent must not wait for the user to ask for a self-improvement proposal.

## Required proposal content

Every proposal records:

- the observation and exact evidence;
- the current behavior or limitation;
- the human assumption being challenged, if any;
- the proposed capability or change;
- the reason it is safe and the boundaries that remain human-controlled;
- the expected metric effect (build time, wait time, evidence latency, or
  coordination quality);
- risks, failure modes, rollback, and the smallest safe experiment;
- owner, reviewers, dependencies, and acceptance evidence;
- whether it changes policy, build authority, specs, production, or only
  tooling/tests.

## Review loop

1. Any agent opens the issue with `agent-coord issue add --kind improvement`
   or `--kind capability`; the human report or observation is preserved
   verbatim.
2. Another agent triages it into tasks and steps, using `task add`, `task dep`,
   and `issue link-task`. Assign one owner and at least one reviewer where
   practical.
3. Agents may reproduce the problem, run isolated probes, and collect evidence
   for and against the proposal. They may challenge the premise.
4. Reviewers record `issue review` verdicts. The owner synthesizes a solution
   and presents the evidence and tradeoffs to a human.
5. The human approves, rejects, or defers. Approved work becomes normal tasks
   with acceptance evidence and metric targets.
6. The result is measured. If it does not improve the target, record that and
   leave the issue open or close it with the evidence.
7. `issue close` requires a human-review verdict for proposals that change
   policy, build authority, specs, production, or shared coordination.

Agents may review and converge without a human. A human remains final for
changes to the system the agents use to coordinate themselves.

## Capability proposals

The agent should state what it can do more powerfully than a human may assume,
for example:

- coordinate many isolated tests in parallel;
- maintain a live board of every agent's phase and blockers;
- scan documentation for TODO markers and surface clarification questions;
- run candidate builds and pin each result to a separate journal;
- compare evidence across agents and detect contradictions;
- propose its own tooling improvements after observing friction.

A capability proposal is not a claim of authority. It must state the safe
boundary, evidence, and human approval needed.

## Board and issue docs

- Open proposals appear on `agent-coord board` with owner, state, reviewers, and
  blockers.
- Detail files live under `docs/issues/` and are append-only. The human quote
  and original observation are never rewritten.
- Interpretations, evidence, reviews, deferrals, and resolutions are appended.
- Existing specifications and TODO text are not edited to resolve a proposal
  without user clarification.

## Relationship to build-friction

A build-friction issue is a self-improvement proposal about the development
system. It uses the same issue, task, review, deferral, and human-review loop.
The target is to reduce queue waits, DLL build time, EXE requirements, and
`change_to_evidence_ms` without weakening correctness or evidence.

## Boundaries

- No direct compiler, `build.py`, production, or deployment access.
- No silent change to `AGENTS.md`, build authority, pixel/game behavior, or
  packet formats.
- No lowering of a metric budget to make a build pass.
- No rewriting human quotes, prior interpretations, or append-only evidence.
- No model-specific assumptions or role assignment.

## Acceptance

- Any agent can open, triage, review, defer, and close a proposal.
- A proposal can include evidence from isolated runs and other agents' reviews.
- The board shows proposed, under-review, deferred, approved, and rejected
  improvements.
- A human can see the proposed solution, evidence for and against, tradeoffs,
  and the exact metric target.
- No proposal changes authority or production without human approval.
