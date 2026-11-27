# Animation and weapon-pose editor

**Status:** desired behavior contract for the in-engine animation editor.

## Purpose

Let a creator select an actor, play any animation clip in a loop, scrub its tick
timeline, and edit a limb's socket transform with in-world move/rotate/scale
gizmos. The same editor owns live weapon pose tuning (offset, rotation, scale,
and per-tick action poses) so fixing a down-pointing rocket launcher or iron
sights does not require a Blender round-trip.

## Entry and selection

1. Enter the map editor with `mapeditor 1`.
2. Select the local actor or any editor-selectable NPC/actor.
3. Open the animation panel with `anim_editor 1` or the map-editor Animation tab.
4. Leaving the map editor, moving the player, or pressing `anim_stop` ends the
   preview and restores the normal state machine.

## Clip list and playback

`anim_list` lists the selected actor's clips sorted alphabetically and numbered:

```text
[ANIM] 1  advance
[ANIM] 2  bounce
[ANIM] 3  walk
```

- `anim_play <number|clip>` starts the clip. Default loop is on.
- `anim_play <number|clip> 0` plays once and holds the last pose until stopped.
- `anim_stop` clears the preview and returns to normal animation.
- `anim_pause` / `anim_resume` pause the preview clock.
- `anim_scrub <tick>` samples the current clip at that fixed tick.
- `anim_step <ticks>` advances or rewinds a bounded number of ticks.
- Clip ids remain accepted as aliases; numbers are the primary editor surface.

Preview must not alter gameplay state, collision, damage, or weapon fire. It is a
locked presentation clip using the existing one-shot/locked-clip path.

## Tick timeline and gizmos

- The panel shows a tick ruler, current tick, keyframe markers, and the selected
  part.
- `anim_part <torso|head|leftArm|rightArm|leftLeg|rightLeg>` selects a part.
- `anim_key add [tick]`, `anim_key delete [tick]`, and `anim_key set` write a
  keyframe for the selected part at the current tick.
- `anim_mode move|rotate|scale` selects the gizmo; G/R/S are the hotkeys. Move and
  rotate are the primary modes. Scale is supported but not required for the first
  milestone.
- Gizmos use the map-editor handle style and screen-space picking. Dragging edits
  the selected part at the current tick only.
- Edits preview immediately. `anim_save` persists an override; `anim_reload`
  discards live edits and reloads the saved set.

## Save contract

- Save into `config/animations/overrides/<avatar-or-set>.json` by default. The
  base library under `config/animations/library/` is not modified by the editor.
- Overrides are merged over the base clip at load time and are hot-reloaded by
  the existing animation-set watcher.
- Saving never restarts the current action, hit windows, sound markers, or
  movement authority. A later valid save replaces the override atomically.
- A malformed save keeps the last valid override and emits one bounded
  `anim.editor.reload_rejected` event.

## Weapon pose layer

The same panel provides a weapon layer:

- Select a weapon id and an action state (`idle`, `fire`, `reload`, `equip`,
  `aim`).
- Edit the viewmodel/hand attachment transform: position offset, rotation, and
  scale. First-person and third-person use one pose owner; there is no separate
  viewmodel-only animation path. The visible arms, weapon, and hand attachment
  use the same render-pose function as the body.
- `anim_weapon_save` writes an override under `config/weapons/<id>/pose-override.json`
  or the existing weapon pose JSON owner.
- The editor reports sight alignment, muzzle alignment, and hand-to-grip distance
  so iron sights can be tuned without guessing.

## Later phases

- Two-bone foot/hand IK targets (`anim_ik foot|hand`) layered after the clip pose.
- Blender/glTF animation import/export.
- Per-face texture, UV, and vertex editing for imported GLB/OBJ assets.
- Monster/RPG attack animation authoring using the same editor.

## Diagnostics and acceptance

- `anim.editor` records play/stop/scrub/mode/save with actor, clip, tick, part.
- `anim.editor.saved` records override path, avatar/clip key, and part count.
- Acceptance: `anim_list`/`anim_play`/`anim_stop`/`anim_scrub` work live in a
  pinned run; a moved limb is visible at the edited tick; save/reload survives a
  restart; no gameplay authority changes while previewing.
