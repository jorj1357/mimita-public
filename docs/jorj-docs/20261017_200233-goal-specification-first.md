# Goal: specification-first implementation handoffs

- Date: 2026-10-17
- Status: Direction to adopt; workflow still needs to be practiced and improved.
- Audience: Jorj, AI agents, and human contributors who discuss or implement
  MiMITA features.

## Purpose

This document records a goal for how MiMITA work should move from an idea to an
implementation. It does not select the next game feature, claim that the
current Melee implementation is complete, or replace feature specifications.

The goal is to discuss a desired behavior with an AI, improve the relevant
specification and related documents together, and make the result clear enough
that a different contributor can implement it without inventing important
requirements. AI can help identify missing networking, simulation, data, and
testing details, including details the project owner may not know yet. The AI
must explain those choices plainly and mark what is confirmed, provisional,
reused from existing systems, or still needs a decision. Technical detail being
added to a document is not by itself approval of a hidden product decision.

## What this goal does not do

- It does not choose or reorder the game's feature priorities.
- It does not make an unfinished specification, scenario runner, or feature
  implementation complete by documenting it.
- It does not allow an AI to turn a behavior-affecting guess into an approved
  requirement without making it visible for review.

## Why this is the direction

The previous pattern was to put a large amount of intent into one plan, begin
implementation, and repeatedly correct the result because important behavior
or architecture had not been made clear first. The preferred pattern is to
spend more effort in specification dialogue before handing work to an
implementer. The owner can ask what a proposed design means in simple terms,
correct it, and decide whether it matches the intended experience before code
is written.

Using GPT-6 Luna at Medium and discussing the work with another model through
OpenCode are examples of the current process, not requirements that future work
use those particular models or tools.

## Desired workflow

1. **State the experience.** Capture what the player or contributor should see
   and be able to do, with examples where helpful.
2. **Find the existing owners.** Read the routed specifications,
   architecture, workflows, and scenarios. Extend documents that already own
   the behavior instead of creating a competing source of truth.
3. **Trace missing technical pieces.** Have the AI identify what existing
   systems can be reused and what support is genuinely absent. Explain network
   authority, tick timing, configuration, assets, runtime loading, and other
   consequential details in plain language.
4. **Resolve or label decisions.** Ask the owner about choices that materially
   change the experience. Label clearly tunable first-pass values as
   provisional; do not present guesses as user-approved behavior.
5. **Make the spec implementable.** Describe the data/configuration shape,
   behavior owner, state transitions, failure cases, and boundaries between
   client and server when they apply. Link related specs and keep each rule in
   its appropriate owner document.
6. **Define proof before handoff.** Specify repeatable scenarios, the existing
   command or automation entrypoint, expected visible behavior, and the
   `events.jsonl` evidence needed to distinguish success from the first
   failure. Include screenshots only when visual inspection materially helps.
7. **Explain and review.** Ask the AI to explain the proposed behavior as if
   to a beginner and ask whether a contributor could implement it without
   guessing. The owner checks that explanation and any unresolved choices
   before treating the handoff as ready.
8. **Implement and prove separately.** The implementation agent follows the
   reviewed docs and scenarios. Report source changes, build results, live
   runtime evidence, and human acceptance as separate kinds of proof.

## Handoff readiness

A task is ready to hand to an implementer when:

- the desired player-visible outcome and relevant edge cases are stated;
- the authoritative documents and existing code/system owners are linked;
- confirmed decisions are separated from provisional values and open
  questions;
- the data/configuration contract and runtime behavior are concrete enough to
  implement, including networking authority where relevant;
- scenarios describe setup, actions, expected results, and useful journal
  evidence, and identify how an agent runs them;
- the owner can understand the plain-language explanation and says it matches
  the intended experience.

If a required command, scenario runner, or logging field does not exist yet,
record it as prerequisite implementation work. Do not imply that a scenario is
automated or that a feature is proven merely because its desired test is
written down.

## Applying this goal to current work

The Melee work is a current example of specification-first discussion: the
mode, Captain Falcon moveset, animation phases, movement, shield/grab behavior,
and network outcome have been discussed across their owning documents. That
does not mean every value is final or that the implementation is complete.
Its implementation handoff should identify remaining provisional tuning,
missing runtime support, and runnable proof scenarios explicitly.

Use this process for Zombie Tower and future work too. This goal does not
reorder the roadmap or displace the owner's current feature priorities.

## How to tell whether the workflow is improving

- Before implementation, the owner can explain what the feature will do and
  why its major technical choices are there.
- A new implementer can find the behavior in the right documents and does not
  have to silently invent missing product requirements.
- When an agent must choose a technical detail, it records the choice and its
  reason, and marks any behavior-affecting assumption for review.
- Scenarios and logs make failures diagnosable without relying only on a
  person's memory of what happened during play.
- Feedback is about intentional tuning or newly discovered evidence, rather
  than repeatedly correcting behavior that the spec never described.

This is a process goal, not a claim that every current specification, scenario,
or developer command already meets these criteria.
