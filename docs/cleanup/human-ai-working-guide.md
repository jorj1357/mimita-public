# Human + AI working guide

This document turns the human's lessons from September 2026 into a simple
working system. It is a guide for collaboration, not a replacement for game
specifications.

## The most important rule

The human does not need to know the code owner before asking for help.

The AI must discover the likely owner, explain it in plain language, show the
files and functions involved, and say how confident it is. If the owner is not
clear, the AI must say so instead of quietly adding another owner.

## The human's job

The human decides:

- what the game should feel and look like
- which behavior matters most right now
- whether a result is actually good
- whether a proposed tradeoff is acceptable
- whether old behavior may be changed

The human does not need to memorize every file or function.

## The AI's job

The AI should:

- inspect the repository before changing important behavior
- read the relevant specification
- find existing owners and callers
- explain the current behavior simply
- make a small change
- test or build it when appropriate
- separate what was proven from what was only guessed
- record what changed and what remains uncertain

## A good work loop

1. Pick one visible behavior.
2. Describe the desired result in ordinary language.
3. Ask the AI: “What currently controls this? Explain it like I am five.”
4. Ask the AI to list the exact files and functions involved.
5. Decide whether the current owner is acceptable.
6. Write or update the behavior spec if the desired result is unclear.
7. Change one small working slice.
8. Build or test it.
9. Human checks whether it feels right.
10. Record the result before starting another slice.

## What “one small working slice” means

It means one complete, visible piece—not one random line and not an entire
subsystem.

Example for ragdoll:

> One player dies, becomes a physical body, collides with the world, and then
> respawns correctly.

After that works, add the next slice, such as NPC ragdoll or remote-player
replication. Do not silently build all of ragdoll, networking, editor support,
replays, and every visual effect in the first slice.

## When fast building is appropriate

Fast exploratory building is useful when:

- the desired behavior is easy to recognize
- the change is isolated
- the code can be reverted safely
- the human will immediately test it

Fast building is risky when:

- several systems own the same state
- networking or persistence is involved
- the AI cannot explain the call path
- the change affects many unrelated features
- the result cannot be tested immediately

## The three questions before a risky edit

1. What state is changing?
2. Who currently changes it?
3. What proves that the new owner works and the old path is no longer active?

If the AI cannot answer these, investigate first.

## Simple feature tracker

Every important feature can use this small card:

```text
Feature:
Desired visible result:
Current behavior:
Current owner, if known:
Other writers or possible duplicate owners:
Specification:
Smallest next slice:
How we will test it:
Human result: pass / fail / unsure
AI confidence: high / medium / low
What remains:
```

## Trust rules

AI confidence is not proof. “Build succeeded” only means the code compiled.
“The AI says it works” is not runtime evidence. A feature is trusted only when
the relevant test, log, runtime observation, or human check supports it.

The AI must never hide uncertainty to sound confident.

## Lesson from past work

The successful back-and-forth method should stay. The improvement is to add a
small explanation and proof step around it:

```text
human idea -> AI explains current path -> small edit -> test -> human checks
-> record result -> next slice
```

This keeps the speed and creativity of conversational work while preventing
the repository from quietly growing duplicate systems.

