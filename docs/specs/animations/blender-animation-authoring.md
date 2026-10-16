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
2. Export the action as data that converts into the existing
   `animations.json` clip/keyframe model, sampled or authored at integer
   simulation ticks.
3. Keep a stable mapping from Blender object/bone names to the rig's canonical
   part names. Reject or clearly report missing/duplicate mappings.
4. Preserve the actor root as the movement origin. Animation may pose limbs
   relative to that root; it must not silently move the authoritative actor
   root or create client-only gameplay transforms.
5. Let an attack clip start at its own tick zero when the authoritative action
   begins, then transition through the shared animation state machine to the
   next valid state.
6. Validate an exported clip in the running game and through the normal
   replicated player/NPC path before treating the export as accepted.

## Open decisions

- Whether Blender actions first target rigid multipart object transforms only,
  or whether armature/skinned animation is also required. The current runtime
  spec explicitly excludes armature retargeting.
- Whether conversion is a Blender add-on, an external exporter, or a checked-in
  project tool; do not invent a new runtime file format before comparing with
  `animations.json`.
- Sampling rate, interpolation conversion, root-motion policy, event markers,
  and how non-60-Hz Blender frame rates map to simulation ticks.
- How animation authoring cooperates with physical limb collision, ragdoll,
  weapon poses, and Melee-inspired attacks without allowing visual poses to
  bypass authoritative physics.

## Acceptance outline

- Export a simple idle and attack action from Blender for one existing rig.
- Confirm stable pivots, orientation, scale, and part-name mapping in-game.
- Confirm attack timing and visible pose agree for two clients and that the
  server remains the source of gameplay hit results.
- Confirm a missing or invalid part mapping fails visibly and does not replace
  the last valid animation data.

See also `docs/specs/gamemodes/supersmashbrosmelee.md` and
`docs/specs/weapons/swordsword.md` for proposed consumers of authored attacks.
