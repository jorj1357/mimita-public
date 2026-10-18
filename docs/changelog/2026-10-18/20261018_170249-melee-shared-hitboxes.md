# Melee shared hitboxes and Force Punch contact contract

- Status: PASS_WITH_HUMAN_REVIEW (specification updated; gameplay implementation and acceptance remain outstanding)
- Recorded: 2026-10-18T17:02:49Z (2026-10-18 13:02:49 EDT, America/New_York)
- Branch: `20261011zt`
- Base commit: `3a89b2de5059eee6256b5b4a614a83496acc5709`
- Final commit: unchanged

## Scope and result

Added section 58 to `docs/specs/gamemodes/supersmashbrosmelee.md` in response to the user's request to specify Force Punch style contacts across all Captain Falcon moves, with player and NPC actors sharing the attack and visual path. The section makes `config/weapons.json` the single editable move-set source and requires per-active-tick attachment, supported shape, dimensions, and offset; it requires a matching translucent red 0.5-alpha active hitbox visual. It also states that Melee applies percent and impulse to either actor, while other modes retain their own result policy.

The exact combined force curve and hit-repeat policy remain open in the existing specification. This change records desired behavior; it does not claim the current code implements it.

## Files and exact changes

### `docs/specs/gamemodes/supersmashbrosmelee.md`

- Added lines 704–714, `## 58. Shared Force Punch contact and active-hitbox presentation`. There was no prior text at this insertion point.
- Exact addition:

```text
## 58. Shared Force Punch contact and active-hitbox presentation

**DECIDED for Captain Falcon Melee:** Every attack in `captain_falcon_moveset` uses one shared, actor-neutral contact pipeline based on Force Punch's physical-contact behavior. Players and NPCs use the same move definition, active-tick contact query, hitbox visualization, contact-force inputs, and confirmed-result path whether they are attacking or being hit. Human input and NPC decision-making may choose actions differently; they do not select different hitbox, collision, or damage/knockback implementations.

`config/weapons.json` remains the single editable source for the move set. For every move and each active simulation tick, its hitbox data must allow editing the attached named limb, supported shape, shape dimensions (including sphere radius where applicable), and limb-local offset. Tick-specific values may change as the animated attack progresses. The move's active phase determines which tick records are evaluated. Do not add a parallel Melee hitbox configuration file or hard-code per-move geometry in C++.

At each fixed tick, derive the hitbox world transform from the attacker's authoritative actor root and sampled animated limb pose, then sweep the configured shape from its previous to current transform. Use the same resolved transform and shape for contact testing and visualization. During each active tick, render the attack volume as a translucent red shape with 0.5 alpha for player- and NPC-originated attacks on observing clients. The visual follows the same limb and tick-specific geometry used by the contact query; a separate visual-only approximation is not acceptable.

The shared contact result uses Force Punch's normalized contact-center proximity and movement-through-volume force inputs, together with Melee's already specified approach directness. Those inputs feed one shared force calculation for the move's percent and knockback. Melee's result policy adds percent and applies impulse to either player or NPC actors; it does not substitute ordinary health damage. Outside Melee, a reused contact result must still pass through the active mode's damage/result policy. Keep the exact combined force curve, normalization, and caps open as recorded in sections 25–26 until they are selected or explicitly made JSON-editable provisional tuning.

Keep the existing hit-count decision open under section 23: adding per-active-tick geometry does not by itself decide whether one target can be confirmed once per activation or multiple times. All client prediction, server validation, actor generation, move revision, and accepted-result replication continue to follow sections 23–24 and the shared networking contract.
```

- Updated line 718 to reflect that hitbox visualization is now specified and remains to be implemented.
- Before: `- **Melee presentation to specify/implement:** visible shield sphere/effect; a debug view for configured attack hitboxes and their active windows; fuller Captain Falcon limb/leg clips and required idle, locomotion, jump/fall/land, attack, shield, grab, and respawn states.`
- After: `- **Melee presentation to implement:** visible shield sphere/effect; the active-hitbox visualization specified in section 58; fuller Captain Falcon limb/leg clips and required idle, locomotion, jump/fall/land, attack, shield, grab, and respawn states.`

## Evidence and review

- Reviewed current Melee requirements in sections 21–26, 51, 53, 57 and existing open decisions in sections 23, 25–26.
- Source review found that current Melee server contacts use an approximate root-relative center and `size.x` sphere radius; the current debug renderer draws weapon collision capsules rather than the configured Melee action volumes. These implementation gaps support specifying the shared animated hitbox path and matching visualization.
- `git diff --check` was run on the spec. It reported trailing whitespace on previously existing informal note lines 802–823; none of the added section lines have trailing whitespace. Those user-authored notes were preserved.
- Manual document check confirmed section 58, internal section references, and its reference from the presentation status list.
- No build, tests, or runtime scenario were run; this is a documentation-only change. Gameplay behavior, live visuals, and user acceptance remain unverified.

## Instructions and warnings

- Read `docs/ROUTER.md`, the relevant Melee and weapons specifications, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- The router also names `docs/doc-review-09-03-2026.md`, but that file is absent. A repository file search found no current replacement; only archived `docs/regressions/archive-ignore/regressions-v1.md` exists for the regression index.
- The router's TODO note applies to the existing first line of the Melee spec, `2026 10 08 1228 jorj todo explainbetter`. It was not changed. Suggested resolution: replace that raw marker with a short explanation that the opening text is historical context and numbered sections are the active requirements.
- Preserved pre-existing uncommitted changes in the Melee spec and source/configuration files, including `src/network/server-melee-actions.cpp`, `config/weapons.json`, and unrelated Dev Loop/configuration/assets edits. No pre-existing changes were staged, reverted, or claimed as part of this task.
