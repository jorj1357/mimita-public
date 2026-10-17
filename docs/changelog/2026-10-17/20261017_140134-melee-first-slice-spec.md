# Melee first-slice specification

- UTC: 2026-10-17T18:01:34Z
- Local time: 2026-10-17 14:01:34 EDT
- Branch: `20261011zt`
- HEAD: `a6401b0763e5335e8bc547149722662410fb313a`
- Scope: documentation only. No implementation, build, test, or runtime behavior was changed or claimed.
- Instructions followed: `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.

## Pre-existing work preserved

`docs/specs/gamemodes/supersmashbrosmelee.md` had uncommitted work before this session: its detailed sections 1–50, including movement, animation, controls, network, rollback, and rollout proposals. This session preserved that text and appended section 51. Other unrelated workspace edits were left untouched. `docs/specs/animations/animations.md` also had a pre-existing user note about quaternion conversion; it was not edited in this session.

## Changed file

### `docs/specs/gamemodes/supersmashbrosmelee.md:310-444`

Before this addition, the file ended with:

> - A transcript expresses intent and decisions. It does not prove implementation or runtime acceptance.

Added section 51, “First playable slice: Captain Falcon,” covering:

- a mode-owned fighter profile and move-set/action identity, with an ordinary slot allowed only as an input/UI binding;
- proposed mouse controls and explicit unresolved directional mappings;
- move data requirements and shared 60 Hz phase/animation timing;
- third-person actor-facing and camera responsibilities;
- Blender-authored clips and limb-bound, active-window hit shapes;
- the conflict between the current Euler JSON contract and the new quaternion request in `animations.md`;
- percent, knockback, stage blast zone, void presentation, stocks, and server ownership;
- prediction, authoritative results, and five first-slice acceptance scenarios.

The new text begins:

> ## 51. First playable slice: Captain Falcon
>
> **Status: PROPOSED; controls and match rules below still need the decisions in
> section 49.** This section makes the first implementation target concrete
> without claiming that the mode, attacks, exporter, or networking path already
> exists.

Reasoning: the spec already has general fighter intent but leaves the first character, input mapping, hit attachment, and concrete combat/network responsibilities underspecified. This addition makes those implementation contracts reviewable while labeling uncertain controls and match policy as open.

## Related specification review

- `docs/specs/networking/networking.md:450-489` already specifies stable item identity, server validation, actor-generation handling, and that hotbar position is not weapon identity. The Melee extension applies that distinction to fighter move-set/action IDs; it does not claim a new networking implementation.
- `docs/specs/animations/animations.md:2,63,248-260` contains a conflict: the dated note requests no Euler animation rotations and normalized quaternion conversion, while the current JSON contract specifies Euler XYZ degrees. The new Melee section proposes normalized local quaternions for new authored poses and explicitly requires the shared format to resolve a versioned representation before implementation. Existing clip interpretation is not changed.
- `docs/specs/animations/blender-animation-authoring.md` labels the Blender exporter path as a proposal and leaves rig, conversion, tick sampling, and collision integration open. The Melee addition links to this document and does not claim an exporter exists.
- `docs/specs/weapons/melee-weapons.md` remains the separate contract for existing generic melee weapon behavior; this mode uses fighter move/action identity and percent/knockback rules.

## Focused review and validation

- Specification and behavior review: passed for scope and authority boundaries; unresolved decisions remain labeled in the spec.
- Documentation checker: links to the animation and networking specs resolve; changed content is clearly labeled as proposed and does not claim implementation.
- `git diff --check -- docs/specs/gamemodes/supersmashbrosmelee.md`: passed.
- No tests or builds were run because this was a documentation-only edit.
- No runtime, multiplayer, exporter, or human-play acceptance was performed.
- The repository-wide TODO scan found existing informal notes outside this task. In the edited target, line 1 still says `2026 10 08 1228 jorj todo explainbetter`; suggested replacement: retain it only as a dated historical note if needed, and rely on the expanded titled sections below it as the current explanation. It was left unchanged to preserve historical user-authored context.

## Human decisions still needed

1. Should Captain Falcon use an always-active mode move-set controller, with slot 1 only as a UI/input binding, or should the mode expose a literal equipped item?
2. Which direction does each WASD modifier mean for A/B attacks, and how should actor facing turn relative to the third-person camera?
3. Should crossing the void blast zone always consume one stock and respawn, and what are the initial stock count and respawn rules?
4. Should the shared animation JSON move to normalized quaternion channels, and if so, what versioned migration preserves existing Euler clips?
