# Avatar shoulder-pivot live fix

Date: 2026-10-14

## User-visible problem

Custom avatar arms appeared to rotate around the middle of the limb instead of
the Blender shoulder origin during the hybrid aim path. Changing Euler order did
not correct it.

## Root cause confirmed

The GLB normalizer preserved the arm node origins. The hybrid aim/ragdoll
writeback then built each visual node as:

`body COM position * body rotation * meshLocal`

`meshLocal` includes the COM-to-node translation, so the visual node origin
orbited the physics center of mass when the limb rotated. The attachment solver
already had the shoulder anchor, but the visual writeback did not use it.

## Change

`src/ragdoll/ragdoll-mode.cpp` now separates physics placement from visual
placement:

- Physics bodies, collision capsules, COM offsets, and network state remain
  unchanged.
- Attached visual nodes use `body.position + body.orientation * childLocalAnchor`
  as their origin.
- The bind rotation/scale in `meshLocal` is retained, but its translation is
  removed from the visual matrix so it cannot reintroduce COM orbiting.
- The same rule is used by the live hybrid aim path and corpse/ragdoll visual
  synchronization.
- Bounded `AIMBODY_LIVE_SAMPLE` diagnostics now record the expected visual pivot,
  final node position, and their error.

## Evidence

- Build: dev-loop published build `2240`; `builds_match=true`.
- Runtime: `C:\mimita-v9\logs\2026-10-14\20261014_000007\events.jsonl`.
- Scenario: loaded `monsterstabber`, captured a snapshot, aimed at a target, and
  captured another snapshot through the dev-loop command path.
- Both `leftArm` and `rightArm` samples reported `final_node_pivot_error` at or
  below approximately `0.00002` world units while their body rotations changed.

## Remaining acceptance

The numeric runtime invariant is confirmed. A separate human visual check in the
visible client is still useful to confirm the mesh silhouette and weapon mount
look correct at several camera pitches; no screenshot-based acceptance is
claimed by this changelog.
