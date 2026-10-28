2026 10 27 1012 jorj doing animations

not sure what to do about arms in the animations, if a weapon is equipped mabye defer to the animations.json entry of it? 
but i dotn liek that i want to have like

one master list of weapons json so each weapon has all the values it could possibly have
not one big weapons.json

then each waepon has like its own animatons
bc weapons should be tools
and tools have their own aniamtions like
and u can reuse them
we alreaed have anim library
idk

### actual anim list 2026 10 27 1045

locomotion
0. idle 
1. walk
2. sprint
3. crouch
4. jump
5. falling
6. landing

combat

weapons

### general animations list all of them , for normal sized boy or girl avatars

walk
run
sprint

starting/stopping anim, e.g 50 m/s to 0 m/s maybe a quick anim  idk

dash
down dash
freeze 

knockback

jump
falling/in air(?)
landing
crouching, crouched, uncrouching

get hurt a little
get hurt normal
get hurt a lot

death 1?
death 2?

### weapons

chainsaw
equip?
idle
slash forward?

mace
equip
idle
slam down: windup, active, recovering, back to idle?

swordsword
equip
idle
left click 1x: slash down 
left click 2x: natural continuaition of being down, like, down then up/to the right

right click 1x: wide arc? sweep?
right click 2x and then hold: defensive pose, waepon is across ur whole body

### visuals

melee contact:

tick 1 of contact = white spehre like 0.5m diameter spehre, 0.5 alpha, bigger and more alpha when hitting harder

base = 0.5 diameter, 0.5 alpha, 123,123,123 color
max force = 1.0 diameter, 1.0 alpha, 255,255,255 color

tick 2 of cotnact = white sphere disappears, replaced with 
this spark effect png
"C:\mimita-v9\assets\textureshq\sparkv3.png"
"C:\mimita-v9\assets\textureshq\sparkv2.png"
"C:\mimita-v9\assets\textureshq\sparkv1.png"

tick 2 thru 6 = spark v1
tick 7 thru 13 = spark v2
tick 14 thru 20 = spark v3

client sided tick rate as well
all sparks configurable alpha and configurable random rotation
implementation owner: `config/hitfx.json` -> `meleeContact` (transient sparks
are effects, not decals). One contact effect per client tick is allowed.



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
    masterlist/animation-sets.json # ordered reusable clip/state sets
    settings.json        # priorities, hurt tiers, root translation, interpolation
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

## How to use this document

**Part I — task guide:** jump to “How to connect an animation to a player or
item,” “Quick tuning recipe,” “How to edit Melee playback timing,” and “NPC
tick-locked playback.” These sections describe the shortest path from an asset
change to a visible result.

**Part II — reference:** the clip/rig/state/runtime/replication sections define
the format and ownership rules. Read them when adding a new field, tool, or
actor type.

## NPC tick-locked playback

An avatar animation profile may opt NPC actors into a fixed authored-tick clock:

```json
"animation_profile": {
  "set": "general_set_1",
  "npcTickLocked": true
}
```

When this field is true, actors marked as NPCs advance exactly one authored clip
tick for each fixed 60 Hz simulation tick. Movement speed, `speedMultiplier`,
`speedBased`, `speedScaleFromVelocity`, and the global walk-frequency knobs do
not scale that clock. The profile is still sampled once per simulation tick, and
render-pose interpolation remains presentation-only.

This is an NPC-only option. The local player and remote human players using the
same avatar keep normal playback. NPC profiles without `npcTickLocked` keep their
existing playback behavior. Remote NPC replicas use the same flag because they
are presentation copies of NPC actors. Action clips with an authoritative phase
timeline continue to follow the action owner’s fixed-tick sample; the flag does
not retime their markers.

### Ordered sets, priorities, and runtime settings

`config/animations/masterlist/animation-sets.json` defines named reusable
sets. A set may include clips or directories and may provide a `states` map.
An avatar profile selects one with `"set": "<set-id>"`; the set is merged before
the avatar's own `include` and inline `states`, so avatar values still win.
`general_set_1` is the ordered set of every imported `mimita_sizing_6` clip.

`config/animations/settings.json` is hot-reloaded and owns runtime policy:- `statePriorities`: integer priority per state. Higher wins. Defaults keep
  landing below locomotion, hurt above locomotion/landing, and death highest.
- `hurtTierMode`: `1` uses percent of max HP at hit time, `2` uses fixed damage.
- `hurtPercent` / `hurtFixed`: small/normal thresholds. The next tier is large.
- `rootTranslationMode`: `1` keeps authored part translation; `2` zeroes it.
  A clip may override this with its own `rootTranslationMode`.
- `renderPoseInterpolation`: when true, presentation blends between the last two
  fixed-tick poses. Gameplay, collision, and hitboxes still use the fixed pose.
- `deathPresentation`: `auto`, `animation`, `ragdoll`, or `disappear`. A mode may
  set `death_presentation`; `auto` uses the authored small-death clip when
  ragdoll is not presenting the death.

New state keys are supported by `AvatarStateMap`: `dash`, `downDash`, `freeze`,
`knockback`, `crouchWalk`, `crouchIn`, `crouchOut`, `equip`, `hurtSmall`, `hurtNormal`,
`hurtLarge`, `deathSmall`, and `death`. Missing states fall back to the old
behavior.

### Melee action clip availability

**Current workflow — 2026-10-18 15:55 EDT (19:55 UTC):** A clip in
`config/animations/library/` is not automatically part of an avatar's loaded
animation set. Until automatic discovery is implemented, add each Melee action
clip file to the active avatar's `animations.json` `include` list. Keep the
move's `animation_clip_id` in `config/weapons/<id>/config.json` matched to the clip ID in
that included file.

When a Melee action has no visible animation in game, check these links in
order:

1. Find the active avatar's animation profile from `avatar.json` (or its
   default `animations.json` path).
2. Confirm that profile's `include` list contains the file that defines the
   action's `animation_clip_id`.
3. Confirm that included file defines that exact clip ID, with no duplicate
   clip ID in the shared library.
4. Confirm the Melee action in `config/weapons/<id>/config.json` references that same ID.
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
| `aimLayer` | bool | true | Apply AimBody over this clip. Set false to keep this clip's authored look pose unchanged. |
| `speedMultiplier` | float | 1.0 | Multiplies clip playback speed. |
| `lockTicks` | int | -1 | For a regular non-looping `playOneShot`, sets how long it holds. Phase-driven weapon actions are timed by their action owner. This does not currently enable interruptions. |
| `overrideMode` | string | priority | Parsed, but not currently used by the runtime. Do not rely on its values to interrupt clips. |
| `partMask` | array | [] | Optional part whitelist; empty owns every part. |
| `rootTranslationMode` | int | -1 | `1` keeps authored translation, `2` zeroes it, `-1` inherits settings. |
| `rootTranslationScale` | float | 1.0 | Multiplier on authored part translation. |
| `speedBased` | bool | true | Allow this clip's playback rate to scale with actor movement. |
| `speedScaleFromVelocity` | bool | true | When `speedBased`, map speed to playback rate. |
| `inputTriggered` | bool | false | Locomotion presence is input-driven, not speed-driven. |
| `tickBasedReturnToIdle` | bool | false | Play `return_to_idle`/`default` clip before settling. |
| `priority` | int | 0 | Added to the mapped state's priority during automatic state selection. It does not interrupt a locked weapon action. |
| `blendInTicks` | int | 0 | Parsed, but not currently used to blend poses. |
| `blendOutTicks` | int | 0 | Parsed, but not currently used to blend poses. |
| `markers` | array | [] | Optional named, clip-local timeline markers. Combat phase markers are authoritative for that action's phase boundaries; see below. |
| `keyframes` | array | required | Ordered keyframes. |
| `audio` | object | optional | Tick-addressed animation sounds; schema below. |

### How to connect an animation to a player or item

Think of animation setup as three separate steps: put the pose in the library,
tell an actor or item which clip names it can use, then map the game situations
to those names. A clip sitting in the library by itself will not play.

**For an avatar:** the current loader first checks
`assets/avatars/<avatar>/avatar.json` for an `animation_profile` object. That
profile can select a reusable set and then override its state map. For example,
Jason selects `general_set_1`; the set already contains the imported walk,
idle, sprint, jump, fall, landing, hurt, and death clips:

```json
"animation_profile": {
  "version": 2,
  "set": "general_set_1",
  "states": {
    "default": "mimita_sizing_6_idle_003",
    "locomotion": { "walk": "mimita_sizing_6_walk" },
    "run": "mimita_sizing_6_sprint_001",
    "hurt": "mimita_sizing_6_hurt_normal_001",
    "death": "mimita_sizing_6_dead"
  }
}
```

The set is defined in `config/animations/masterlist/animation-sets.json` and
its clips are files under `config/animations/library/`. Set clips and states
load first; the avatar profile's `include` files and inline `states` can replace
them. You can also omit `set` and list only the clip files you need in
`include`. If there is no inline `animation_profile`, the loader falls back to
the avatar's legacy `animations` filename (normally `animations.json`).

**For a weapon or held item:** its
`config/weapons/<id>/config.json` `animation_profile` points to a clip profile,
for example `config/animations/library/weapons/swordsword.json`. The profile's
`states.default` selects its held idle and `states.equip` selects its equip
clip. For a Melee move, the weapon's
`components.melee.actions.<action>.animation_clip_id` must exactly match a clip
ID included by that weapon profile. The weapon profile does not automatically
make every library clip available to every avatar.

### Which settings currently change playback

`config/animations/settings.json` is the shared, hot-reloaded policy file.
`statePriorities` gives each automatic state a base score. Each eligible clip's
`priority` is added to that score; the highest score wins on the next fixed
tick. Ties keep the earlier candidate in the runtime's selection order. For
example, `hurt: 60` beats `walk: 10` while a hurt timer is active. Raising hurt
or dash above another state makes it win more often whenever both are eligible.
Priority does not make a state eligible: the actor still needs to be hurt,
dashing, airborne, and so on.

For example, these values make hurt and dash beat equip and the lower movement
states more often:

```json
"statePriorities": {
  "idle": 0,
  "land": 5,
  "walk": 10,
  "run": 10,
  "jump": 20,
  "fall": 20,
  "crouch": 30,
  "crouchWalk": 30,
  "hurt": 85,
  "dash": 90,
  "downDash": 92,
  "freeze": 94,
  "attack": 80,
  "death": 100
}
```

These scores only choose among automatic candidates. In particular, raising
`hurt` above `attack` can beat an equip pose, but it still cannot break a locked
Melee attack clip.

### Locomotion clips should interrupt one another

Walk, run, land, crouch, jump, and fall belong to the actor's movement pose
family. When the actor changes to a different eligible movement state, the new
state should take over on that fixed tick; the old clip should not finish just
because it is a one-shot. Examples: run replaces walk when speed crosses the
run threshold, movement replaces landing as soon as movement starts, and a new
crouch transition replaces the previous crouch transition. The latest actor
state is then reevaluated each tick, so stopping or becoming airborne selects
the appropriate current state.

This rule also applies when a movement state interrupts a weapon swing. The
action owner must stop the action at the same fixed tick as the pose change, so
later phase events and hitbox samples from the canceled move do not run. The
server is authoritative for Melee hitboxes; the client predicts the pose and
sends the existing action-cancel request. Remote actors stop showing the action
when the next inactive action snapshot arrives.

The selector now chooses one movement-family state at a time. Crouch plus
movement uses the dedicated `states.crouchWalk` clip; if a profile does not
provide it, the normal walk clip is used. Starting movement or becoming
airborne clears any crouch-in/out transition timer. Changing movement state
replaces the prior clip on that fixed tick.

The automatic state selection covers idle, walk/run, jump/fall, land, crouch,
crouch-walk, hurt tiers, dash/down-dash, freeze, knockback, respawn, equip, and death. An
avatar's `states` map tells the runtime which clip ID to use for those states.
The profile's `states.attack` value is parsed but not currently read by the
Player state selector. Its temporary equip candidate is scored under the
`attack` priority name, but takes the clip from `states.equip`. So mapping a
general clip to `states.attack` does not automatically play it. Weapon attacks
are triggered by their weapon action and `animation_clip_id`.

**Locked weapon actions:** the action owner supplies the attack clip and
authoritative phase timeline. State priorities alone do not replace that lock.
When a movement state changes during a Melee action, the client ends its local
action and sends the existing cancel request; the server ends the authoritative
action before any later hitbox sweep. Death, unequip, and held-shield changes
also end an action through their existing owners. A cancel does not undo hits or
impulses that already occurred.

`overrideMode`, `blendInTicks`, and `blendOutTicks` are present in the clip
format and parsed by the loader, but source inspection shows they are not yet
applied by playback. They should not be tuned as if they already control
interruptions or blending. Implementing those behaviors for weapon actions
needs a shared rule for interrupting the visible pose while keeping action
timing and hitbox sampling in agreement.

### Quick tuning recipe for more frequent state changes

1. Confirm the state exists in the avatar profile and points to the intended
   clip ID. Confirm the clip file is loaded by `set` or `include`.
2. In `config/animations/settings.json`, compare the state priorities. Raise the
   reaction that should win (for example `hurt`) above the states it should
   replace. Keep `death` highest.
3. If two eligible clips map to states with close scores, add a small positive
   clip `priority` to the one that should win. State and clip scores add
   together.
4. Save the JSON while the game runs; the animation profile and runtime
   settings hot-reload. Check the `anim.state.changed` event for `selected_state`,
   `selected_priority`, `from`, and `to`.
5. A movement transition cancels the active Melee move and its future hitbox
   samples. State priorities do not cancel an action by themselves.

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

These marker values are editable in the exported clip JSON. Each required
Melee marker has two tick values: `sourceTick` is the Blender-authored pose
boundary, and `tick` is where that boundary plays in the engine. A fresh export
sets both values to the Blender marker's tick. To retime a clip, edit `tick`
and leave `sourceTick` unchanged. This preserves the authored pose boundary
while moving its playback time. New exports must write both values. Before
retiming an older clip that has no `sourceTick`, copy each current marker's
`tick` into `sourceTick` once; clips without `sourceTick` retain legacy direct
playback until migrated.

`durationTicks` equals the runtime `action.end.tick`, which is exclusive. For
example, runtime markers at 0, 10, 25, and 50 make the clip 50 ticks long:
windup is 10 ticks, active is 15 ticks, recovery is 25 ticks, and the final
sampled pose is tick 49. Changing the runtime end from 60 to 50 therefore also
changes `durationTicks` from 60 to 50. The source `action.end.sourceTick` remains
the authored end of the Blender pose timeline.

During playback, each interval between adjacent phase markers is mapped
linearly from its runtime `[tick]` interval to its authored `[sourceTick]`
interval. The engine samples the existing keyframe curve at that mapped source
position, including fractional source ticks; the normal keyframe interpolation
then supplies the pose. Thus shortening a phase speeds up its authored motion,
and lengthening a phase slows it down, without moving or re-exporting the pose
keyframes. A phase's speed ratio is its authored tick span divided by its
runtime tick span. For example, an authored active span of 20 ticks played in
5 ticks is 4x speed. Changing only `phase.active.tick` from 20 to 2 while
leaving recovery at 40 makes the windup 10x faster, but stretches the active
motion from 20 authored ticks across 38 runtime ticks; to make the active
motion faster too, move `phase.recovery.tick` closer as well.

#### Simple editing guide

Think of `sourceTick` as the page in Blender's flipbook where a pose was drawn.
Think of `tick` as when the game should show that page. To change the speed in
the game, edit `tick` and leave `sourceTick` alone. The engine slides through the
poses between the two marker lines; it does not move or redraw the Blender poses.

For example, Captain Falcon's `kick_a` starts with these marker values:

```json
"markers": [
  { "name": "phase.windup", "tick": 0, "sourceTick": 0 },
  { "name": "phase.active", "tick": 2, "sourceTick": 20 },
  { "name": "phase.recovery", "tick": 40, "sourceTick": 40 },
  { "name": "action.end", "tick": 59, "sourceTick": 59 }
]
```

Here is how to edit it:

1. Open `config/animations/library/melee/captainfalcon/kick_a.json` and find
   its `markers` list.
2. Change the playback `tick` numbers. Leave every `sourceTick` number alone.
   Keep the playback numbers in order: windup, active, recovery, end.
3. Keep `durationTicks` equal to the `action.end.tick` value. Save the file;
   the game reloads it, and new actions use the new timing.
4. If you export this clip again from Blender, choose **Preserve playback tick
   edits** in the exporter to keep your game timing. Choose **Reset playback
   ticks to Blender** only when you want Blender's marker positions back.

In the example, `phase.active.tick` is 2 but its Blender pose boundary is 20.
The windup therefore runs through 20 Blender ticks in 2 game ticks (10x faster).
Because recovery still begins at game tick 40, the active poses from Blender
ticks 20 to 40 are spread over 38 game ticks (about half speed). If you want
that active part faster too, move `phase.recovery.tick` closer, for example to
15. This makes active run from game tick 2 to 15; it also makes recovery start
earlier, so choose the number that gives you the timing you want.

Changing these playback markers changes both the visible pose timing and the
gameplay phase timing. For example, moving `phase.active.tick` also moves the
first tick when the move can hit. Keep the markers increasing and do not change
`sourceTick` when you are only tuning game speed.

```json
"markers": [
  { "name": "phase.windup", "tick": 0, "sourceTick": 0 },
  { "name": "phase.active", "tick": 10, "sourceTick": 20 },
  { "name": "phase.recovery", "tick": 15, "sourceTick": 40 },
  { "name": "action.end", "tick": 60, "sourceTick": 60 }
]
```

In this example, the active pose motion authored over 20 ticks plays over 5
runtime ticks, so it is 4x faster. `sourceTick` values must remain strictly
increasing and within the authored pose timeline. Runtime `tick` values must
remain strictly increasing and satisfy the existing phase and duration rules.

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
- Melee move sound ownership is in the selected `config/weapons/<id>/config.json` move-set
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

Melee no longer retimes phase clips from durations in `config/weapons/<id>/config.json`.
One action references one authored clip. Its runtime `durationTicks` and phase
marker `tick` values define the complete action duration and exact server-tick
windup, active, and recovery windows. The same markers' `sourceTick` values
anchor those phase boundaries in the Blender-authored pose timeline. Playback
maps each runtime phase interval linearly onto its source interval, then
samples the existing keyframes there. Moving a marker's runtime `tick` therefore
changes both gameplay timing and the visible phase speed; no second timing file
or keyframe edits are required. This is the required/default Melee playback
behavior, not an optional mode.

This is the required/default Melee runtime behavior for every action using
`animation_clip_id`, not an optional playback mode. `config/weapons/<id>/config.json`
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

The retimed animation curve supplies the limb path, and the runtime timeline
controls hit eligibility. Swept hitbox queries must use the retimed fixed-tick
poses for each active tick; render frame time cannot stretch or shorten the
active window. Server and clients must use the same validated clip revision and
the same piecewise-linear source-tick mapping.

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

**Implementation status:** the current Player selector chooses the highest
scoring eligible automatic candidate each tick, using `statePriorities` plus
the chosen clip's `priority`. Movement states are mutually exclusive, including
the dedicated crouch-walk state. A `lockedClip` still wins before automatic
selection; the Melee action owner explicitly clears it when the movement-state
signature changes, and the server stops the authoritative action before its
next hitbox sweep. `states.attack` is still not connected to a general attack
trigger. Other transition rules below may not be implemented yet.

For the built-in actor-state selector, eligible states are scored each fixed
tick using `settings.json` priorities plus clip priority. The movement pose is
exclusive: grounded crouch plus movement selects `crouchWalk`; crouch without
movement selects crouch; otherwise the current walk/run, jump/fall, landing, or
idle state is selected. A new state starts at tick 0 and interrupts the old
clip. Landing is eligible only while grounded and not moving. If the profile
has no crouch-walk clip, the normal walk clip is used; if it has no landing
clip, the existing brief landing fallback remains.

| State | Entered when | Default clip resolution | Loop | One-shot |
|---|---|---|---|---|
| `idle` | not moving, alive | `states.default` | yes | no |
| `locomotion` | moving | `states.locomotion.<movement-mode>` | yes | no |
| `crouchWalk` | grounded, crouch held, moving | `states.crouchWalk` or locomotion fallback | yes | no |
| `crouch` | grounded, crouch held, not moving | `states.crouch` | yes | no |
| `attack` | an attack/hit edge this tick | `states.attack` | no | yes |
| `hurt` | damage taken edge | `states.hurt` | no | yes |
| `death` | actor died | `states.death` | no | terminal |
| `return_to_idle` | one-shot finished with `tickBasedReturnToIdle` | `states.default` | no | yes |

Target transition rules:

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
actor context that requires JSON keyframes to determine the base rig pose. In
this policy, sample the selected state/action clip and apply its part transforms
relative to the normalized rest pose. AimBody adds look pitch to the authored
base pose by default, including attacks. An individual clip may set
`aimLayer: false` when looking up or down would spoil its authored action.
Do not add locomotion sway, weapon/reload, camera-follow, dash/freeze, or other
procedural pose layers to those authored parts. State selection still chooses the appropriate
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
The current Player runtime uses `authoredOnly` to suppress unrelated procedural,
weapon, dash, and freeze pose layers for Melee authored poses. It adds AimBody
unless the selected clip sets `aimLayer: false`. Matching rendered limbs and
hitbox attachments still need human runtime verification.

## Runtime contract

- Evaluate once per fixed simulation tick (60 Hz), never per render frame.
  `renderPoseInterpolation` may blend the last two fixed poses for presentation
  only; it must not change collision, damage, hitboxes, or fixed-tick selection.
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

- Saving an avatar's `avatar.json` animation profile or any file it includes updates that
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
  edit keyframes, add/remove states, and save to the avatar's `avatar.json`
  `animation_profile` and shared clip library.
- Editing an `avatar.json` `rig` re-imports parts at the lifecycle-safe
  boundary (same path as a model change).
- A terminal command `anim.play <clip>` and `anim.reload` provide the
  deterministic, no-GUI path used by the dev loop and evidence runs.

### Live-edit acceptance

With a game session already running, change one visible channel value in a
clip included by `assets/avatars/captainfalcon/avatar.json` and save it. Confirm the journal
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
7. When a Melee library clip changes, reload the move-set data that reads its
   marker `tick` values. New actions use the edited phase boundaries and the
   matching piecewise pose map together; actions already in progress keep their
   captured timing until they finish.

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
- `anim.npc.ticklocked`: bound NPC tick-locked profile, actor, clip, clip tick,
  movement speed, and playback scale. Emit at most four times per second.

Diagnostics are off by default and enabled through the existing categorized
logging configuration.

## Replication

- Clients re-evaluate the same clips for NPCs and remote players from
  replicated transform + state flags, so no per-part wire format is required.
- Remote *players* may still layer the existing reconstructed wire limb pose on
  top; when present it wins for the six physical parts.
- Animation is deterministic given (clip, tick, speed, state flags), so client
  and server agree without extra packets.
- Replay actors with an avatar animation set use the same shared state machine as
  live actors. Legacy recordings without a set retain their recorded body-part
  poses.
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
7. enable `npcTickLocked` on an NPC avatar profile and compare clip tick over
   the same simulation window at low and high movement speed; each simulation
   tick must advance one authored tick for NPCs while the player and an unset
   NPC profile keep existing playback.

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
