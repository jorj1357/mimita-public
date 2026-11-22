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

## Shared findings and cross-agent roles

Useful analysis must enter the shared brain, not remain only in a user-facing
answer or an agent's private working context. Before or alongside telling the
user a nontrivial finding, uncertainty, hypothesis, suggested fix, test idea,
or workflow gap, the agent appends it to the relevant issue discussion. If no
issue fits, it creates one using the full issue-intake contract. The append
states whether the claim is observed, inferred, hypothesized, disproved, or
unknown and includes evidence references, timestamp, confidence/limits, and a
next discriminating check when available.

Every agent may assume any task role needed for the current work, including
investigator, implementation owner, evidence auditor, independent reviewer,
or workflow/meta reviewer. These are temporary responsibilities, not a model
or agent allowlist. The task owner coordinates contributions; any agent may
add evidence, challenge a claim, propose alternatives, or review another
agent's method through the shared issue. A handoff must work from `issue brief`
and linked artifacts alone.

At task and phase boundaries, and while waiting when useful, agents review the
workflow around their work for untracked time, missing stage evidence,
unnecessary human steps, repeated manual work, slow handoffs, duplicated
effort, ineffective tests, and safe opportunities to offload work to AI. Add
specific findings and proposals to an existing issue or open a fully described
improvement/build-friction/capability issue. Include the evidence, affected
owner, proposed experiment, expected metric, correctness risk, and human-only
decision that remains. This is a shared meta-review duty any agent can take;
it does not require a permanently running or privileged meta-agent.

At the end of every completed agent activation, make one short process check:
did this work expose a repeated delay, failed-tool dead end, missing evidence,
manual step, avoidable retry, or safe capability that could make development
faster or more reliable? If the evidence supports a useful change, append one
specific suggestion to the best existing issue (usually the relevant issue or
the agent-awareness/self-improvement issue), with the observation, expected
metric, risk, and smallest experiment. If not, do not invent a generic issue;
record that no actionable improvement was found in the work summary. This
check is bounded and must not interrupt the user's requested task.

The general issue-review scheduler performs a deeper rotating review every
five completed work sessions or after 30 minutes when another agent activation
occurs. It cold-reads one stale/unproven open issue, asks an independent agent
to append evidence or counterevidence, and asks for a process improvement only
when the reviewed artifacts support one. Cadence counts completed work
sessions/activations, never hidden model thoughts. If no agent activation or
wake adapter is available, the due review remains visible and pending.

For tests and claims, append both positive and negative live evidence over
time. Name the exact build/revision, run, journal, event or measurement, and
test conditions. Compare before and after under matching conditions, preserve
failed attempts and contrary evidence, and state what result would falsify the
claim. Do not equate a build, synthetic test, or process liveness with runtime
behavior proof.

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

## Autonomy classes

### Class R — reversible

Examples:

- reports, dashboards, diagnostics, tests
- local helper scripts
- documentation fixes that do not change authority or behavior
- evidence tooling
- issue/changelog automation

Agents may propose, prototype, measure, request another agent's review, and
apply a Class R change when evidence is green and rollback is simple. The
change and metric effect are recorded.

### Class C — critical

Examples:

- build authority or the build scheduler
- `AGENTS.md`, specifications, or shared coordination policy
- production or deployment
- packet/wire formats, save formats, or gameplay behavior
- access control, secrets, destructive operations, or process-kill policy

Class C requires human approval, an exact diff, a small safe experiment,
integration proof, and a rollback plan.

A user's explicit standing authorization may satisfy approval only for the exact
scope and limits written in the authoritative project instructions. For
example, `AGENTS.md` grants authenticated agents a bounded deploy permission for
routine mimita.fun website releases. This does not authorize an agent to widen
that scope, change shared authority, or bypass the relevant evidence and
rollback gates. Work outside the recorded scope still requires an explicit
human decision.

Autonomy may increase over time only from measured trust: accepted changes,
rollback rate, failed proofs, human overrides, incidents, and evidence latency.
It may not increase by lowering a metric or self-approving proof.

## Mandatory reflection

After an issue closes, a build waits unexpectedly, proof fails, or agents
conflict, run:

```text
agent-coord retro <issue>
```

The record includes:

- observation and exact evidence
- `change_to_evidence_ms`
- build queue/compile/link/publish time
- runtime startup and proof time
- retries and failed attempts
- duplicate work or conflicts
- missing diagnostics/artifacts
- a proposed improvement, metric target, and smallest safe experiment

Reflection is required for a healthy loop, not optional commentary.

## Improvement scanner

A rotating systems agent or scheduled audit scans the brain and journals for:

- top build waits and slow translation units
- header fan-out
- stale/unowned issues
- failed or stale proof
- duplicate tasks
- idle agents
- repeated manual commands
- missing screenshots/audio/replay artifacts
- unavailable evidence paths
- regressions and repeated build-friction

It opens ranked `improvement`, `capability`, or `build-friction` issues with
evidence and metric targets. It does not edit policy or authority directly.

## Tech-debt and generic-owner review

Every agent that touches networking, combat, contact, damage, build, launch,
test, or coordination must perform a bounded generic-owner review before
editing:

- Is this creating a second owner for contact, damage, health, rewind, duplicate
  suppression, or policy?
- Is there a duplicate packet, handler, formula, event name, cache, or state
  variable?
- Is a value hardcoded that belongs in configuration?
- Is a name weapon-specific where the concept is generic?
- Could this use the shared `ContactClaim`, resolver, damage sink, test, or
  coordination record?

Findings are filed as `tech-debt` or `duplication` issues with exact evidence,
why it is a problem, a suggested fix, risk, rollback, and expected metric/test.
The owner and reviewers add analysis over time; no agent silently refactors a
shared path.

A periodic audit also checks:

- unused source, headers, assets, configs, and generated files;
- files that only consume disk or build time;
- dead code and stale compatibility wrappers;
- duplicate workflows and repeated manual commands;
- avoidable build, launch, test, readiness, and dashboard waits;
- stale docs/specs and misleading names.

Unused claims require reference/runtime evidence and a safe removal proposal.
Do not delete automatically. The audit opens ranked issues; humans/owners decide.

`python devscripts/tech-debt-audit.py --json` is the read-only starting scan
for duplicate special contact paths, direct health writes, weapon-name
branches, stale TODOs, and large hand-maintained files. It reports candidates;
it does not create issues or edit code. Agents add exact evidence and a
suggested fix to the relevant issue before asking for review.

Project knowledge is a bounded graph/subgraph, not an unbounded document dump.
The target graph contract is in
[`../architecture/nodes-and-connections.md`](../architecture/nodes-and-connections.md).

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

## Automatic efficiency and awareness proposals

The dev-loop, a reviewer, or any agent may open an `efficiency`/`performance`
issue from measured evidence:

- `PERFORMANCE_SPIKE`, frames over the 240 FPS budget, or a frame below 240;
- a dominant UI/network/render/ragdoll/animation scope;
- high offscreen work or unnecessary polling;
- avoidable build/test wait or a stale issue queue;
- a dominant work-session phase from `agent-coord work summary`, especially
  repeated `build_wait`, `test_wait`, `review`, or `blocked` time.

The issue must include the exact journal, build/generation, run, scope, frame
counts, work-session phase totals where relevant, expected improvement, risk,
and a pinned-run acceptance.

An agent that lacks information about game state, intent, ownership, or desired
behavior opens an `awareness`/`needs_info` issue with the exact question and its
impact. It does not guess. Awareness gaps go through the same triage, review,
deferral, and user-clarification loop.

Agents may propose improvements for any issue or any part of the repository.
While waiting for a build/test, they review or update these proposals; every
issue should receive periodic progress, evidence, or a reasoned deferral rather
than silently going stale.

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
