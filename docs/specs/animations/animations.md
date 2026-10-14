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
part `translation`, `rotation` (Euler degrees), and `scale`, plus an
interpolation mode. A small **state machine** selects the active clip from
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
  "version": 1,
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
        { "tick": 0,  "interp": "smooth", "parts": { "torso": { "translation": [0, 0, 0.02], "rotation": [0, 0, 0] } } },
        { "tick": 30, "interp": "smooth", "parts": { "torso": { "translation": [0, 0, 0.05], "rotation": [0, 0, 2] } } },
        { "tick": 60, "interp": "smooth", "parts": { "torso": { "translation": [0, 0, 0.02], "rotation": [0, 0, 0] } } }
      ]
    },
    "fly": {
      "durationTicks": 30,
      "loop": true,
      "speedBased": false,
      "keyframes": [
        { "tick": 0,  "interp": "linear", "parts": { "leftWing":  { "rotation": [0, -45, 0] },
                                                     "rightWing": { "rotation": [0,  45, 0] } } },
        { "tick": 15, "interp": "linear", "parts": { "leftWing":  { "rotation": [0,  20, 0] },
                                                     "rightWing": { "rotation": [0, -20, 0] } } },
        { "tick": 30, "interp": "linear", "parts": { "leftWing":  { "rotation": [0, -45, 0] },
                                                     "rightWing": { "rotation": [0,  45, 0] } } }
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
| `keyframes` | array | required | Ordered keyframes. |

### Keyframe and part fields

| Field | Type | Meaning |
|---|---|---|
| `tick` | int | Fixed tick within the clip. |
| `interp` | string | Interpolation from this keyframe to the next: `linear`, `smooth`, `easein`, `easeout`, `easeinout`, `step`. Default `linear`. |
| `parts` | object | Map of part name → channel values. |
| part `translation` | `[x,y,z]` | Offset in the part's normalized rest frame (meters). |
| part `rotation` | `[x,y,z]` | Euler degrees applied about the part's pivot. |
| part `scale` | `[x,y,z]` | Multiplier on the part's mesh. Default `[1,1,1]`. |

Parts not named in a keyframe hold their rest pose. A clip may name any rig
part, plus the special key `plrOrigin` for whole-body root motion.

### Interpolation

- `linear`: component-wise lerp.
- `smooth`: smoothstep `t*t*(3-2t)`.
- `easein` / `easeout` / `easeinout`: standard ease curves.
- `step`: hold the previous keyframe until the next tick.
- Rotations interpolate numerically in Euler degrees, consistent with the
  existing runtime. Curves are per-keyframe, matching the request "define 4
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

- `animations.json` and the shared library are hot-reloaded by file mtime.
- The Avatar Editor gains an Animation panel: choose a clip, play/pause/scrub,
  edit keyframes, add/remove states, and save to the avatar's `animations.json`.
- Editing an `avatar.json` `rig` re-imports parts at the lifecycle-safe
  boundary (same path as a model change).
- A terminal command `anim.play <clip>` and `anim.reload` provide the
  deterministic, no-GUI path used by the dev loop and evidence runs.

## Hot reload and lifecycle

1. Poll `animations.json`, its includes, and `avatar.json` for changes.
2. Parse and validate off the active render state.
3. Atomically swap the avatar's animation set on success.
4. Preserve the previous set on failure and emit a validation record.
5. Rebind animation on spawn, respawn, teleport, map change, model change, and
   avatar change (same boundary as AimBody rebind).

## Diagnostics

All records use `StructuredLogger` and the canonical `events.jsonl`; no
animation-specific file.

- `anim.set.loaded`: avatar, source path(s), include count, clip names, state
  map, result.
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
- clip parse: translation/rotation/scale, per-keyframe interp, loop/duration;
- interpolation: linear/smooth/ease curves at known ticks;
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

1. **Format + registry:** data types (scale, interp, priority), `rig` in
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
