// 2026-11-27
/* purpose
* define how MiMITA specifications are written, read, and treated as authority
* make the desired end state explicit before engineering compromises enter
* separate the spec (the wish) from implementation evidence (what exists)
* this file DOES NOT describe gameplay, build, or runtime behavior itself
* this file DOES NOT let a spec claim runtime proof
*/

# Specification authoring and authority

**Status:** Desired behavior contract.

## The magic-wand rule

A specification is the **magic-wand description of the finished world**.

Write every spec as if time, money, CPU, memory, human patience, and
available engineering are infinite. Describe the world you actually want.
Do not write implementation compromises, workarounds, "for now", "temporary",
or "good enough" into the spec. Those words belong in an issue, not in the
target description.

The contract is one-directional:

```text
spec  = what the finished system must be
code  = the best current attempt to match the spec
issue = every measured gap between them
```

If reality forces a compromise, the spec is **not** edited to match the
compromise. The gap is recorded as an issue with evidence, and the spec keeps
describing the target. A human may change the target; an implementation may
not.

## What a spec is

A spec owns:

- the desired behavior;
- the invariants that must never be violated;
- the vocabulary and ownership boundaries;
- the observable events and commands;
- the failure behavior;
- acceptance criteria written as observable facts;
- non-goals that prevent accidental scope.

A spec does not own:

- the current implementation state;
- build or test evidence;
- runtime proof;
- human acceptance;
- schedules, effort estimates, or staffing;
- temporary mitigations.

Those live in changelogs, issues, proof records, and release notes.

## Required structure

Every spec starts with a purpose comment block:

```cpp
// YYYY-MM-DD
/* purpose
* <what this spec decides>
* <why the decision exists>
* this file DOES NOT <scope guard>
* this file DOES NOT <scope guard>
*/
```

Then:

1. **Status** — desired behavior contract, target architecture, or research.
2. **Audience and authority** — who must follow it and which owners it defers to.
3. **Model** — the nouns, verbs, and ownership boundaries.
4. **Invariants** — normative MUST / MUST NOT statements.
5. **Behavior** — the normal path, including ordering and failure behavior.
6. **Events and commands** — the observable surface.
7. **Performance and resource rules** — budgets, not hopes.
8. **Migration** — how the current world moves toward the target without a
   big-bang rewrite, when migration is needed.
9. **Acceptance** — falsifiable statements with events, paths, or measurements.
10. **Non-goals** — what this spec deliberately does not decide.

## Normative language

- **MUST / MUST NOT** — invariant. A violation is a defect.
- **SHOULD / SHOULD NOT** — strong default. A deviation needs a recorded reason.
- **MAY** — allowed but not required.

Avoid "should probably", "ideally maybe", and "we could". A spec that cannot
say MUST is not yet a spec.

## Specs are not implementation notes

A spec may name an owner file or function when ownership itself is the point.
It must not freeze incidental implementation details such as a local variable
name, a temporary data layout, or a specific workaround.

Good:

> Contact geometry has one owner: `ActorHitVolume`, built from the fixed-tick
> pose. Hitscan, projectiles, melee, splash, and claim validation MUST use it.

Bad:

> Add a `std::vector<Box>` to `server-contact.cpp` and loop over it in
> `handleContact`.

The second belongs in an implementation plan, not the target spec.

## Evidence classes

Never collapse these into one claim:

```text
specified      the desired behavior is written here
implemented    code exists and compiles
runtime-proven a pinned executable emitted the required events in its journal
human-accepted a human observed the behavior and said it is right
```

A spec is always "specified". It is never "runtime-proven" by itself.

## Gap recording

When engineering cannot yet match the spec:

1. Keep the spec unchanged.
2. Open or append to an issue with the exact gap.
3. Record the measured evidence and the smallest next step.
4. Link the issue from the spec's migration or acceptance section when the gap
   is long-lived.

This prevents the common failure where a compromise silently becomes the
target because it was the last thing written down.

## Acceptance quality

Acceptance statements must be observable by a reader who was not present:

- a named event in a pinned run journal;
- a measured number with units, sample size, and run/build identity;
- a file, command, or packet that exists and behaves as stated;
- a human-visible behavior with the exact scenario.

"Works", "looks good", "faster", and "should be fine" are not acceptance.

## Review checklist

- [ ] Purpose block exists and names at least two non-goals.
- [ ] Status and authority are explicit.
- [ ] Every invariant uses MUST / MUST NOT.
- [ ] One owner per concept; no duplicate source of truth.
- [ ] Events and commands are named and versioned where needed.
- [ ] Failure behavior is specified, not implied.
- [ ] Performance rules are budgets with units.
- [ ] Acceptance is falsifiable.
- [ ] Non-goals prevent scope drift.
- [ ] No implementation compromise is written as the target.

## Acceptance

- Every new spec follows the required structure.
- A reader can tell specified, implemented, runtime-proven, and human-accepted
  apart without asking the author.
- A long-lived gap appears as an issue, and the spec still describes the target.
