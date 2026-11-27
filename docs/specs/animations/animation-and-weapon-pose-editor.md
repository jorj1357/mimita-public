// 2026-11-27
/* purpose
* define one in-game editor for avatar animation and weapon/hand pose
* fold the animation timeline and weapon pose into the existing map editor
* make every edit live, replayable, and saved as a per-avatar/clip override
* this file DOES NOT define the animation clip library or the rig format
* this file DOES NOT create a second editor shell or a second pose path
*/

# Animation and weapon pose editor

**Status:** Desired behavior contract.

## The rule

> One editor, one selection model, one gizmo, one pose function.

The map editor already owns selection, camera, gizmos, undo, and panels. The
animation and weapon pose editor is a mode of that editor, not a second window.

The first/third-person viewmodel, cosmetics, the visible hit volume, and the
editor preview all read the same pose function. There is no separate
"editor pose" or "first-person pose".

## Modes

```text
Map        existing object/block/chunk editing
Actor      select an actor and inspect its rig
Animation  clips, tick timeline, keyframes, overrides
Weapon     held weapon, hand pose, per-tick fire/reload/equip tracks
```

Switching modes preserves the selected actor and the camera. Undo is shared.

## Actor selection

- Click selects the visible body part under the cursor, using the same
  `ActorHitVolume` as damage.
- The selected part and its socket chain are shown in the inspector.
- `Tab` cycles parts; `Shift+Tab` cycles actors.
- Selection works on players, NPCs, and preview actors.

## Animation mode

Layout:

```text
left:   numbered clip list (anim_list)
center: viewport with the selected actor
right:  inspector (clip, tick, keyframe, socket, override)
bottom: transport + tick timeline
```

Transport:

```text
play / pause
loop on/off
stop
previous/next keyframe
scrub to tick
speed (0.25x, 0.5x, 1x)
```

Timeline:

- horizontal axis is the tick, not seconds;
- keyframes are markers;
- the current tick is a playhead;
- selecting a marker selects its keyframe in the inspector;
- the timeline shows the clip duration and loop point.

Gizmos:

- `G` translate, `R` rotate, `S` scale, `Esc` cancel;
- the gizmo edits the selected part or keyframe;
- the same gizmo code as the map editor is used;
- numeric fields and gizmo edits are one undo step each.

## Weapon mode

- Select the held weapon on the same actor.
- Show the hand and weapon attachment socket.
- Edit offset, rotation, and scale with the same gizmo.
- Per-tick tracks for `equip`, `idle`, `fire`, `reload`, `holster`.
- The preview uses the first-person and third-person shared pose function.
- The weapon's muzzle, hit volume origin, and projectile spawn read the same
  pose, so a pose edit cannot desync visuals from damage.

## Data

Overrides are per avatar and per clip, never edits to the base library:

```text
config/animations/overrides/<avatar>/<clip>.json
config/weapons/pose/<weapon>.json
```

Override file:

```json
{
  "version": 1,
  "avatar": "jason",
  "clip": "mimita_sizing_6_walk",
  "tick_rate": 60,
  "tracks": {
    "leftArm": {
      "rotation": [ { "tick": 0, "quat": [0, 0, 0, 1] } ],
      "translation": [ { "tick": 0, "offset": [0, 0, 0] } ]
    }
  },
  "loop": true
}
```

Rules:

- The base clip is read-only.
- Loading merges base + override deterministically.
- An override records the base clip hash it was authored against.
- A base clip change that invalidates an override emits a warning event; it does
  not silently apply.
- Saving is atomic: write temp, fsync, rename.
- Reload is hot; no executable restart.

## Commands

```text
editor.anim.open
editor.anim.list
editor.anim.select <n>
editor.anim.play <n> [--loop]
editor.anim.stop
editor.anim.scrub <tick>
editor.anim.speed <factor>
editor.anim.gizmo g|r|s
editor.anim.save
editor.anim.reload
editor.weapon.select <weapon>
editor.weapon.pose <field> <value>
editor.weapon.track <clip> <tick>
editor.weapon.save
```

The commands exist so a test can drive the editor without the GUI. The GUI is a
projection of the same commands.

## Events

```text
editor.mode.changed
editor.actor.selected
editor.part.selected
editor.anim.list
editor.anim.play
editor.anim.stop
editor.anim.scrub
editor.anim.gizmo
editor.anim.override.saved
editor.anim.override.loaded
editor.weapon.selected
editor.weapon.pose.changed
editor.weapon.override.saved
editor.pose.preview.built
```

Every event names the actor, clip, tick, part or field, and the override path.

## Shared pose function

```text
poseActor(actor, tick, clip, overrides) -> part transforms
poseWeapon(actor, weapon, tick, overrides) -> attachment transform
```

- Render, cosmetics, hit volume, muzzle, projectile spawn, and editor preview
  call these.
- A pose edit changes all consumers in the same tick.
- The hit volume is rebuilt from the same pose hash.

## Performance

- Editor preview runs at the normal frame rate; it does not change simulation.
- Scrubbing does not allocate per frame.
- The editor is a hot module; a change reloads without an EXE restart.
- Opening the editor does not pause the server.

## Acceptance

- `anim_list` shows numbered clips for the selected actor.
- `anim_play 3` loops clip 3; `anim_stop` stops it; `anim_scrub` moves the
  playhead without playing.
- G/R/S edits the selected part and can be undone.
- Saving writes a per-avatar override and reloads it live.
- A weapon pose edit changes the preview, the muzzle, and the hit origin
  together.
- First-person, third-person, cosmetics, and hit volume show the same pose in a
  pinned run.
- No base library file is modified by an edit.
