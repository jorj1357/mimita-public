# Melee contact modes

- Status: PASS_WITH_HUMAN_REVIEW
- Time: 2026-10-30T03:16:42Z (2026-10-29 23:16:42 America/New_York)
- Branch: `20261011zt`
- HEAD: `1b5f3bac`

## Result

Clarified that the no-artificial-cooldown contact rule is **Mode 1**, applying
across melee weapons that deal damage through continuous physical contact.
Added a Mode 2 TODO for click-triggered attacks with authored active hitboxes,
recorded as not implemented as of 2026-10-29 23:16 America/New_York. The TODO
requires the cadence and hit policy to be documented when Mode 2 is added.

## Exact change

File: `docs/specs/weapons/melee/melee-weapons.md`, heading “No Artificial
Melee Cooldown”.

Before:

```md
## **No Artificial Melee Cooldown**

There is intentionally **no generic melee hit cooldown**.

Simulation is 60 Hz.

Therefore a melee weapon may potentially hit:

once per victim
per weapon
per simulation tick

60 hits/second

This is intentional.
...
Extremely fast repeated melee contact is allowed and encouraged.
```

After: the heading now reads `Mode 1 — Continuous Physical Contact: No
Artificial Melee Cooldown`; the rule explicitly scopes the once-per-victim,
per-weapon, per-simulation-tick cadence to Mode 1 across all melee weapons.
Added `Mode 2 — Click-triggered active hitbox (TODO)`, with the requested
implementation status/date and instructions to specify contact cadence,
per-activation/per-victim policy, and intentional limits when implemented.

## Reasoning and review

- User clarified that the prior answer should apply to all melee weapons and
  named this existing rule Mode 1; Mode 2 remains future work.
- Read `docs/ROUTER.md`,
  `docs/specs/weapons/melee/melee-weapons.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- The routed `docs/doc-review-09-03-2026.md` file does not exist in this
  checkout.
- Specification review: PASS_WITH_HUMAN_REVIEW. This is an explicit
  documentation change requested by the user; no implementation or runtime
  claim was made.
- Documentation/TODO scan: no matching “todo: explain this better”, “todo:
  clean this document up”, or “todo: add an explanation for” entries were
  found outside router/skill instructions and historical changelog mentions.
  The new Mode 2 TODO is specific and states when it should be updated.

## Validation and remaining review

- Inspected the focused diff. No build, tests, runtime scenario, or gameplay
  acceptance was needed or performed for this documentation-only change.
- Existing unrelated worktree edits were present before this change and were
  preserved: config account/gamemode/movement files, Chainsaw/Deathball/
  Swordsword weapon configs, the Blender helper submodule, vision and several
  feature specs, plus an untracked changelog for vehicle/creator mode specs.
- Human review may confirm the Mode 1/Mode 2 labels match the intended design.
