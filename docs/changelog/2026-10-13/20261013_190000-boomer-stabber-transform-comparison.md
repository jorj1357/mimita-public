# Boomer v21 and Stabber v6 transform comparison

- Date: 2026-10-13
- Status: `DIAGNOSIS_COMPLETE_SPEC_UPDATED_IMPLEMENTATION_PENDING`
- Scope: `assets/entity/player/monsters/boomer/mimita-boomer-v21.glb`, `assets/entity/player/monsters/stabber/mimita-stabber-v6.glb`, and `docs/specs/avatar/avatar-editor.md`

## Request

Explain in simple terms why the working Boomer v21 model and failing Stabber v6 model behave differently, and use the evidence to define a general import normalization process rather than another avatar-specific rotation fix.

## Evidence

Both GLBs contain seven nodes, six meshes, no skins, and no animation clips. The difference is transform/frame data:

- Boomer v21 `plrOrigin` is approximately a 90-degree X rotation: quaternion `[0.7071, 0, 0, 0.7071]`.
- Stabber v6 `plrOrigin` is approximately a 90-degree Z rotation: quaternion `[0, 0, 0.7071, 0.7071]`.
- Boomer arm mesh positions are long on local Y, approximately `-2.39..0.08`, with small arm-node X rotations.
- Stabber arm mesh positions are long on local Z, approximately `-3.02..0.11`, with identity arm-node rotations.
- Node scales are omitted in both files, so scale defaults to one.

This identifies the first incorrect step as a mismatched complete bind frame: root basis, child locations, arm mesh axis, and shoulder pivot do not have the same meaning. Euler-order selection cannot correct that mismatch.

## Result

Updated `docs/specs/avatar/avatar-editor.md` with a concrete Boomer-v21/Stabber-v6 comparison and an explicit import rule:

- normalize only explicit custom player-body GLBs;
- convert root, child locations, child rotations, mesh vertices, and pivots together exactly once;
- place arm mesh origins at shoulder joints;
- let ordinary animation rotate the normalized arm nodes;
- automatically repair bounded transform-frame mistakes, scale, common root rotations, mesh-axis differences, and named-part pivot errors;
- reject genuinely ambiguous front/basis/pivot cases instead of guessing;
- do not copy Boomer v21's root quaternion as a universal fix;
- do not affect maps, weapons, props, cosmetics, or unrelated GLBs.

## Validation

- GLB structure and transform data were read directly from both files and a default control GLB.
- `git diff --check` passed for the avatar spec change.
- Existing trailing whitespace remains at `docs/jorj-docs/20261013plan.md:17`; it belongs to pre-existing user work and was intentionally not edited.
- No source code, GLB asset, or runtime configuration was changed in this investigation.
- Build, dev-loop runtime, live journal, and visual acceptance were not run; implementation remains pending.

## Required next implementation proof

Implement the player-only `player_canonical` normalizer from the avatar spec,
then validate with a fresh dev-loop executable and journal using Jason/default as
control, Boomer v21 as a working reference, and Stabber v6 as the previously
failing input. Acceptance must include camera look, arm swing, weapon grip and
muzzle alignment, hybrid/ragdoll binding, and proof that a non-player GLB is
unchanged.

