
2026-10-17 user decision (clarifies the earlier note): animation files may author rotations as Euler XYZ degrees or as quaternions. Convert either encoding to one normalized quaternion representation for runtime use. Keep the format names and coordinate spaces explicit so authors and code do not confuse local part rotation with world position.

// 2026-10-14T01:15:00Z
/* purpose
* define the desired behavior for authored, data-driven part animation
* give avatars (humanoids, monsters, flyers, crawlers, huge creatures) one
  animation format, one state machine, one runtime, and one live-edit path
* make "import a flying monster, name its wings in JSON, give it idle/fly/
  attack/hurt clips, and see it live" true
* this file DOES NOT define skeletal skinning, retargeting, or Blender authoring
* this file DOES NOT replace avatar-editor.md (transform normalization) or
  player-npc-systems movement/architecture documents
* this file DOES NOT require a second animation implementation for NPCs
*/

# Authoritative avatar and actor animation

Date: 2026-10-14
Status: SPECIFICATION — first implementation slice in progress
Owner: the animation owner (`Player` procedural animation + a shared
`AvatarAnimationRegistry`). Consumers: loader/render, movement, ragdoll/AimBody,
replication, and the Avatar Editor.

## Goal

A creator can take any rigid multipart `.glb` (humanoid, wasp, crawler,
centipede, giant) and, through JSON only:

1. declare which mesh nodes are which parts and what roles they play
   (`head`, `wing_L`, `stinger`, `tail_03`, ...);
2. give the avatar a set of named animation clips (`idle`, `fly`, `attack_1`,
   `hurt`, `death`, ...);
3. assign clips to states and triggers;
4. edit the clips live and see the result in the running game;
5. use the exact same clips for the player, NPCs, corpses, and remote replicas.

This spec is the source of truth for the animation *format*, the *state
machine*, the *runtime contract*, the *live-edit path*, and the *rendering of
arbitrary parts*. It builds on the normalized bind frame defined in
`docs/specs/avatar/avatar-editor.md`: every animated part rotates about its
derived joint pivot and channels are relative to the normalized rest pose.

## Does not

- Implement skeletal-skin/armature retargeting or import Blender action curves.
  The supported body is rigid multipart geometry (whether 6 parts or 100).
- Add a second animation path for NPCs. NPCs and players run the same clips.
- Replace movement physics. Movement owns *why/where an actor goes*; animation
  owns *what the parts do* while it goes.
- Change collision or hit detection for a part unless that part is declared
  `physical: true` in the rig.
- Bake animation into mesh data or the GLB.

## Design in one paragraph

A rigid multipart actor is described by a **rig**: an ordered list of **parts**,
each mapping a stable part name to a GLB node and a **role**. The rig lives in
`avatar.json` (per the decision to keep one file per avatar) under `rig`. Each
avatar may ship an **`animations.json`** next to `avatar.json`; it can `include`
clips from the **shared clip library** in `config/animations/`. A clip is a list
of **keyframes** at integer **ticks**; each keyframe names parts and gives per
part `translation`, an explicitly named Euler-degree or WXYZ-quaternion
rotation channel, and `scale`, plus an interpolation mode. A small **state machine** selects the active clip from
actor state (grounded/airborne/flying/crawling, attacking, hurt, dead) and
applies transitions with optional blend and one-shot semantics. The runtime
evaluates the active clip once per fixed tick, writes each part's `localTransform`
relative to its normalized rest pose, and the existing renderer draws every
declared part.

## Terminology

| Term | Meaning |
|---|---|
| **actor** | Anything with a `Player` body: local player, remote player, NPC, corpse, replay actor. |
| **rig** | The declared part list and movement capability for an avatar. |
| **part** | A named animatable unit mapped to one GLB node (a mesh node or an empty parent). |
| **role** | Semantic class of a part (`head`, `torso`, `arm`, `leg`, `wing`, `tail`, `stinger`, `weapon`, `helper`). |
| **channel** | One part's `translation` / `rotation` / `scale` over a clip. |
| **clip** | A named, ordered set of keyframes over `durationTicks`. |
| **keyframe** | Poses at one integer tick, with an interpolation mode to the next. |
| **state** | A logical behavior (`idle`, `locomotion`, `attack`, `hurt`, `death`) resolved to a clip. |
| **layer** | An additional clip/overlay composed on top of the base clip. |
| **blend** | Time-based interpolation between two poses or two clips. |
| **tick** | One fixed simulation step (60 Hz). Animation is tick- and velocity-driven, never render-frame-driven. |

## Owners

- **Loader / rig:** `player-loader` builds render parts and (optionally)
  physical parts from the rig; `AvatarSystem` owns the rig inside
  `avatar.json`.
- **Animation format + registry:** a new `AvatarAnimationRegistry`
  (`src/entities/player-avatar-animations.*`) parses `animations.json`, resolves
  `include`s from the shared library, and caches sets by avatar name.
- **Animation runtime:** `Player::updateProceduralAnimation` evaluates the
  active clip and writes part transforms. One implementation for all actors.
- **Movement capability:** movement presets + `rig.movement` decide
  gravity/hover/fly/crawl; movement reports state flags to animation.
- **Replication:** clients re-evaluate the same clips from replicated state;
  no new per-part wire format is required for client-visible NPCs.

## File layout and ownership

```
assets/avatars/<avatar>/
    avatar.json          # rig + movement + texture + policy; references animations
    animations.json      # optional: this avatar's clips, states, and includes
config/animations/
    library/<clip>.json  # shared, reusable clip files (any actor may include)
    <category>/<...>.json# optional shared overlays (weapon/reload/...)
config/animations.json   # legacy global profile; remains the default fallback
```

Rules:

- `avatar.json.animations` is either a relative path (default `"animations.json"`)
  or absent. Absent means "use the global fallback profile".
- `animations.json.include` is a list of repository-relative paths to shared
  clip files or directories. Includes are merged first; local definitions win.
- Shared clip files contain one or more named clips and never reference an
  avatar. They are the generalized library for "any avatar".
- Exactly one animation format version number at the top of every file.

### Melee action clip availability

**Current workflow — 2026-10-18 15:55 EDT (19:55 UTC):** A clip in
`config/animations/library/` is not automatically part of an avatar's loaded
animation set. Until automatic discovery is implemented, add each Melee action
clip file to the active avatar's `animations.json` `include` list. Keep the
move's `animation_clip_id` in `config/weapons.json` matched to the clip ID in
that included file.

When a Melee action has no visible animation in game, check these links in
order:

1. Find the active avatar's animation profile from `avatar.json` (or its
   default `animations.json` path).
2. Confirm that profile's `include` list contains the file that defines the
   action's `animation_clip_id`.
3. Confirm that included file defines that exact clip ID, with no duplicate
   clip ID in the shared library.
4. Confirm the Melee action in `config/weapons.json` references that same ID.
5. Check the `anim.melee.clip_missing` diagnostic and profile fallback if the
   clip still does not appear.

**Future follow-up — OPEN:** Make shared Melee clips discoverable by the
action's clip ID without manually adding every action clip to each avatar
profile. Define how discovery remains deterministic when clip IDs collide and
how actor-specific profiles restrict eligible clips before replacing the
explicit-include workflow. Automatic discovery is not implemented yet.

## `avatar.json` rig contract

The rig is additive to the existing avatar schema and does not change the six
canonical names.

```json
{
  "player_model": "assets/entity/player/monsters/wasp/mimita-wasp-v1.glb",
  "body_frame_policy": "player_canonical",
  "body_transform_source": "model",
  "animations": "animations.json",
  "rig": {
    "version": 1,
    "movement": { "mode": "fly", "gravity_scale": 0.0, "hover": true },
    "parts": [
      { "name": "head",     "node": "head",      "role": "head",    "physical": true },
      { "name": "torso",    "node": "torso",     "role": "torso",   "physical": true },
      { "name": "leftArm",  "node": "leftArm",   "role": "arm",     "physical": true },
      { "name": "rightArm", "node": "rightArm",  "role": "arm",     "physical": true },
      { "name": "leftLeg",  "node": "leftLeg",   "role": "leg",     "physical": true },
      { "name": "rightLeg", "node": "rightLeg",  "role": "leg",     "physical": true },
      { "name": "leftWing", "node": "wing_L",    "role": "wing" },
      { "name": "rightWing","node": "wing_R",    "role": "wing" },
      { "name": "stinger",  "node": "stinger",   "role": "stinger" }
    ]
  }
}
```

Field rules:

- `parts[].name` is the stable animation key (must be unique). For the six
  canonical parts it must stay `head`, `torso`, `leftArm`, `rightArm`,
  `leftLeg`, `rightLeg` so humanoid clips remain compatible.
- `parts[].node` is the GLB node name (Blender `.NNN` suffix tolerated). If
  omitted, `name` is used as the node name.
- `parts[].role` is one of `head`, `torso`, `arm`, `leg`, `wing`, `tail`,
  `stinger`, `weapon`, `helper`. Roles drive default pivot derivation and
  default aim/collision behavior; a role-less part animates but is render-only.
- `parts[].physical` defaults to `true` for the six canonical names and `false`
  otherwise. Physical parts participate in collision, hitboxes, ragdoll, and
  AimBody; render-only parts are drawn and animated but not simulated.
- When `rig.parts` is absent, the loader falls back to the six canonical names
  and the current behavior, so every existing avatar keeps working.

### Movement capability

`rig.movement` declares the actor's locomotion capability:

| Field | Values | Meaning |
|---|---|---|
| `mode` | `walk`, `fly`, `crawl`, `swim` | Primary locomotion mode. Default `walk`. |
| `gravity_scale` | number ≥ 0 | Multiplier on world gravity. `0` = weightless (flyers). Default `1`. |
| `hover` | bool | When true the actor maintains a target altitude instead of falling. Default `false`. |
| `hover_height` | number | Preferred altitude above the ground/water when `hover`. |
| `altitude_change` | number | Max vertical speed the actor can command while flying. |

First slice implements `gravity_scale` and `hover` as a simple flag; richer
flight integration (steering, altitude sticks, banking) iterates later.

## `animations.json` contract

```json
{
  "version": 2,
  "include": ["config/animations/library/base_idle.json"],
  "states": {
    "default": "idle",
    "locomotion": { "walk": "walk", "fly": "fly", "crawl": "crawl" },
    "attack": "attack_1",
    "hurt": "hurt",
    "death": "death"
  },
  "clips": {
    "idle": {
      "durationTicks": 60,
      "loop": true,
      "speedBased": false,
      "keyframes": [
        { "tick": 0,  "interp": "smooth", "parts": { "torso": { "translation": [0, 0, 0.02], "rotationEulerDegrees": [0, 0, 0] } } },
        { "tick": 30, "interp": "smooth", "parts": { "torso": { "translation": [0, 0, 0.05], "rotationEulerDegrees": [0, 0, 2] } } },
        { "tick": 60, "interp": "smooth", "parts": { "torso": { "translation": [0, 0, 0.02], "rotationEulerDegrees": [0, 0, 0] } } }
      ]
    },
    "fly": {
      "durationTicks": 30,
      "loop": true,
      "speedBased": false,
      "keyframes": [
        { "tick": 0,  "interp": "linear", "parts": { "leftWing":  { "rotationEulerDegrees": [0, -45, 0] },
                                                     "rightWing": { "rotationEulerDegrees": [0,  45, 0] } } },
        { "tick": 15, "interp": "linear", "parts": { "leftWing":  { "rotationEulerDegrees": [0,  20, 0] },
                                                     "rightWing": { "rotationEulerDegrees": [0, -20, 0] } } },
        { "tick": 30, "interp": "linear", "parts": { "leftWing":  { "rotationEulerDegrees": [0, -45, 0] },
                                                     "rightWing": { "rotationEulerDegrees": [0,  45, 0] } } }
]
    }
  }
}
```

### Clip fields

| Field | Type | Default | Meaning |
|---|---|---|---|
| `durationTicks` | int | 60 | Clip length in fixed ticks. |
| `loop` | bool | true | Loop continuously, or clamp on the last keyframe. |
| `speedBased` | bool | false | Scale playback by actor speed. |
| `speedScaleFromVelocity` | bool | true | When `speedBased`, map speed to playback rate. |
| `inputTriggered` | bool | false | Locomotion presence is input-driven, not speed-driven. |
| `tickBasedReturnToIdle` | bool | false | Play `return_to_idle`/`default` clip before settling. |
| `priority` | int | 0 | Higher-priority clips interrupt lower ones. |
| `blendInTicks` | int | 0 | Blend from the previous pose over this many ticks. |
| `blendOutTicks` | int | 0 | Blend toward the next pose over this many ticks. |
| `markers` | array | [] | Optional named, clip-local timeline markers. Combat phase markers are authoritative for that action's phase boundaries; see below. |
| `keyframes` | array | required | Ordered keyframes. |
| `audio` | object | optional | Tick-addressed animation sounds; schema below. |

### Clip markers and action timing

An animation clip owns its fixed-tick duration and any semantic timeline
markers used by its consumers. Markers are integer clip ticks. A melee action
clip includes exactly one each of `phase.windup`, `phase.active`,
`phase.recovery`, and `action.end`. The first three mark phase starts;
`action.end` is an exclusive endpoint and must equal `durationTicks`.
Therefore markers define half-open intervals `[start,end)`. For example,
duration 45 with markers at 0, 10, 25, and 45 means windup `[0,10)`, active
`[10,25)`, and recovery `[25,45)`. The last sampled pose is tick 44. A phase may
not be empty; markers must be strictly increasing.

`phase.windup` is the action's starting boundary and must be at tick 0. The
`phase.active` marker is the first active tick; `phase.recovery` is the first
recovery tick. To author a 10-tick windup, set `phase.active.tick` to 10; do
not move `phase.windup` to 10. To author a 15-tick active phase after that,
set `phase.recovery.tick` to 25. Recovery then continues up to, but not
including, `action.end`.

These marker values are editable in the exported clip JSON. That JSON is the
runtime source for the move's duration and phase timing. `durationTicks` must
equal the exclusive `action.end` tick. For example, markers at 0, 10, 25, and
50 make the clip 50 ticks long: windup is 10 ticks, active is 15 ticks,
recovery is 25 ticks, and the final sampled pose is tick 49. Changing
`action.end` from 60 to 50 therefore also changes `durationTicks` from 60 to
50. Pose keyframes remain independent; they do not need to be added for every
marker unless a specific pose is desired at that boundary.

```json
"markers": [
  { "name": "phase.windup", "tick": 0 },
  { "name": "phase.active", "tick": 10 },
  { "name": "phase.recovery", "tick": 25 },
  { "name": "action.end", "tick": 45 }
]
```

This keeps the total length and gameplay phase windows beside the authored
poses in one animation asset. A weapon action references one `animation_clip_id`
and may not repeat total duration, phase durations, or phase clip IDs. The
animation controls when the server considers the move windup, active, and
recovery; weapon data still owns hitbox geometry, damage, knockback, impulses,
input rules, and sound recipe. Hitbox per-tick overrides use active-phase-local
ticks, beginning at zero. A hitbox cannot be active outside the clip's active
interval.

Generic markers remain available for future consumers and events. Unknown
marker names are preserved/ignored by consumers that do not own them. Marker
ticks must be in `[0,durationTicks]`; only the exclusive `action.end` boundary
may equal duration. Event markers such as sounds occur on sampled ticks within
`[0,durationTicks)`.

For an action instance, the server and clients resolve the same clip content
revision and start tick. The server derives action phase and hit eligibility
from elapsed fixed ticks and that revision's markers. Clients use the same
timeline for pose and prediction; server authority/referee rules still decide
accepted gameplay results. Replication must identify the action/clip instance,
start tick, and content revision (or equivalent immutable timeline identity)
so a peer cannot apply different phase boundaries.

**OPEN:** whether a live edit changes the timeline of an already-running
action or whether that action pins the validated revision captured at start.
Whichever policy is selected must be consistent on server and clients; new
actions must use the newly accepted revision.

### Animation sound events

```json
"audio": {
  "onStartSoundId": "falcon_punch_start",
  "onStartPitchMultiplier": 1.0,
  "events": [
    { "tick": 12, "soundId": "falcon_punch_swing", "pitchMultiplier": 1.0 },
    { "tick": 18, "soundId": "falcon_punch_impact", "pitchMultiplier": 0.9 }
  ]
}
```

- `onStartSoundId` plays at clip-local tick 0 whenever a new clip instance
  starts, including an interrupted/retriggered Melee move.
- Each `events` entry plays once when playback reaches its integer clip tick.
  Exported markers must be quantized to simulation ticks; render frame rate
  cannot move a sound earlier or later.
- `pitchMultiplier` is optional, defaults to `1.0`, and scales the pitch from
  the resolved audio recipe for this event. Validate it as finite and within
  the audio owner's configured safe range; do not duplicate sound asset or
  device state inside the animation document.
- `onStartPitchMultiplier` applies the same audio-recipe multiplier rule to
  `onStartSoundId`; it defaults to `1.0`.
- Melee move sound ownership is in the selected `config/weapons.json` move-set
  definition, alongside input, hitboxes, percent, and knockback. A Melee action
  must define a start sound and may define phase-local sound events there; its
  clip must not duplicate those sound values. A new Melee action
  immediately stops the prior sound on that actor's Melee-animation channel
  and starts its tick-0 sound. Retriggering the same sound starts it from the
  beginning. This intentionally allows fast cancels to cut off earlier sounds.
  It does not interrupt music or unrelated world sounds.
- This channel rule is per actor, as a first implementation contract; whether
  the user intended one sound globally across the whole client remains open.
- Sound is presentation only. It cannot confirm a hit or alter simulation.
  Predicted and replicated events use a stable deduplication key composed of
  actor generation, action/clip instance ID, clip tick, and event index. A
  canceled clip emits no later markers; its replacement emits its tick-0 sound
  immediately.
- Sound IDs resolve through the existing audio/resource owner. Missing IDs
  produce a bounded diagnostic and do not stop animation.

### Animation-authored action timeline

Melee no longer retimes phase clips from durations in `config/weapons.json`.
One action references one authored clip. Its `durationTicks` and phase markers
in that clip define the complete action duration and the exact server-tick
windup, active, and recovery windows. Playback samples the clip directly at
elapsed clip tick; there is no normalized phase retiming. Editing the clip's
markers and keyframes together changes both the visible action and its active
window without editing a second timing file.

This is the required/default Melee runtime behavior for every action using
`animation_clip_id`, not an optional playback mode. `config/weapons.json`
supplies that clip ID and non-timing combat data such as hitboxes and damage;
it may not also supply phase durations or separate phase clip IDs. The runtime
resolves the ID from `config/animations/library`, validates the four markers,
and derives duration and phase lengths from them. If the referenced clip or
its required markers are missing, ambiguous, or invalid, that action is
invalid; it never falls back to weapon-config timing. Existing actions with
only the old `phases` object remain an explicit migration compatibility path
until each is given a single clip and markers. That compatibility path does
not apply when an `animation_clip_id` is present.

Tick zero is the first action tick. For duration `N`, sample ticks `0` through
`N-1`; `action.end` at tick `N` is exclusive and means the action is complete
before sampling another action tick. At each fixed tick, derive the phase from
the half-open marker intervals. Only ticks in `phase.active` can evaluate the
move hitbox. The hitbox follows its named body part's evaluated transform.
Root movement remains separate, action-owned data expressed in the captured
camera-relative basis and simulated/collision-checked by the movement owner.

The timeline controls eligibility, while the animation curve supplies the
limb path. Swept hitbox queries must use the fixed-tick poses for each active
tick; render frame time cannot stretch or shorten the active window.

### Keyframe and part fields

| Field | Type | Meaning |
|---|---|---|
| `tick` | int | Fixed tick within the clip. |
| `interp` | string | Interpolation from this keyframe to the next: `linear`, `smooth`, `easein`, `easeout`, `easeinout`, `step`. Default `linear`. |
| `parts` | object | Map of part name → channel values. |
| part `translation` | `[x,y,z]` | Offset in the part's normalized rest frame (meters). |
| part `rotationEulerDegrees` | `[x,y,z]` | Euler angles in degrees in the part's local normalized rest frame, composed as `Rx * Ry * Rz` (`qx(x) * qy(y) * qz(z)`). |
| part `rotationQuaternionWxyz` | `[w,x,y,z]` | Quaternion orientation in the part's local normalized rest frame. Must be finite and non-zero; the loader normalizes it. |
| part `scale` | `[x,y,z]` | Multiplier on the part's mesh. Default `[1,1,1]`. |

### Rotation encoding and conversion

- New animation files use format `version: 2`. At a part keyframe, specify
  exactly one of `rotationEulerDegrees` or `rotationQuaternionWxyz`. Never
  provide both for the same part and keyframe. The explicit names identify
  both the encoding and the quaternion component order.
- Format `version: 1` remains readable for compatibility. Its legacy `rotation`
  array means Euler XYZ degrees with the same `Rx * Ry * Rz` composition. New or
  edited files must use version 2 and the explicit field names.
- All file encodings describe a local orientation around the part pivot,
  relative to its normalized rest pose. They are not world positions,
  world-space rotations, or accumulated transform deltas.
- On load/import, convert Euler XYZ degrees to a quaternion using
  `qx(x) * qy(y) * qz(z)` and normalize it. Normalize valid input quaternions
  too. The animation runtime stores and evaluates only normalized quaternions;
  it does not branch on whether the source was Euler or quaternion.
- If a tool needs Euler output, convert from the canonical quaternion with the
  same XYZ order and wrap each angle to `[-180, 180)`. For the rotation matrix
  `R = Rx * Ry * Rz`, extract `y = asin(clamp(R02, -1, 1))`; when
  `abs(cos(y)) > 1e-6`, extract `x = atan2(-R12, R22)` and
  `z = atan2(-R01, R00)`. At gimbal
  lock, set `z = 0` and extract `x = atan2(sign(sin(y)) * R10, R11)`. Euler
  output is for authoring/interchange; never accumulate it frame to frame.
- Reject zero-length or non-finite quaternions and non-finite Euler values at
  load/validation; retain the last valid pose and emit the bounded animation
  diagnostic. A quaternion and its negation represent the same orientation;
  interpolation must choose the equivalent sign that gives the shortest arc.
- Apply the keyframe easing to interpolation time, then use normalized
  shortest-arc spherical interpolation (SLERP) between the canonical endpoint
  quaternions. Euler values are converted before interpolation; do not lerp
  Euler components. Legacy version-1 clips therefore load, but their between-
  keyframe path can change from the old Euler-component interpolation and must
  be visually reviewed during migration.
- **Implementation alignment (source review, 2026-10-18):**
  `src/entities/player-animation-config.cpp` reads the explicit Euler and WXYZ
  quaternion fields into quaternion pose storage, and
  `src/entities/player-animation.cpp` interpolates those rotations with
  shortest-arc SLERP. This is source evidence; the conversion cases and visual
  equivalence still need their focused validation.

### Blender and file-to-runtime conversion pipeline

Every importer/exporter follows this sequence. It must not branch the runtime
based on whether Blender supplied Euler angles or a quaternion:

1. **Sample a pose at a simulation tick.** Blender frame/curve evaluation is
   converted to an integer 60 Hz tick by the exporter's declared sampling
   rule. In Blender 5, evaluate the selected Action's assigned-slot FCurves and
   construct the transform from its location, rotation, and scale channels;
   do not rely on a potentially stale background `evaluated.matrix_world`.
   Do not treat a Blender frame number as a game tick unless the scene rate
   proves the mapping is exactly 60 frames per second.
2. **Extract the local part orientation.** Remove parent/world transforms and
   express the pose relative to the part's normalized rest frame and pivot.
   The exported pose is a local orientation, never a world-space quaternion.
3. **Convert the authoring basis once.** Use the canonical avatar coordinate
   contract: Blender and MiMITA are Z-up; game forward is +X and lateral is +Y.
   Let `B` be the transform required to express the imported reference GLB and
   evaluated Blender pose in that normalized game basis (identity when the
   source already matches it). Convert a rotation matrix `R_source` to
   `R_game = B * R_source * inverse(B)`, then convert that matrix to a
   quaternion. Convert local translation offsets as `t_game = B * t_source`.
   The exporter reports the selected `B`; runtime must not guess another axis
   correction or apply the conversion again. For the supported Blender 5
   direct-child player rig and default player GLB, `B` is the selected rig
   root's stored quaternion basis (the supported scene has an identity active
   Euler transform and stores −90° around X in that quaternion), calibrated
   against the reference asset. Root translation and scale are excluded. This
   mapping applies only to that rig/reference pair; reject other rigs until
   their basis and pivots are verified.
4. **Normalize to the canonical runtime value.** For Euler input, interpret
   version-2 `rotationEulerDegrees` as XYZ degrees composed exactly as
   `Rx * Ry * Rz` (`qx * qy * qz`), then apply basis conversion and normalize.
   For quaternion input, interpret version-2
   `rotationQuaternionWxyz` as `[w,x,y,z]`, convert its orientation through the
   same basis mapping, then normalize. Do not reorder quaternion fields
   implicitly or apply the Euler conversion to a quaternion input.
5. **Validate before install.** Reject non-finite values, zero-length
   quaternions, duplicate encodings on one keyframe, missing part mappings,
   and invalid basis metadata. Report source encoding, basis policy, tick,
   part, and reason. Preserve the last valid animation set on failure.
6. **Use one internal representation.** Runtime channels store only normalized
   local quaternions. Interpolate them using shortest-arc SLERP after easing.
   Rendering converts the resulting orientation to its matrix at the pose
   boundary. Any Euler export is derived from that quaternion for display or
   authoring; it is never accumulated from the previous tick.

### Blender export density and pose export modes

The Blender exporter contract is defined in
[`blender-animation-authoring.md`](blender-animation-authoring.md). The default
`Authored keyframes` mode writes only the union of integral transform-key frames
from the selected Actions. Each exported key contains a complete evaluated rig
pose, and the runtime interpolates smoothly between keys. A 60-tick clip still
has `durationTicks: 60`; for keys at Blender frames 1, 15, 30, and 60, JSON ticks
are 0, 14, 29, and 59. The optional `Dense every tick` mode writes an evaluated
pose for every fixed 60 Hz game tick.

The standalone exporter offers two pose export modes:

- `Authored keyframes` is the default. Blender evaluates all selected part
  Actions at each integral authored transform-key frame; only those complete
  poses are exported. The game interpolates the sparse keys using the shared
  interpolation curve and shortest-arc quaternion SLERP.
- `Dense every tick` is optional. Blender evaluates and exports the complete
  pose for every game tick, making each tick's pose explicit in JSON.

For the first exporter version, the Blender scene must run at 60 FPS. The first
sampled Action frame maps to game tick 0 (so frames 1–60 map to ticks 0–59).
The exclusive end marker maps to `durationTicks`. See the Blender authoring
spec for the unanimated GLB reference pose, Z-up coordinate frame, marker
convention, and export UI behavior.

The two JSON encodings for the same pose must produce equivalent rotation
matrices within a documented floating-point tolerance. A golden conversion
fixture must cover identity, a single-axis quarter turn, a combined XYZ turn,
near-gimbal Euler input, quaternion sign reversal (`q` versus `-q`), and any
non-identity normalization basis. The exporter report and runtime diagnostics
must make the chosen basis and source encoding inspectable. The canonical axis
convention is decided by `avatar-editor.md`; the exact normalized GLB-to-runtime
matrix and pivot delta remain implementation prerequisites.

Blender JSON output uses two decimal places for serialized transform values;
negative zero and values that round to zero serialize as `0.0`, never scientific
notation. The runtime normalizes rounded quaternion components before use. This
is a readability/authoring precision rule, not a substitute for a correct basis
or pivot conversion. If visual review finds quaternion quantization too coarse,
quaternion precision may be increased independently while retaining zero
cleanup and non-scientific output.

Version-2 examples for the two supported authoring encodings:

```json
{ "tick": 0, "parts": {
    "leftArm": { "rotationEulerDegrees": [0, 0, 90] },
    "rightArm": { "rotationQuaternionWxyz": [0.7071068, 0, 0, 0.7071068] }
} }
```

Parts not named in a keyframe hold their rest pose. A clip may name any rig
part, plus the special key `plrOrigin` for whole-body root motion.

### Interpolation

- `linear`: component-wise lerp.
- `smooth`: smoothstep `t*t*(3-2t)`.
- `easein` / `easeout` / `easeinout`: standard ease curves.
- `step`: hold the previous keyframe until the next tick.
- Translation and scale use the selected interpolation curve component-wise.
  Rotation uses the canonical quaternion conversion and shortest-arc SLERP
  described above. Curves are per-keyframe, matching the request "define 4
  poses, each with its own smoothing mode".

### Shared library

- `config/animations/library/*.json` files contain `clips` only (plus
  `version`). They are merged into every avatar that `include`s them.
- A directory path in `include` (ends with `/`) includes every `*.json` in it,
  sorted by filename.
- Local `clips` override included clips of the same name. Include order is
  declaration order; later includes override earlier ones.
- The library is intentionally non-avatar-specific so a flying body, a crawling
  body, or a giant can share wing/tail/gait clips.

## State machine

States are logical; clips are concrete. The machine is evaluated once per fixed
tick after movement and combat have produced their flags.

For the built-in actor-state selector, precedence from lowest to highest is
`idle < crouch < walk < run < jump < fall < land < hurt < respawn < death`.
An explicit locked action clip, hurt, respawn, or terminal state may replace the
selected state clip. Landing starts at clip tick 0 on the landing tick
(including a retrigger while the same clip is active) and remains selected over
automatic jump, fall, crouch, locomotion, and idle states until its non-looping
clip duration has elapsed. The latest automatic state resumes afterward. If the
profile has no landing clip, the existing brief landing fallback remains.

| State | Entered when | Default clip resolution | Loop | One-shot |
|---|---|---|---|---|
| `idle` | not moving, alive | `states.default` | yes | no |
| `locomotion` | moving | `states.locomotion.<movement-mode>` | yes | no |
| `attack` | an attack/hit edge this tick | `states.attack` | no | yes |
| `hurt` | damage taken edge | `states.hurt` | no | yes |
| `death` | actor died | `states.death` | no | terminal |
| `return_to_idle` | one-shot finished with `tickBasedReturnToIdle` | `states.default` | no | yes |

Transition rules (this is the tick-17 behavior the user asked for):

1. A higher-priority or higher-urgency trigger interrupts the current clip.
2. An interrupted clip stops at its current tick; the new clip starts at tick 0.
3. A one-shot clip plays to `durationTicks`; `attack`/`hurt` then fall back to
   `return_to_idle` if defined, then `idle`/`locomotion`.
4. `death` never returns.
5. If a state's clip is missing, fall back to `states.default`, then to the
   global fallback profile, then to the rest pose. Missing clips never crash and
   never produce NaNs.

An owning action system may explicitly replace a clip regardless of animation
priority. Melee uses this path: a newly accepted move cancels the current move
clip at that tick and starts the replacement at clip tick 0. The animation
system must not make the actor finish a long clip before honoring that action,
and it must not queue the canceled clip's later sound markers.

Triggers available to the state machine (produced by existing owners):

- movement mode (walk/fly/crawl) and "moving" boolean;
- attack edge (weapon fire / NPC attack);
- damage edge and current health;
- death;
- explicit command (`anim.play <clip>`) for editor/testing.

## Layers and precedence

Composition order, lowest to highest:

1. base clip (state machine);
2. locomotion additive (gait, bob) if configured;
3. weapon/reload overlay (existing);
4. aim/camera limb follow (existing `aimbody.json`);
5. hit/freeze/dash overlays (existing);
6. explicit editor/command override.

Each layer is optional. A layer names parts and channel deltas; missing layers
are skipped. The existing hard-coded dash/freeze/reload overlays become the
default instance of these layers and are expressed for the rig's parts rather
than six fixed names.

### Exclusive authored-pose policy

An actor profile may select `poseComposition: "authored_only"` for a mode or
actor context that requires JSON keyframes to fully determine the visible rig
pose. In this policy, sample the selected state/action clip and apply its part
transforms relative to the normalized rest pose. Do not add locomotion sway,
weapon/reload, aimbody, camera-follow, dash/freeze, or other procedural pose
layers to those authored parts. State selection still chooses the appropriate
authored clip; it does not bypass the state machine or change the fixed-tick
clock.

The actor root remains owned by shared movement/collision. Part-local animation
does not move the authoritative root. Any root travel during an action must be
expressed by the action/movement owner and simulated through normal collision.
The selected clip's sampled part transforms are also the pose source for
attachments such as move hitboxes; rendering and hit testing must not use
different animation samples or apply a second procedural offset to only one of
them. If a required clip is missing, use that profile's explicit fallback and
emit a bounded diagnostic; never silently re-enable a suppressed overlay.

The default policy remains the existing layered composition above. An
exclusive policy is opt-in and scoped to the actor/profile context, so Melee can
request it without changing other game modes or introducing a second animator.
The current Player runtime has an `authoredOnly` policy and uses it to suppress
procedural, aimbody, weapon, dash, and freeze pose layers for Melee authored
poses. This is source evidence; matching rendered limbs and hitbox attachments
still needs runtime verification.

## Runtime contract

- Evaluate once per fixed simulation tick (60 Hz), never per render frame.
- No per-tick heap allocation in the hot path: pre-size part arrays when the
  rig loads; interpolation works in place over a cached scratch buffer.
- Setting a part writes `perfectPoseSkeleton.nodes[part.nodeIndex].localTransform`
  as `restLocal * poseMatrix(channel)` exactly as today, so physics, hitboxes,
  and rendering continue to agree.
- Parts with no keyframe in the active clip keep their rest local transform.
- Missing node, missing clip, or non-finite value → keep last valid pose and
  emit one bounded warning; never propagate NaN.
- Ragdoll/hybrid mode continues to own the final transform while active; when it
  releases, animation resumes from the current state.

## Rendering arbitrary parts

To make wings/stingers/tails visible, the loader and renderer generalize from
"six physical parts" to "all declared rig parts":

- The loader builds a render mesh for every rig part (physical and render-only)
  and records `nodeIndex` + local mesh bounds.
- Physical parts keep their current collision/ragdoll behavior. Render-only
  parts are drawn and animated but never simulated.
- The renderer draws physical parts first (existing path), then render-only
  parts, each with its own `worldTransform`.
- The legacy fallback (whole-model mesh) remains for models with no rig.

## Live editing

- Saving an avatar's `animations.json` or any file it includes updates that
  avatar's clips in the running game. It never requires rebuilding or
  restarting the executable. This file is presentation data and its watcher is
  independent of the general gameplay-config hot-reload switch.
- The animation owner checks the avatar JSON, the selected animation file, and
  every included file at least once every 100 ms while actors are updating.
  After a successful save, the new set is installed at the next fixed
  simulation-tick boundary; the expected save-to-visible-update delay is at
  most 100 ms plus one 60 Hz tick, not counting a slow disk or editor save.
- Parse and validate a complete candidate before installing it. Install the
  complete set together so no actor can see a mixture of old and new clips.
  If a file is missing, half-written, malformed, or contains no valid clips,
  retain the last valid set and emit one bounded rejection event for that file
  revision. A later valid save is detected and applied without a restart.
- Existing players, NPCs, corpses, and remote replicas using that avatar all
  observe the same updated set. Do not require an avatar swap, respawn, editor
  button, or `anim.reload` after saving.
- An animation edit during a running state or Melee action keeps the actor's
  current state, action, and fixed-tick phase. The new clip data is sampled on
  the next tick at that state's current tick or normalized phase progress. It
  does not restart the action, replay sound markers, change hit windows, or
  alter movement/combat authority. Newly accepted actions use the new set.
- Saving an `avatar.json` rig change is different: re-import rig/model parts at
  the existing lifecycle-safe avatar/model boundary. A clip-only edit must not
  trigger model import or resource rebuilds.
- The Avatar Editor gains an Animation panel: choose a clip, play/pause/scrub,
  edit keyframes, add/remove states, and save to the avatar's `animations.json`.
- Editing an `avatar.json` `rig` re-imports parts at the lifecycle-safe
  boundary (same path as a model change).
- A terminal command `anim.play <clip>` and `anim.reload` provide the
  deterministic, no-GUI path used by the dev loop and evidence runs.

### Live-edit acceptance

With a game session already running, change one visible channel value in
`assets/avatars/captainfalcon/animations.json` and save it. Confirm the journal
records a higher `anim.set.reloaded` revision and the changed pose appears
within 100 ms plus one fixed tick, with no rebuild, restart, avatar swap, or
manual reload. Repeat while an action is active: its action and phase tick must
continue, and its hit window and sound markers must not move or replay. Then
save malformed JSON: the old pose must remain and one
`anim.set.reload_rejected` record must appear. Save valid JSON again and confirm
the next revision becomes visible. Human visual review confirms that the pose
change is clear and the actor does not stutter.

## Hot reload and lifecycle

1. Poll `animations.json`, its includes, and the animation-file selection in
   `avatar.json` for changes, at least every 100 ms while actors are updating.
2. Parse and validate a complete candidate without modifying the active set.
3. At the next fixed simulation tick, atomically replace the cached set in
   place so actors already holding it see the new clips safely.
4. Preserve the previous set on failure and emit one bounded
   `anim.set.reload_rejected` event per rejected file revision.
5. Emit `anim.set.reloaded` with avatar, included file paths, revision, clip
   count, and activation tick when a changed set becomes active.
6. A clip edit does not wait for spawn, respawn, teleport, map change, model
   change, avatar change, manual reload, or editor interaction. Rig/model edits
   still use their lifecycle-safe import boundary.

## Diagnostics

All records use `StructuredLogger` and the canonical `events.jsonl`; no
animation-specific file.

- `anim.set.loaded` / `anim.set.reloaded`: avatar, source and included paths,
  revision, clip count, activation tick, state map, result.
- `anim.set.reload_rejected`: avatar, rejected source path, reason, and whether
  the previous valid set was retained; emit once for each changed bad revision.
- `anim.state.changed`: actor id/kind, from-state, to-state, clip, tick,
  trigger.
- `anim.clip.missing`: requested state/clip, fallback used.
- `anim.part.sample`: bounded (≤1/s) per-part tick, clip, channel, world
  transform for troubleshooting.
- `anim.rig.parts`: declared parts, roles, physical flag, resolved node index,
  whether the node was found.

Diagnostics are off by default and enabled through the existing categorized
logging configuration.

## Replication

- Clients re-evaluate the same clips for NPCs and remote players from
  replicated transform + state flags, so no per-part wire format is required.
- Remote *players* may still layer the existing reconstructed wire limb pose on
  top; when present it wins for the six physical parts.
- Animation is deterministic given (clip, tick, speed, state flags), so client
  and server agree without extra packets.
- If an avatar's animation set is unavailable on a client, it falls back to the
  global profile; the model still loads.

## Movement and animation coupling

- Movement capability (`rig.movement`) reports the locomotion mode and moving
  state to the animation state machine.
- `gravity_scale` and `hover` are applied by the movement/physics owner; the
  first slice implements them as a simple per-actor flag rather than full flight
  steering.
- Crawling uses the same pipeline with a `crawl` locomotion clip and a low
  profile; flying uses `fly` and weightlessness.
- The same mechanism serves the player and NPCs; capabilities come from the
  avatar rig and movement presets, not from actor type.

## Failure and backward compatibility

- No `rig` and no `animations.json`: behave exactly as today (global profile,
  six parts). This is the compatibility guarantee for every existing avatar.
- Invalid clip/keyframe: skip the bad clip or keyframe, keep the last valid
  pose, emit `anim.clip.missing`/validation.
- Unknown part in a clip: ignored with a bounded warning.
- Unknown node in the rig: the part is dropped from render with a warning; the
  rest of the avatar still loads.
- Non-finite or extreme channel values: rejected per keyframe.

## Tests

Pure/model (no game required):

- rig parse: canonical defaults, custom parts, suffix-tolerant node names,
  `physical` defaults;
- clip parse: translation, both version-2 rotation encodings, legacy version-1
  rotation, scale, per-keyframe interp, loop/duration;
- interpolation: linear/smooth/ease curves at known ticks, Euler/quaternion
  equivalence, shortest-arc SLERP, normalization, and invalid-value rejection;
- include/merge precedence and override order;
- state machine: transitions, one-shot completion, interruption at tick,
  missing-clip fallback.

Runtime (dev loop, `docs/specs/debug-logging/AI-fix-live.md`):

1. publish a fresh executable through `python devscripts/dev-loop.py`;
2. `avatar.load <flyer>`; confirm `anim.set.loaded` and `anim.rig.parts`;
3. observe `fly` state, wing motion, hover altitude, and `anim.state.changed`;
4. trigger attack and damage; confirm one-shot `attack`/`hurt` then return;
5. load an NPC with the same avatar and confirm identical clip playback;
6. load the default avatar and confirm unchanged behavior.

## Acceptance

Human acceptance (not provable by build):

- a flying monster imports, its wings are selected via `avatar.json.rig`, and
  `idle`/`fly`/`attack`/`hurt` clips play and are editable live;
- a crawling monster and a huge monster reuse the same format;
- the default humanoid is visually unchanged.

## Implementation slices

1. **Format + registry:** version-2 explicit Euler/quaternion encodings,
   version-1 compatibility, normalized quaternion runtime channels, `rig` in
   `avatar.json`, `AvatarAnimationRegistry` with shared-library includes and
   global fallback, generalized interpolation. *(in progress)*
2. **Arbitrary-part rendering:** build and draw render-only rig parts.
3. **State machine + gravity/hover:** states/triggers and the simple movement
   capability flag.
4. **Editor + live edit:** animation panel, `anim.play`/`anim.reload`.
5. **Flying monster end-to-end**, then crawling and huge creatures.
6. **Replication/NPC parity and acceptance evidence.**

## Related documents

- `docs/specs/avatar/avatar-editor.md` (rigid-part transform/pivot contract)
- `docs/architecture/player-npc-systems/npc-movement.md` (actor capabilities)
- `docs/architecture/collision/actor-vs-actor-collisions.md` (NPC pose order)
- `docs/specs/movement/movement.md`
- `docs/specs/debug-logging/debug-logging.md`,
  `docs/specs/debug-logging/AI-fix-live.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `config/animations/readmewhat.md` (original intent)
- `docs/specs/animations/blender-animation-authoring.md` (Blender action export
  proposal and unresolved exporter/rig decisions; rotation encoding is defined
  in this file)
