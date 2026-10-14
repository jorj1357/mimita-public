# Avatar root bind alignment

## Problem

The shoulder-pivot correction fixed attached arms and legs, but it also treated
the root torso like an attached joint. That removed the torso's imported
Blender bind translation from its visual origin. The torso therefore moved to
the physics center of mass while the head remained at its child attachment,
which appeared in-game as a floating torso/head and gaps between body parts.

## Change

In `src/ragdoll/ragdoll-mode.cpp`, visual origins now use two explicit rules:

- Attached parts use their physics body's `childLocalAnchor`, so an arm or leg
  rotates around its shoulder or hip rather than its mesh center.
- Root parts, currently the torso, retain the translation stored in the
  imported `meshLocal` transform. This preserves the Blender bind offset from
  the physics center of mass.

The change affects visual placement only. Physics bodies, collision, animation
rotation, and the existing legacy transform boundary are unchanged.

## Runtime evidence

- Published executable: build `2245`.
- Live executable: `C:\mimita-v9\.dev\builds\2245\mimita.exe`.
- Live journal: `C:\mimita-v9\logs\2026-10-14\20261014_001627\events.jsonl`.
- Runtime avatar loaded by the manifest: `monsterstabber`, using
  `assets/entity/player/monsters/stabber/mimita-stabber-v7.glb`.
- Exercised commands: `avatar.load monsterstabber`,
  `agent.snapshot torso_after_bind`, `agent.aim -1256 526 395`, and
  `agent.snapshot torso_after_aim`.
- `avatar.pose.sample` and `avatar.command.assert` records show matching
  rendered and physical part matrices after the correction.
- The corrected torso root position includes its imported bind offset; attached
  arm and leg positions remain anchored to their joint locations.

This is numeric/runtime evidence from the live journal. A final screenshot or
human visual acceptance check is still separate and has not been claimed here.

