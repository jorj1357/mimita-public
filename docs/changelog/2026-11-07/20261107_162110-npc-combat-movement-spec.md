# NPC combat and movement ideal behavior

- Date: 2026-11-07
- Timestamp: 2026-11-07T16:21:10Z (2026-11-07 11:21:10 EST)
- Type: specification
- Branch: `20261011zt`
- HEAD at start: `839322c04ef8ed95717609aba1a519ee4aa6ddfa`
- Scope: document desired elite NPC knowledge, varied weapon use, fair firing, and context-aware movement.

## Pre-existing work

The working tree already contained unrelated changes before this task. At final review, unrelated paths included account, analytics, audio, NPC difficulty, and video configuration; the `devscripts/blender-ai` submodule; dev-loop docs and regression records; and NPC collision diagnostics/gold documentation. These were not edited for this task. Existing untracked changelogs were preserved.

## Changes

- `docs/specs/npc-behavior/npc-behavior.md`, sections 1–5 and 12:
  - Added explicit elite behavior and exact through-wall enemy position knowledge, restricted to target selection and routing; aiming and firing still require legal line of sight.
  - Added seeded random starting weapons and bounded weapon visit lengths. Magazine weapons remain ammo-limited; melee visits use action count or duration. Preserved fixed cycles as an explicit option.
  - Required ordinary weapon cadence, ammo, reload, aim error, reaction delay, and attack validation to remain active. Normal elite play cannot use force-hit behavior; revolver bursts must allow reaction opportunity.
  - Added context-aware airborne point approach, downhill contact dash chaining, and look-ahead wall avoidance requirements, with stop/interruption conditions and shared movement ability rules.
  - Required high-level difficulty profiles to bundle behavior so routine tuning does not require coordinating unrelated global settings.
  - Added acceptance scenarios for randomization, melee visits, through-wall knowledge limits, slope chaining, anticipatory wall avoidance, and fair revolver firing.
  - Old wording in the Fun Counter Strike CT response said all CTs used only perception-gated reports and never received exact enemy positions. New wording limits that rule to below-elite profiles and explicitly preserves site-choice secrecy and line-of-sight firing for elite profiles.
- `docs/specs/gamemodes/funcounterstrike.md`, NPC objective strategy and rush:
  - Aligned CT knowledge with the explicit elite exception while retaining the restriction on Terrorist site-choice knowledge.
  - Clarified that easy and elite profiles share objectives and formation but may use different optional techniques.
  - Added the elite movement techniques by reference to the NPC behavior spec.

## Exact design wording added

- Old Fun Counter Strike CT rule: “They use perception-gated team reports and memory only.”
- New: below-elite profiles use perception-gated reports; an explicitly selected elite profile may use exact enemy locations through walls, but still needs line of sight to aim and fire.
- Old NPC weapon direction: fixed/ordered sustained-fire visits such as six revolver shots followed by two shotgun attacks.
- New default elite direction: seeded randomized starting weapon and bounded visit length within the resolved loadout; ammunition and legal cadence remain authoritative. Explicit fixed cycles remain supported.
- Old NPC movement direction: authored moves generally use shared inputs, with generic bunny-hop and air-strafe examples.
- New: add conditional airborne point approach, progress-checked slope contact dash chaining, and anticipatory wall avoidance with route recovery when blocked.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/specs/performance/performance.md`, `docs/specs/gamemodes/funcounterstrike.md`, `docs/specs/movement/movement.md`, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- Documentation checker: PASS_WITH_HUMAN_REVIEW. Changed specs agree on the elite wall-knowledge exception and preserve line-of-sight firing. Implementation remains partial and is described as desired behavior, not completed code.
- Specification behavior review: PASS_WITH_HUMAN_REVIEW. The mode-specific CT conflict was aligned. No implementation, runtime, or balance claim is made.
- Existing TODO follow-up: `docs/architecture/player-npc-systems/npc-movement.md:22` says “todo - explain”. Suggested expansion: explain the current goal → navigation → movement-intent → shared-physics ownership, then list the open multi-floor, traversal, and crowd-navigation acceptance gaps. Not edited because it is outside this task's requested spec changes.

## Validation and remaining work

- `git diff --check -- docs/specs/npc-behavior/npc-behavior.md docs/specs/gamemodes/funcounterstrike.md`: passed; only line-ending normalization warnings were reported.
- Reviewed the complete focused diff and checked the updated headings and cross-spec wording.
- No tests, build, or runtime gameplay were performed; this task changed specifications only.
- Human review is still needed for the intended difficulty of exact elite enemy knowledge and the feel of the proposed weapon and movement behavior. Implementation must follow the updated specifications and receive real gameplay validation later.
