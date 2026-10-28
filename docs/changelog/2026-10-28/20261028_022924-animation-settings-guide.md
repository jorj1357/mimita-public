# Animation settings guide

- Time: 2026-10-28T02:29:24Z.
- Branch/commit: `20261011zt`, `ada6a0d0`.
- Result: `PASS_WITH_HUMAN_REVIEW`; documentation now explains current config ownership and identifies settings that are parsed but not implemented.

## Request and code findings

The request was to make `docs/specs/animations/animations.md` explain how to map clips to actors/items and how to make clips replace each other more often. Source review found:

- `AvatarAnimationRegistry::load` reads `avatar.json.animation_profile` first, merges its selected animation set, then includes and inline clip/state overrides. It falls back to the avatar's legacy `animations` filename when there is no inline profile.
- `WeaponDefinition.animationProfile` loads a weapon profile; that profile supplies held default/equip clips. Melee action animation comes from `components.melee.actions.*.animation_clip_id`.
- `Player::updateProceduralAnimation` scores eligible automatic states as `statePriorities[state] + clip.priority`, selecting the highest score each fixed tick.
- A non-empty `lockedClip` takes precedence before automatic state selection. Melee action owners update that locked clip from the combat phase timeline, so state priorities cannot interrupt an active action clip.
- `states.attack` is parsed but not selected by the general state machine. The equip candidate uses the weapon profile's `states.equip` but is scored as state `attack`.
- `overrideMode`, `blendInTicks`, and `blendOutTicks` are parsed into `AnimClip` but are not consumed by playback. `lockTicks` affects timed non-looping `playOneShot` duration; it does not provide a priority-based interrupt gate.
- The follow-up clarified the requested movement rule: walk, run, land, crouch, jump, and fall should switch immediately when the actor's state changes, rather than waiting for a prior one-shot to end. Source confirms run/walk is reselected per tick and movement clears landing, but `crouchIn`/`crouchOut` remain eligible until their timers expire and steady crouch currently outranks walk/run.

## Documentation change

Expanded `docs/specs/animations/animations.md` under `Clip fields`, `How to connect an animation to a player or item`, `Which settings currently change playback`, `Quick tuning recipe for more frequent state changes`, and `State machine` implementation status. Corrected defaults for `speedBased`, documented `aimLayer`, marked parsed-but-inactive settings, gave an example priority table, and distinguished current behavior from the target state-machine contract.

The guide explains that raising a reaction's state priority can make it replace eligible automatic states sooner. It also explains that priorities alone cannot interrupt an active weapon action; doing that safely needs an implementation that keeps the displayed pose and combat hitbox sampling synchronized.

Follow-up edit: added `Locomotion clips should interrupt one another` and revised the state-machine summary. Before, landing was specified as remaining selected for its full clip duration over locomotion; after, the target rule says a new movement state takes over immediately, with landing eligible only while stopped. The guide distinguishes this target from current crouch behavior and marks crouch-while-moving as undecided. Attack/hitbox cancellation is separately awaiting the user's choice.

## Review

- `docs/skills/spec-behavior-review-v1.md`: source/spec disagreement documented; weapon action preemption remains unimplemented and needs its own runtime design.
- `docs/skills/documentation-checker-v1.md`: paths and claims checked against current loader/config; no runtime behavior claims presented as implemented when source does not support them.
- No code changed and no build, runtime session, or test was run. `git diff --check` passed for the animation guide; the Markdown checker was reviewed. The router-listed `docs/doc-review-09-03-2026.md` does not exist in this checkout. The router's documentation TODO scan found these unrelated notes, left unchanged: `docs/architecture/player-npc-systems/npc-movement.md:22` (`todo - explain`), `docs/specs/weapons/melee/swordsword.md:231` (`todo need to make the poses animation things etc dont have tem right now`), and `docs/specs/weapons/tool-weapon-ideas.md:1` (`todo explain like`). Suggested expansions are listed below.
- Pre-existing working-tree edits were preserved: account settings, shared death/jump clips, dev timer tips, TDM settings, hit effects, video settings, and weapon collision config.

## Relevant TODO notes

- `docs/architecture/player-npc-systems/npc-movement.md:22` says `todo - explain`; suggested replacement: document the current NPC movement inputs and where authored locomotion clips are selected.
- `docs/specs/weapons/melee/swordsword.md:231` says `todo need to make the poses animation things etc dont have tem right now`; suggested replacement: list the SwordSword action IDs, required clip IDs, and current missing poses.
- `docs/specs/weapons/tool-weapon-ideas.md:1` says `todo explain like`; suggested replacement: state that the file is a proposal list and distinguish ideas from implemented tool capabilities.
