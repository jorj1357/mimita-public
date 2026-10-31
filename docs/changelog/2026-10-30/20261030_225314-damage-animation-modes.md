# Damage Animation Modes

- Result: PASS_WITH_HUMAN_REVIEW (documentation-only; runtime behavior not changed)
- Timestamp: 2026-10-30T22:53:14Z / 2026-10-30 18:53:14 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD at start: `a7e5285a74a6ae8fe52aa4ced7dd39ecd2a10c98` (`stuff like jugernaut horde and pre weaponclash settings`)
- User request: specify a selectable mode where damage does not overwrite the mace attack animation, and distinguish that from the mode where it can.

## Final state

Added a weapon-level `damage_animation_mode` policy to the animation and melee specifications. Mode 1 preserves an active attack animation, still applies damage and effects, and does not defer a hurt animation until the attack ends. Mode 2 allows the normal hurt animation to interrupt; absent configuration defaults to mode 2 to preserve existing behavior. Clarified that this policy is independent of the existing melee contact-generation Mode 1/Mode 2 names. Identified the mace's requested behavior as damage animation mode 1. No runtime configuration or implementation was changed.

## Files changed by this session

- `docs/specs/animations/animations.md`, lines 521-539: added the animation-owned contract for the weapon setting and its two behaviors.
- `docs/specs/weapons/melee/melee-weapons.md`, lines 249-268: defined the setting as independent of melee contact modes and identified mace's requested selection.
- `docs/changelog/2026-10-30/20261030_225314-damage-animation-modes.md`: this record.

## Exact specification changes

In `animations.md`, the inserted requirement is:

> The weapon's `config/weapons/<id>/config.json` owns `damage_animation_mode`; it is not a damage, health, knockback, or contact setting. It governs animation selection only while that weapon has an active attack animation.

Mode 1 lets the active attack finish without queuing a delayed hurt animation, while leaving damage, health changes, hit effects, and knockback intact. Mode 2 allows hurt animation priority to interrupt. Omitted setting uses mode 2. This policy is orthogonal to melee contact generation; death and explicitly action-locked clips retain their ownership rules.

In `melee-weapons.md`, documented the same field, semantics, default, and orthogonality, and stated: “The mace's requested non-interrupting behavior corresponds to damage animation mode 1; runtime configuration and implementation remain separate work.”

## Preservation of pre-existing work

The worktree was already dirty. In particular, `melee-weapons.md` already contained uncommitted weapon-clash cooldown/configuration edits at session start; those edits are not part of this change and were preserved. Other unrelated modified, deleted, and untracked files were left untouched. No commits were created.

## Documents and review

- Read `docs/ROUTER.md`, `docs/specs/weapons/weapons.md`, `docs/specs/weapons/melee/melee-weapons.md`, `docs/specs/animations/animations.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/operations/task-completion/task-completion.md`.
- `docs/regressions/regressions-v1.md` is referenced by the router but is absent; the repository has dated regression folders instead. No regression was recorded because this change only clarifies desired behavior and no regression was established.
- Focused spec review: PASS. The damage-animation setting is explicitly separated from the pre-existing melee contact modes, and damage effects remain independent from animation selection.
- Documentation whitespace check (`git diff --check` on the two spec files): PASS.
- No build or runtime scenario was run because this is documentation-only.

## Human review still needed

Review the wording and later implement `damage_animation_mode` in the runtime and mace configuration. Runtime behavior and visual acceptance remain unverified.
