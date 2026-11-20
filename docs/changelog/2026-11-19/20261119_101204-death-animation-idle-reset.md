# Death Animation Idle Reset

## Task

- Summary: Specify and implement an animation reset to idle while an actor is dead.
- Status: source and specification updated; runtime acceptance remains open.
- Date/time: 2026-11-19 10:12:04 EST (America/New_York).
- Branch/base: `checkpoint/one-checkout-condensation-20261111` at `214849f0`.

## Pre-existing changes

The shared checkout contained unrelated modified files and untracked handoffs/changelogs before this task. They were preserved and not edited. This task changed only the animation specification, the player animation owner, and this changelog.

## Specification review

- Routed specification: `docs/specs/animations/animations.md`.
- Focused review: `docs/skills/spec-behavior-review-v1.md`.
- Finding: the state-machine spec listed death as a trigger but did not define what happens to a weapon/action clip when death interrupts it. The user explicitly chose to reset to idle and clear all other animations.
- Added requirement: on death, clear active one-shots/actions and transient animation state, select idle/default for the rest of that life, and resume normal selection on respawn. Gameplay weapon runtime and inventory remain unaffected.

## Changes

- `docs/specs/animations/animations.md`: added the death-reset contract.
- `src/entities/player-animation.cpp`: clears locked/action poses and transient animation timers while dead; forces the idle/default clip; suppresses equipped weapon, reload, dash, and freeze overlays so they cannot restore an interrupted pose.
- `src/entities/player-animation.cpp::Player::clearOneShot`: reused the existing helper to clear the action clip, procedural melee pose, and weapon action pose.

## Validation and evidence

- Source review: confirmed death is sampled in `Player::updateProceduralAnimation`, active clips are selected there, and `clearOneShot()` owns clearing the locked/action pose fields.
- `git diff --check` on the two changed source/spec files: passed (only line-ending conversion warnings).
- Tests: not run.
- Build: not run by the agent; repository policy reserves builds to the root dev-loop authority. Build note queued for the next whole-game DLL build, note ID `16951fde7ef1`.
- Runtime: not verified in the live executable or `events.jsonl`; no build identity or runtime behavior claim is made.
- Human visual acceptance: pending.

## Regression review

No regression record added. This is an explicitly requested behavior change; runtime evidence has not yet established a confirmed regression or a verified fix.

## Remaining evidence

Run the real dev-loop executable, interrupt a weapon animation with death, and inspect the matching client/server `events.jsonl` plus the visible actor pose. Confirm that the actor stays on idle while dead and normal animation state resumes after respawn.
