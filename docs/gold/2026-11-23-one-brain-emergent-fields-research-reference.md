# Gold reference: one brain, many hands, emergent structure

- Recorded: 2026-11-23
- Status: reference for the current coordination and measurement direction; research hypotheses below are not established scientific results or shipped engine behavior.
- Audience: MiMITA agents, maintainers, and humans shaping autonomous investigation and graph research.
- Related: [Agent coordination](../specs/dev-loop/agent-coordination.md), [Agent self-improvement](../specs/dev-loop/agent-self-improvement.md), [Measurement and recursive improvement](../workflows/measurement-hypothesis-driven-improvement.md), [Nodes and connections](../specs/architecture/nodes-and-connections.md), [Agent proof and verification](../specs/dev-loop/agent-proof-and-verification.md), [Issue intake and triage](../workflows/issue-intake-and-triage.md).

## Purpose

Preserve the direction behind the recent MiMITA agent-coordination and measurement work: one shared brain, many agents able to investigate and contribute, with a scientific loop that can discover useful next steps the user did not already specify. Connect that direction to the longer-term nodes-and-connections research idea, while keeping observed implementation, design targets, and speculation distinct.

## Does not

- Claim that the engine currently derives physical space, time, quantum fields, atoms, or molecules from a graph.
- Establish that a discrete neighbor-updating model is mathematically equivalent to a quantum field.
- Replace the current ECS, physics, networking, compiler, issue tracker, or coordination brain.
- Grant agents authority to change shared policy or self-approve evidence.
- Treat fluent explanations, a successful build, or a plausible visualization as scientific proof.

## What exists in the repo now

These are repository facts, based on the current authoritative documents:

1. **One shared brain, many named agents.** The coordination specification makes the brain the shared owner of tasks, directives, decisions, messages, leases, and related records. Agents synchronize against that shared state; their private context is only a cache. It calls this model “one shared authoritative brain and many named worker agents.”
2. **Agents can coordinate and hand work across roles.** The same spec supports shared issue evidence, issue-linked questions and messages, edit leases, work phases, bounded observation/context bundles, and a board. Roles can shift between investigation, implementation, evidence review, and process review. The brain remains advisory except for its explicit lease, halt, and freeze controls; it does not grant build authority.
3. **Improvement is expected to be evidence-led and recursive.** The self-improvement spec asks agents to surface coordination gaps, repeated manual work, missing evidence, and ways to reduce time-to-evidence. It requires an observation, a small experiment, a measurable expected effect, risk, rollback, and human-controlled decisions where applicable. Review findings enter the shared issue trail rather than disappearing into chat.
4. **The measurement workflow makes claims falsifiable.** It asks for a goal and hypothesis, matched pinned baseline and after runs, units, sample sizes, exact artifact and journal paths, counterevidence, and a next decision. Negative and inconclusive outcomes remain part of the record.
5. **Nodes and connections are a target architecture, not an engine rewrite.** The architecture spec defines nodes, typed connections, provenance, confidence, version, and read-only projections for lists, boards, 3D, timelines, code, and agent context. Existing system owners remain authoritative. The staged route starts with read-only import and bounded context queries, then projections and experiments.

The corresponding active sources are linked above. This note summarizes their intent; those documents remain authoritative if details change.

## The direction to grow

The user wants AI-assisted research to become more ambitious and generative: agents should notice opportunities the user did not think to request, turn real unanswered problems into precise experiments, investigate them in a graph-based engine, and show what evidence supports each conclusion and what useful next question follows. The long-range intuition is that relationships may be more fundamental than a pre-given space and time, with spatial, temporal, networking, and field-like behavior arising as useful large-scale descriptions of an underlying structure.

Treat that as a valuable research program, not a conclusion. In particular, “millions of discrete nodes look mathematically identical to a continuous quantum field” is a hypothesis to define and test. It needs a precise update rule, observables, comparison model, error bounds, and reproducible evidence before anyone may report equivalence.

## A scientific loop for AI-proposed work

For each proposed discovery or improvement:

1. **Name the question.** State the user-facing goal or scientific question in one sentence. Make clear what is unknown and why it matters.
2. **Separate facts from ideas.** Label claims as observed, inferred, hypothesized, disproved, or unknown. Link the source, code owner, issue, run, and artifact. Never turn an agent's recollection into evidence.
3. **Form competing hypotheses.** Include at least one alternative explanation. State what observation would distinguish them and what result would falsify the leading hypothesis.
4. **Set a baseline and acceptance measure before changing anything.** Choose units, input conditions, sample size, thresholds, and known confounders. Pin the baseline when runtime behavior is involved.
5. **Make the smallest isolated change or model.** Keep experiments reversible and attributable. For graph research, preserve provenance and deterministic inputs; do not silently mutate facts when switching views.
6. **Run the experiment and preserve all outcomes.** Record exact code/config version, command, environment, random seed if applicable, build/run identity, raw measurements, and logs. Include failures and null results.
7. **Compare against the baseline and alternatives.** Report effect size, uncertainty, limitations, and whether the acceptance rule passed. Build success is compilation evidence only; a view is not proof of the underlying model.
8. **Ask for independent challenge.** Another agent should try to reproduce the result, find counterexamples, inspect confounders, or propose a discriminating follow-up. The reviewer does not self-approve the owner's claim.
9. **Decide and update the shared record.** Keep, revise, or reject the idea based on evidence. Append the decision and links to the shared issue/evidence trail, then update the next task. Human review remains final for shared authority and other critical changes.
10. **Look one order farther.** After a result is supported, ask what new observable, failure mode, dependency, or consequence follows from it. For each second-, third-, or later-order proposal, label the inference chain and uncertainty; do not let a chain of plausible steps silently become a fact.

An AI suggestion is useful when it creates a testable, reviewable next move—even if the experiment disproves it. Novelty alone is not success. Track accepted/rejected proposals, replication rate, prediction quality, time-to-evidence, regressions, and avoidable human or agent wait so the process itself can improve without moving its own acceptance thresholds.

## Graph and emergent-structure research path

Use the existing phased nodes-and-connections roadmap as the safe foundation:

1. **Read-only graph of current project knowledge.** Import existing authoritative issues, specs, decisions, code ownership, builds, and evidence with provenance. Compare bounded graph context against today's `observe` results. Do not introduce a competing store.
2. **Projection and consistency experiments.** Render the same selected subgraph as a list, 2D relationship view, timeline, and 3D projection. Verify identity and facts survive view changes; expose hidden or filtered facts.
3. **Small deterministic network sandbox.** Define nodes, typed edges, local update rules, boundary conditions, and measurement outputs. Bound computation and connection storage in each experiment even when the abstract graph permits arbitrary degree. Start with a modest graph whose full state can be inspected and reproduced.
4. **Recover useful spatial and temporal descriptions.** Propose a mapping from graph relationships and event order to distances, directions, coordinates, and clocks. Test whether independent observers or equivalent projections recover consistent measurements under stated transformations. State failure cases and precision. Do not assume the mapping exists.
5. **Networking and gameplay benchmark.** Use a reproducible, bounded scenario such as a TF2-like rocket duel: actors, launch events, projectile paths, collision/contact, latency, and authoritative outcomes. Compare graph-derived predictions with a conventional reference implementation and pinned runtime events. This is an engine benchmark, not evidence about fundamental physics.
6. **Field-like behavior as a separate hypothesis.** If local node states and neighbor updates produce stable large-scale waves, define the continuum quantities and equations being compared. Measure convergence as graph resolution changes, symmetry/locality properties, stability, and deviations from the chosen reference. Only claim an equivalence within measured assumptions and error bounds.
7. **Further domains only after validation.** Atoms, molecules, and other physical systems require their own definitions, scales, constraints, and independent reference data. A visual resemblance or shared graph vocabulary does not demonstrate that those systems have been recovered.
8. **Second- and later-order consequences.** Generate explicit predictions from a validated model, rank them by information gain and test cost, and test the highest-value discriminating prediction. Retain a traceable dependency graph: premise -> transformation -> prediction -> measurement -> decision.

At every stage, the existing systems remain owners until a separately reviewed experiment demonstrates a safe replacement. A graph view or simulator can be exploratory without becoming a production game architecture.

## Make the reasoning auditable

Expose the **evidence trail and decision record**, not private hidden chain-of-thought. For a proposed result, show:

- the original question and goal;
- linked observations, sources, assumptions, and confidence;
- alternative hypotheses and the chosen discriminating test;
- the experiment version, conditions, and measured outputs;
- failed attempts, counterevidence, and limitations;
- which inference connects each result to the next prediction;
- reviewer challenge and final decision;
- the next open question and why it is the next useful step.

This makes the work inspectable and reproducible without presenting unverified internal reasoning as fact.

## Near-term behavior to encourage

- Agents may suggest new work when the suggestion is tied to evidence, a current goal, a concrete owner, and a measurable next check.
- Each active agent should preserve focus on its accepted task: coordination should shorten duplicate work and unblock dependencies, then return attention to the goal it owns.
- A suggestion enters the shared brain once, under the best matching issue or a fully described new improvement/capability issue. Do not create duplicate issue trails or status-only messages.
- Preserve the user's exact report and context. Distinguish user intent from the agent's interpretation and proposal.
- Keep shared build/run authority, proof review, and user decisions intact while improving how quickly agents find evidence and coordinate.

## Acceptance for this direction

The direction is making progress when:

- a cold-starting agent can find the goal, current owner, evidence, blockers, and next discriminating step from shared records;
- useful agent-originated suggestions are linked to evidence and can be accepted, rejected, reproduced, or left uncertain;
- experiments include a predeclared measure, exact inputs, preserved outcomes, and an independent challenge;
- the measured time from goal or edit to evidence falls without increasing regressions or lowering proof thresholds;
- graph projections preserve fact identity and provenance;
- any claim that a model recovers spatial, temporal, field-like, or physical behavior is supported by a defined model, a reference comparison, bounds, and reproducible results;
- proposals that change shared authority or production behavior receive the required human decision.

## Current boundary

The shared coordination and scientific-improvement mechanisms are implemented as project workflows/specifications. The graph-based recovery of 3D space/time, networked gameplay, quantum fields, atoms, molecules, or real-world unsolved problems remains future research. This document records the direction and a way to test it; it does not claim those results have been achieved.
