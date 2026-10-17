# Blender Animation Authoring and Export

Status: `PROPOSAL — NEEDS_IMPLEMENTATION_AND_FORMAT_REVIEW`.

## Purpose

Record the desired path from an authored Blender animation to the existing
runtime animation system. This document covers authoring/export. The runtime
clip format, state machine, playback, and replication remain owned by
[`animations.md`](animations.md). The model transform and rig normalization
contract remains owned by [`../avatar/avatar-editor.md`](../avatar/avatar-editor.md).

This does not specify a second runtime animator, require skeletal skinning, or
claim a Blender exporter already exists.

## Proposed first supported slice

1. Author an action in Blender for a rig whose part names and pivots satisfy
   the current rigid multipart avatar contract.
2. Export the action into the version-2 `animations.json` clip/keyframe model,
   sampled or authored at integer simulation ticks. A part keyframe may encode
   orientation as Euler XYZ degrees or a WXYZ quaternion according to
   [`animations.md`](animations.md); the shared loader converts either encoding
   to the normalized quaternion runtime representation. Convert Blender axes
   and parent/world transforms into normalized local part space exactly once;
   record the explicit basis policy in the export report as required by
   [`animations.md`](animations.md).
3. Keep a stable mapping from Blender object/bone names to the rig's canonical
   part names. Reject or clearly report missing/duplicate mappings.
4. Preserve the actor root as the movement origin. Animation may pose limbs
   relative to that root; it must not silently move the authoritative actor
   root or create client-only gameplay transforms.
5. Let an attack clip start at its own tick zero when the authoritative action
   begins, then transition through the shared animation state machine to the
   next valid state.
6. For generic animation-owned sounds, export an immediate start sound plus
   optional sound markers at integer clip ticks using the `audio` shape in
   [`animations.md`](animations.md). Melee move sounds instead belong to their
   `weapons.json` action definition and are authored at phase-local ticks.
   Blender timeline frames must be converted to 60 Hz simulation ticks by one
   documented sampling rule; markers landing between ticks must be rounded by
   that same rule and shown in the export report.
7. Validate an exported clip in the running game and through the normal
   replicated player/NPC path before treating the export as accepted.

## Open decisions

- Whether Blender actions first target rigid multipart object transforms only,
  or whether armature/skinned animation is also required. The current runtime
  spec explicitly excludes armature retargeting.
- Whether conversion is a Blender add-on, an external exporter, or a checked-in
  project tool; do not invent a new runtime file format before comparing with
  `animations.json`.
- Blender-curve interpolation conversion, root-motion policy, and how non-60-Hz
  Blender frame rates map to simulation ticks.
- How animation authoring cooperates with physical limb collision, ragdoll,
  weapon poses, and Melee-inspired attacks without allowing visual poses to
  bypass authoritative physics.

## Acceptance outline

- Export a simple idle and attack action from Blender for one existing rig.
- Confirm stable pivots, orientation, scale, and part-name mapping in-game.
- Confirm attack timing and visible pose agree for two clients and that the
  server remains the source of gameplay hit results.
- Confirm Euler and quaternion exports of the same pose produce equivalent
  runtime rotations, and inspect the reported basis conversion and 60 Hz tick
  mapping.
- Confirm a missing or invalid part mapping fails visibly and does not replace
  the last valid animation data.

See also `docs/specs/gamemodes/supersmashbrosmelee.md` and
`docs/specs/weapons/swordsword.md` for proposed consumers of authored attacks.
