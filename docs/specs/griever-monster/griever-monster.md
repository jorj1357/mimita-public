relaets to C:\mimita-v9\docs\specs\gamemodes\zombie-tower.md
and C:\mimita-v9\docs\specs\avatar\avatar-editor.md 

# Griever monster specification

**Date:** 2026-10-15  
**Status:** Implementation-planning specification; current code audit included  
**Audience:** MiMITA contributors, AI coding agents, gameplay, physics, networking, animation, and asset contributors  
**Primary question:** Can we implement Griever as a reusable rolling actor whose movement, collision, presentation, and networking are owned by existing shared systems rather than by scattered `monstergriever` special cases?

## 1. Purpose and scope

This document converts the informal Griever idea and the current repository evidence into an implementation contract. It is specific enough that a contributor can identify the owners, make a narrow patch, and prove the result in the real game.

This document covers:

- the player-controlled Griever baseline;
- the future server-controlled Zombie Tower Griever, using the same physical movement executor;
- camera-relative WASD force input;
- fixed-tick movement and collision;
- the rolling spike-ball visual state;
- world collision, actor collision, spike contact, and damage boundaries;
- lifecycle, replication, reconciliation, and server-position evidence;
- configuration and avatar/animation ownership;
- implementation slices and runtime acceptance.

This document does **not** yet finalize:

- the Griever's health, damage amount, attack cooldown, death effects, sounds, loot, or spawn-wave balance;
- a complete Zombie Tower navigation algorithm;
- a new collision solver or a replacement for the existing actor-triangle broadphase;
- cheating policy beyond the movement validation needed for this actor;
- human visual or multiplayer acceptance that has not actually been performed.

Those items must reuse the existing weapon, actor, gamemode, audio, navigation, and lifecycle contracts when their separate specifications are implemented.

## 2. Informal product description

The Griever is a dangerous rolling ball covered in spikes.

When I look in a direction and press WASD, the Griever should receive a force in the corresponding camera-relative direction. W means full camera-forward, S means full camera-backward, A means camera-left, and D means camera-right. Looking down and holding W must apply downward force; looking up and holding W must apply upward force. The force should create physical momentum, not directly teleport the actor toward a target.

The ball should roll visibly as it travels. It should fall under gravity, keep useful horizontal momentum, slide or roll along ordinary ground, bounce from appropriate walls or steep surfaces, and settle instead of gaining height every time its spikes touch the floor. It must not grind forever in a corner.

For Zombie Tower, the Griever's brain chooses a target and desired route. The navigation layer chooses a legal direction. The shared physical movement layer turns that direction into force and collision response. Navigation may ask the Griever to go somewhere; it may not directly write the final position.

The player should feel that the Griever is a real physical actor, while other players should see a stable replicated actor rather than a rubber-banding or server-position-error failure.

## 3. User preferences and non-negotiable feel

The following are product requirements, not implementation suggestions:

- camera-relative WASD is required;
- force is applied in the requested direction, rather than replacing position;
- the visible mesh must tumble, not merely rotate around vertical yaw;
- the actor must not hover, climb forever, or receive an unexplained upward impulse from resting spike contacts;
- ordinary horizontal momentum should not be destroyed by generic humanoid friction;
- the actor must collide with the world using an appropriate physical shape;
- spikes must be able to participate in an explicit damage/contact path;
- the server must know the actor's valid lifecycle and accepted transform;
- avatar identity must select data/configuration, not be used as a scattered gameplay gate;
- client prediction must feel immediate, but the network contract must remain explainable and bounded;
- every failed attempt must identify the first missing stage instead of being described only as “it did not work.”

Things that are explicitly disliked:

- a special `if avatarName == "monstergriever"` branch in several unrelated systems;
- separate movement formulas in the client, server, and physics wrapper;
- a capsule that visually looks like a spiked ball but does not collide like one;
- direct target-position writes masquerading as navigation;
- collision response that launches the actor upward every tick;
- a server that accepts a name but has no matching movement capability, causing huge `server_pos_error` or a launch when switching avatars;
- a passing build or a few good log lines being treated as proof of gameplay;
- synthetic tests being added before the real game path has been traced.

Things that are getting better and should be preserved:

- arbitrary GLB rig parts can now be loaded and bound by exact node name;
- the Griever has a 19-part rig and its render-only spike nodes can be drawn;
- the visible root can accumulate a physical rolling rotation;
- the shared structured logger already records movement, avatar, collision, and server-position evidence;
- the server has lifecycle identity, input sequencing, accepted-transform state, and movement validation infrastructure;
- recent bounded runs showed small server position error and grounded settling.

## 4. Current repository state: confirmed implementation

This section describes the code as audited on 2026-10-15. It is evidence, not a claim that the current implementation already satisfies the target contract.

### 4.1 Avatar data and animation

The active Griever avatar is:

`assets/avatars/monstergriever/avatar.json`

It selects:

- model `assets/entity/player/monsters/griever/mimita-griever-v2.glb`;
- `animations.json` with `idle`, `crawl`, `attack_1`, and `hurt` clips;
- a rig with `core` plus `spike_00` through `spike_17`;
- `movement.mode = "roll"`;
- movement values such as `gravity_scale`, `force`, `mass`, `max_speed`, `restitution`, `radius`, `height`, and `root_offset_z`.

The loader and registry parse the movement capability in `src/entities/player-avatar-animations.cpp` and `src/avatar/avatar.cpp`. The shared capability type is `AvatarMovementCap` in `src/entities/avatar-rig.h`. The animation/rig contract is defined by `docs/specs/animations/animations.md`.

The rig currently marks the 19 Griever parts `physical: false`. That means they are renderable rig parts, but they are not automatically authoritative physical parts for actor collision or damage. This is an important decision point:

1. The actual Griever GLB triangles are the authoritative movement/collision geometry.
2. Any sphere/capsule is only a broadphase or explicit invalid-mesh fallback, never the production replacement for the GLB triangles.

Do not infer gameplay collision from “the renderer can draw the cones.”

### 4.2 Existing local movement paths

There are currently two relevant roll implementations:

1. `src/physics/physics-mini.cpp`, around the `rig.movement.mode == "roll"` branch, applies `dir * (force / mass) * dt`, gravity, a speed clamp, and the shared `doCollisions` path.
2. `src/engine/engine-tick-net.cpp`, in the local multiplayer path, computes a camera basis, applies force and gravity, calls `doCollisions` twice, applies roll-specific vertical cleanup, accumulates root rotation, and writes the predicted position and velocity into the input packet.

The second path currently uses the render-frame `dt` (clamped to 1/30) even though the repository's hard rule requires gameplay collision and physics at fixed 60 Hz. This is a spec-code disagreement that must be resolved before calling the system complete. The desired result is one shared roll movement function called by client prediction and server simulation/validation policy, with fixed-tick gameplay state.

`Player::applyAvatarMovementCap()` in `src/entities/player.cpp` applies the avatar radius/height, roll friction, restitution, and root offset to the player movement state. It also resets roll-only state when switching to a non-roll avatar. This reset behavior is required and must remain lifecycle-safe.

### 4.3 Current network authority

The current roll path is intentionally client-transform-authoritative in the multiplayer code:

- `src/network/movement-validation.cpp` accepts a valid roll report's position and velocity after lifecycle/sequence/finite/bounds checks;
- `src/network/server-players.cpp` does not re-simulate the roll actor and keeps the accepted transform;
- `src/network/multiplayer-reconcile.cpp` skips ordinary drift correction for a roll avatar, while epoch changes still hard-snap spawn/respawn/teleport state;
- snapshots broadcast the server's accepted state.

This is not the long-term ideal for all movement, but it is a coherent current authority choice if it is made data-driven and bounded. It must not be replaced with an ad hoc server bypass. The server still owns lifecycle validity, packet ordering, finite values, world bounds, maximum accepted step, and the final networked state that is broadcast.

The existing networking specification prefers shared client/server movement math and eventually derives authority from input commands. This Griever slice may use client-transform authority only as an explicitly documented transitional contract. A future input-authoritative Griever must preserve the same feel and must use the same roll kernel rather than inventing a second formula.

### 4.4 Collision state today

The repository has:

- a shared collision boundary in `src/physics/movement/physics-collision.h`;
- shared collision response in `src/physics/movement/physics-collision-shared.h`;
- actor/world triangle support in `src/physics/movement/actor-triangle-solver.cpp`;
- actor collision mesh collection in `src/physics/movement/actor-collision-mesh.cpp`;
- cached/broadphase gathering rules in `docs/architecture/collision/collision.md`.

Recent Griever work added extra rig parts to actor collision meshes and enabled triangle movement for rig-only avatars. It also added roll-only response rules:

- no tangential friction for roll contacts;
- no generic embedded-contact upward push for roll;
- up-facing floor contacts project/roll rather than bounce upward;
- restitution comes from the roll capability.

Those changes explain why the latest logs show settling instead of infinite climbing, but they are currently spread through generic player fields and special response conditions. The implementation plan below must decide whether these are general reusable rolling-body semantics or temporary Griever patches.

### 4.5 Existing NPC movement and Zombie Tower intent

`docs/specs/gamemodes/zombie-tower.md` already says that Griever:

- is a rolling ball with spikes;
- selects a target and rolls toward it;
- has physical momentum and bounce/recovery;
- must not become permanently stuck in corners;
- gets desired route/direction from navigation while physics determines actual trajectory;
- must not be directly translated toward its target.

The reusable NPC policy lives in `src/npc/npc-movement-policy.h/.cpp` and the shared NPC movement architecture separates objective, navigation, local intent, shared actor movement, and shared collision. Griever should add a rolling movement capability to that existing pipeline rather than create a Griever-only NPC executor.

## 5. Target architecture

The target data path is:

```text
avatar.json / actor preset / role preset
        ↓
resolved AvatarMovementCap + movement policy
        ↓
input or NPC desired direction
        ↓
camera-relative player intent OR navigation-relative NPC intent
        ↓
shared RollMovementKernel at fixed 60 Hz
        ↓
shared world/actor collision and contact response
        ↓
authoritative MovementState + RollPresentationState + ContactEvents
        ↓                         ↓
network snapshot/report          animation/render/audio/damage consumers
```

The kernel must not know whether the direction came from a keyboard, an NPC brain, replay, or a test. It receives a normalized horizontal intent and the resolved capability/configuration.

### 5.1 Ownership rules

| Concern | One owner | Must not be owned by |
|---|---|---|
| Avatar identity and roll values | avatar rig/config resolver | hardcoded avatar-name branches |
| Player input sampling | input layer | physics or network code polling keys |
| NPC target/route choice | brain/navigation layers | collision solver |
| Force integration | shared roll movement kernel | separate client/server formulas |
| World and actor collision | shared collision owner | renderer or network handler |
| Server lifecycle and acceptance | movement validation/server | local renderer |
| Roll presentation rotation | movement/presentation bridge | animation clip pretending to move physics |
| Spike damage | shared contact/damage owner | arbitrary render-node overlap |
| Diagnostics | `StructuredLogger` | ad hoc files/printf traces |

No implementation slice may add a second state variable when an existing `MovementState`, `AvatarMovementCap`, collision feedback, lifecycle identity, or snapshot field can carry the contract.

### 5.2 Shared roll movement contract

The pure/shared operation should be equivalent to:

```text
next = rollStep(previous, RollIntent, AvatarMovementCap, CollisionFeedback, dt)
```

Where:

- `RollIntent.horizontal` is a normalized or zero X/Y vector;
- `dt` is the fixed simulation timestep, initially exactly `1/60` second;
- `force` and `mass` produce acceleration `force / mass`;
- gravity is applied according to `gravity_scale`;
- speed limits, if enabled, are data-driven and explicit;
- collision feedback contains contacts, normals, grounded state, and velocity response;
- the result contains position, linear velocity, angular/visual rotation input, grounded/contact flags, and emitted contact events.

The kernel must not read camera state, avatar names, network transport, or GLB nodes. Client and server wrappers may choose different authority policies, but they call the same movement and collision meanings.

### 5.3 Camera-forward 3D player input

For player control:

1. Normalize the full camera forward vector, including pitch.
2. Derive right from the horizontal camera forward and world up.
3. Compute `direction = cameraForward * moveForward + right * moveSide`.
4. Normalize diagonal input once so W+D is not stronger than W.
5. Pass that direction as `RollIntent.horizontal`.

For Griever specifically, looking down and holding W must apply a downward
force; looking up and holding W must apply an upward force. A and D remain
horizontal camera-relative side forces. This is intentionally different from
ordinary humanoid movement, where camera pitch is ignored. The capability must
select this 3D roll-input policy; the input rule must not be hidden in a
`monstergriever` name check.

The direction is an input/intent vector. It is not a velocity and must not be
written directly to position. The resulting linear/angular response comes from
the shared physics owner.

### 5.4 NPC target and navigation input

For a server-controlled Griever:

- the brain selects a target according to the existing hostility/targeting rules;
- navigation returns a traversable ground route or local desired direction;
- local avoidance/corner recovery may adjust the desired direction;
- the same `RollIntent.horizontal` is passed to the shared roll kernel;
- physics is allowed to redirect, stop, bounce, or remain blocked;
- after a blocked/redirected contact, navigation reassesses rather than teleporting or repeatedly forcing the same wall-facing direction.

The first Griever NPC slice should use a bounded local direction and explicit corner recovery. Full multi-floor pursuit is a later slice and must not be pretended complete because a target was selected.

## 6. Collision design decision

### 6.1 Required gameplay body

The Griever needs a stable authoritative body built from the actual rolling
spike-ball GLB. The active collision representation must provide:

- the exact GLB triangles for the core and spikes;
- a broadphase extent derived from the GLB bounds;
- center/origin semantics documented in the avatar asset;
- swept movement so high-speed travel cannot tunnel through thin walls;
- fixed-tick world collision;
- actor collision using the established actor manifold/broadphase.

The model origin, movement body center, and spike-tip extent must be measured from the actual GLB. `root_offset_z` must not be used as a blind tuning knob to hide a mismatch between render origin and collision origin.

### 6.2 Authoritative collision is the actual Griever GLB triangle mesh

The actual `assets/entity/player/monsters/griever/mimita-griever-v2.glb`
geometry is the authoritative Griever collider. The sphere/capsule values may
remain as a broadphase, startup-safety, or invalid-mesh fallback, but they must
not replace the GLB triangles in the active Griever collision path.

The required path is:

1. `src/entities/player-loader.cpp::appendNodeCollider` imports each GLB
   POSITION triangle into `Collider::triangles`.
2. `Player::updateModelWorldTransforms()` records previous/current transforms
   for the core and every spike part.
3. `collectActorBodyCollisionMeshes()` in
   `src/physics/movement/actor-collision-mesh.cpp` collects the actual part
   triangles, including `physicalBody.extraParts`.
4. `doGLBTriangleCollisions()` dispatches the active actor-triangle path.
5. `runActorTriangleCollisionStep()` gathers cached world candidates and calls
   `collectActorMeshContactsInto()` and `solveActorTriangleCollision()`.
6. The shared response owner applies depenetration, grounding, sliding, and
   bounce from the resulting contact manifold.

The implementation must prove that this is the path actually used by Griever at
runtime. It is not enough that the loader can parse triangles or that
`extraParts` exists in memory. Contact records must identify the GLB part and
triangle or a bounded equivalent.

The representation must preserve exact GLB triangle positions and winding,
node/part identity, previous-to-current swept transforms, cached broadphase
pairs, exact narrowphase contacts, and a stable mapping from contact part to
spike/body gameplay label.

The sphere/capsule is only a safety fallback before valid GLB geometry exists.
That fallback must emit a diagnostic and appear in runtime evidence.

### 6.3 Contact response requirements

For a rolling body using the actual GLB triangles:

- floor contact removes velocity into the floor but does not inject an upward impulse merely because a spike tip is embedded by the collision skin;
- tangential contact retains momentum according to rolling friction config;
- wall/steep-surface restitution is controlled by the capability/config, not a hidden global constant;
- many same-tick spike contacts are batched/deduplicated before root response, so 18 spikes cannot create 18 independent launches;
- a resting body remains grounded and does not gain height over time;
- a high-speed body uses the cached broadphase and bounded substeps;
- no per-query heap allocation is introduced in the fixed-tick hot path;
- the final pose does not fall back to a capsule solver merely because the GLB mesh has many triangles.

Any corner recovery must be an explicit bounded state/event, for example:

```text
blocked_contact persists for N fixed ticks
→ choose a new legal tangent/route direction
→ optionally apply a configured recovery impulse
→ record recovery reason and result
```

It must not be an unconditional “bounce upward every contact” rule.

### 6.4 Reuse the existing player bounce path

Griever bounce must reuse the existing player/world collision-response meaning,
not invent a second bounce system. Trace this complete path before changing it:

```text
Griever GLB triangle sweep
→ actor/world contact manifold
→ response-normal selection
→ respondVelocityAgainstNormal()
→ rolling friction/restitution policy
→ later grounding/floor-recovery writes
→ final velocity committed to MovementState/player
```

Relevant owners include `physics-collision-shared.h`,
`physics-collision-glb-main.cpp`, `actor-triangle-solver.cpp`,
`config/collision.json`, and the existing player bounce/contact diagnostics.

The plan must identify every later writer that can overwrite bounce, including
floor recovery, projection, safety recovery, grounding, and roll-specific
vertical clamps. A bounce log is not proof if a later phase sets `vel.z = 0`
or adds another correction. The final owner must produce one bounded response
per fixed tick from the selected contact manifold.

Required behavior:

- a flat floor produces the same supported/grounded behavior as a player body;
- a wall or steep surface reflects/slides according to shared player bounce
  policy and Griever restitution;
- an angled surface preserves its oriented normal rather than forcing every
  bounce straight upward;
- floor support does not repeatedly bounce a resting actor;
- contact stacking cannot produce slow upward hover;
- the response records incoming velocity, selected normal, restitution/friction,
  response velocity, and final velocity after later movement phases.

### 6.5 Spike damage boundary

“Touching the spikes causes damage” means a gameplay contact event, not a renderer overlap. The eventual contact event must include:

- attacker actor/life identity;
- target actor/life identity;
- contact tick and source tick;
- spike/body feature or contact label;
- contact point and normal when available;
- relative velocity/impact speed;
- configured damage and knockback request;
- deduplication key for repeated overlap in the same tick/window.

The server remains authoritative for damage and knockback. Client prediction may show a provisional effect, but cannot finalize health.

## 7. Networking and lifecycle contract

### 7.1 One resolved capability per life

At spawn, respawn, and avatar change, both client and server resolve the same avatar rig movement capability. The resolved capability must be observable in the lifecycle/debug record, including:

- avatar ID/name;
- movement mode;
- radius/height/origin policy;
- force/mass/gravity/restitution/speed policy;
- movement capability/config hash if available;
- spawn generation and transform epoch.

The server must never enter the roll branch merely because a string matches `monstergriever`. A generic `mode == "roll"` capability is the owner. If a temporary compatibility gate is unavoidable, it must be one documented compatibility boundary with a removal condition, not four copies in engine, server, validation, and reconciliation.

### 7.2 Transitional client-transform authority

Until input-authoritative server simulation is implemented, the accepted contract is:

1. owning client predicts the shared roll kernel locally at fixed 60 Hz;
2. client sends input and the current predicted transform/state using existing movement sequencing and lifecycle identity;
3. server validates player identity, life/epoch, sequence freshness, finite values, world bounds, maximum elapsed step, and configured speed/step limits;
4. server accepts or rejects the report and stores the accepted transform;
5. server broadcasts the accepted state to remote clients;
6. local reconciliation does not erase valid roll prediction every frame, but epoch changes, rejected reports, teleports, respawns, and invalid states still resynchronize;
7. every rejection has a structured reason and the before/report/accepted values needed to find the first divergence.

The accepted transform must never be an unrestricted trust path. “No rubber banding” means valid predicted movement is not repeatedly corrected; it does not mean the server stops validating.

### 7.3 Required future authority migration

The preferred final contract is input-authoritative server movement:

```text
same initial state + same ordered input + same capability/config
→ same shared roll kernel on server and client
```

That migration is a separate phase. Do not mix it into collision tuning. The current spec must remain honest about which phase is implemented.

### 7.4 Server-position error acceptance

For a healthy local-player run:

- `hasLocalServerPosition` must become true after spawn/lifecycle handshake;
- the Griever must not remain frozen solely because the initial transform was not installed;
- `server_pos_error` must remain bounded and must not grow monotonically during ordinary valid movement;
- spawn/respawn/teleport epoch changes may hard-snap by contract;
- a report rejection must identify its reason rather than look like a physics failure.

The same handshake scenario must be run with a normal walking avatar as a control. If both fail, the result is a lifecycle/network issue, not evidence that Griever physics is wrong.

## 8. Configuration and asset contract

The active avatar remains the data entry point. A future cleaned-up schema may look like this, while preserving existing names during migration:

```json
{
  "rig": {
    "movement": {
      "mode": "roll",
      "gravity_scale": 3.0,
      "force": 5000.0,
      "mass": 10.0,
      "max_speed": 999.0,
      "restitution": 0.9,
      "radius": 3.5,
      "height": 0.0,
      "origin_policy": "center",
      "world_friction": 0.0,
      "floor_bounce": 0.0,
      "wall_bounce": 0.9
    }
  }
}
```

The exact final field names require an implementation decision because the current `AvatarMovementCap` does not yet contain all of these values. Do not silently add a parallel JSON file. Extend the existing owner or record a schema migration.

Asset acceptance requires:

- every declared node resolves by exact name;
- model origin and spike extent are recorded;
- render-only versus physical parts are deliberate;
- the rolling root transform is applied before per-part world transforms;
- switching from Griever to a humanoid resets root rotation, root offset, and movement body size;
- the animation state reports `idle` when stopped and `crawl`/the selected roll locomotion clip when moving, without making the animation clip own physics.

## 9. Implementation plan: narrow slices

Do these slices in order. Each slice must preserve unrelated working-tree changes and end with source/build/runtime evidence before the next slice.

### Slice 0 — freeze the contract and inspect owners

- confirm active avatar/config paths;
- identify all current roll/name gates with repository search;
- confirm the actual GLB triangle path and define only the permitted safety fallback;
- record the authority phase: transitional client-transform authority or input-authoritative server movement;
- add no gameplay code.

Exit condition: this document and the chosen collision/authority decisions are explicit.

### Slice 1 — one shared fixed-tick roll kernel

Likely owners:

- shared movement types near `src/physics/movement/`;
- `src/physics/physics-mini.cpp` as orchestration only;
- server movement orchestration in `src/network/server-players.cpp`;
- existing movement validation/report types.

Work:

- move force/gravity/speed/roll response math into one reusable function;
- make both local prediction and server simulation call it where their phase requires simulation;
- eliminate render-frame gameplay collision from the roll path;
- preserve camera input conversion outside the kernel;
- make `dt = 1/60` explicit and test high render FPS against the same result.

Exit condition: identical input/config/initial state produces identical kernel state in a pure/component check, and the real client journal shows fixed-tick roll records.

### Slice 2 — data-driven capability and lifecycle resolution

- replace repeated avatar-name gates with `AvatarMovementCap.mode` and a resolved capability/authority policy;
- resolve on initial spawn, respawn, and avatar switch on both sides;
- reset roll-only presentation/body state on non-roll bind;
- log capability ID/hash, life generation, transform epoch, and owner.

Exit condition: Griever rolls and Jason walks after switching in both directions, with no launch and no stale roll rotation.

### Slice 3 — stable spherical world body

- implement the actual GLB triangle body semantics in the shared collision owner;
- verify radius, origin, floor support, wall sweep, and thin-wall behavior;
- remove generic upward embedded-contact behavior from the rolling response;
- keep broadphase caching and allocation rules intact.

Exit condition: a stationary Griever remains at a stable height; floor contact does not increase Z; wall contact redirects/bounces according to config; a high-speed sweep does not tunnel.

### Slice 4 — rolling presentation

- derive root angular rotation from translational travel and radius;
- apply root rotation before rendering declared rig parts;
- ensure the renderer uses the per-part path for the 19-part Griever;
- keep animation clips for spike motion/locomotion presentation only.

Exit condition: structured pose/render evidence shows changing position and changing quaternion on multiple axes, followed by human visual confirmation.

### Slice 5 — explicit spike contact and damage

- select and implement the GLB-triangle spike contact/damage labels from Section 6;
- create contact events through the existing actor/damage owner;
- deduplicate repeated contact damage according to the chosen design;
- server validates and applies health/knockback; client presents feedback.

Exit condition: controlled player-versus-Griever contact proves the correct target loses health exactly once per intended contact policy, with no render-only false positives.

### Slice 6 — NPC Griever intent

- add a role/behavior preset that identifies rolling locomotion without a Griever-name branch;
- reuse target selection, navigation, local sensing, and blocked behavior;
- feed desired direction into the same roll kernel;
- add bounded corner recovery and reassessment;
- keep server simulation authoritative for NPCs.

Exit condition: a spawned NPC Griever reaches a legal target through a simple route, bounces/recovers from a wall, and does not oscillate forever in a corner.

### Slice 7 — multiplayer acceptance and authority migration

- run server plus two clients with the real dev loop;
- validate local prediction, remote interpolation, lifecycle changes, and controlled packet delay/loss if supported;
- only then decide whether to migrate from accepted transforms to input-derived server simulation.

Exit condition: eight-player target is a later scale test; first prove one server plus two clients, then increase population while monitoring correction, tick rate, and collision budget.

## 10. Diagnostics and golden path

Use the existing `StructuredLogger`; do not create a second debug file. All records are bounded and gated by the existing categories/configuration.

The Griever golden path is:

```text
avatar/config loaded
→ capability resolved for life
→ input or NPC intent created
→ camera/navigation direction converted
→ fixed-tick roll step entered
→ force/gravity integrated
→ world/actor collision queried
→ contact response applied
→ position/velocity/rotation committed
→ client report or server state accepted
→ snapshot broadcast
→ local/remote presentation updated
```

At minimum, records need:

- `griever.capability.resolved`;
- `griever.intent`;
- `griever.roll.step`;
- `griever.collision.contact`;
- `griever.network.accepted` or `griever.network.rejected`;
- `griever.reconcile`;
- `griever.render.pose`.

Each record includes life ID/generation, actor ID, tick, expected/actual values, and a correlation ID when crossing subsystems. Log every 30–60 ticks or on state change/error, not every render frame forever.

## 11. Validation protocol and attempt history

Gameplay validation must use the real executable and dev loop described by `docs/workflows/runtime-scenario-validation.md`:

1. reuse the repository dev loop and publish a newly identified executable;
2. run that exact executable with `--versioninfo`;
3. capture its `EVENTS_JSONL_PATH`;
4. run a bounded real scenario;
5. inspect the journal for the first missing or divergent stage;
6. repeat with a normal walking avatar as control;
7. separate build evidence, runtime evidence, and human acceptance.

Recommended scenario:

```text
ticks 1-60: no input; verify spawn, grounded state, and no upward drift
ticks 61-120: hold W with camera yaw = 0
ticks 121-180: rotate camera 90 degrees and hold W
ticks 181-240: hold D; verify camera-relative right
ticks 241-300: release input; verify momentum/friction policy
ticks 301-360: drive into a wall at speed
ticks 361-420: rest on floor; verify stable Z
ticks 421-480: switch Griever → Jason → Griever
```

Required controls:

- Jason with the same spawn/handshake path;
- Griever with movement mode resolved;
- a flat floor, a wall, and a corner;
- an open area for sustained movement;
- the same executable identity and journal path for all claims.

### Historical attempt matrix

| Attempt/evidence | Result | What it proved | What it did not prove |
|---|---|---|---|
| 1: model/rig work | model loaded after parser/render fixes | 19 rig parts resolved and rendered | movement, collision, visual acceptance |
| 2: input path | movement command began reaching the actor | input/position path could move | correct rolling, server authority |
| 3: force roll | body translated and server accepted state | force-driven movement existed | client immediacy, spike collision |
| 4: collision/replication fixes | grounded state and small error in bounded runs | current path can settle and replicate | universal network health, open-space feel |
| triangle/contact experiments | spike geometry can enter actor collision path | solver can see extra parts | that full mesh collision is the right production model |
| render-root fix | multiple part quaternions changed | mesh can tumble in renderer | human visual acceptance |

The historical notes in the original scratchpad (“crashed,” “would not move,” “moved but would not roll,” “hovered,” and “server position error”) are retained here as a failure vocabulary. Future attempts must add the exact first divergent stage and journal path instead of only incrementing an attempt number.

## 12. Reuse the proven projectile/object networking patterns

The Griever should reuse existing ownership patterns instead of creating a new
network protocol.

### 12.1 Grenade-launcher reference

The grenade launcher is the closest working reference for a moving,
server-spawned, network-visible gameplay object. Inspect and reuse its existing
boundaries:

- `src/combat/weapon-grenade-launcher.cpp` owns local firing/prediction only;
- the server projectile owner accepts the attack and creates the authoritative
  projectile;
- projectile packets carry identity, definition, transform, velocity, and
  lifecycle events;
- clients adopt/reconcile predicted projectiles through the existing reliable
  result plus snapshot/event path;
- detonation/area effects are server-owned and use existing damage ownership.

For Griever, this means the network design must have one authoritative actor
identity, spawn/lifecycle event, transform/state update, and rejection/acceptance
record. Do not make avatar loading itself responsible for transport.

### 12.2 `object_spawn` reference

`object_spawn` is useful as the geometry/collision reference:

- `src/terminal/object-commands.cpp` loads a closed GLB;
- the GLB becomes local collision triangles rather than a guessed capsule;
- the physical-object owner integrates gravity, velocity, angular velocity, and
  world contacts at fixed tick;
- its triangle mesh and orientation are retained as one object state.

However, the current moving-physical-object specification explicitly says that
the physical-entity snapshot/impulse packet owner is not complete. Therefore,
reuse `object_spawn` for GLB triangle import and physical-body semantics, but
reuse the grenade/projectile path for network lifecycle design. Do not copy an
unfinished object network path and call Griever networking solved.

### 12.3 Griever network plan

The implementation plan is:

1. prove the Griever avatar/capability is resolved on client and server;
2. prove the same lifecycle handshake used by a normal player succeeds;
3. trace the existing accepted-transform report and snapshot path;
4. add Griever-specific fields only where the existing movement report cannot
   carry full 3D velocity, angular/roll state, or GLB collision evidence;
5. keep the server-position state updated from the accepted report and emit
   the before/report/accepted values;
6. test a normal Jason avatar as a control before changing Griever physics;
7. only after lifecycle/network health is proven, tune triangle contacts and
   bounce.

The first failure in the user's current report is networking/lifecycle, not
proof that the GLB collision solver is wrong.

## 13. Separate Jason ragdoll/head investigation

This is related to shared avatar physics but is not a Griever requirement and
must not be fixed by changing Griever collision code.

The current configuration provides several possible head-motion owners:

- `config/ragdoll.json` has `physical.limits.head_max_swing_deg = 45`;
- the head attachment currently has a very permissive cone and per-axis
  rotation range (`999`-style values), so the attachment may not be the tight
  limit;
- `config/aimbody.json` is currently in `hybrid` mode;
- `src/ragdoll/ragdoll-mode-config.cpp` parses physical aim gain, damping,
  angular speed, and head swing limits;
- `src/ragdoll/ragdoll-mode.cpp` applies the physical body/aim path and logs
  target, physics, and final head/torso rotations.

Do not assume “ragdoll.json head range” is the cause. The investigation must
compare, for Jason:

1. camera look direction;
2. head target orientation;
3. physical head orientation and angular velocity;
4. final rendered head orientation;
5. the configured head swing and attachment limits;
6. whether hybrid pose springs or a later animation write changes the result.

If the measured head swing exceeds the intended range, the smallest likely
configuration candidate is `physical.limits.head_max_swing_deg`, but the exact
owner must be proven from `AIMBODY_LIVE_SAMPLE` before changing it. Record this
as a separate Jason ragdoll attempt, not as Griever acceptance.

## 14. Acceptance criteria

### Source/architecture

- no repeated Griever-name gameplay gates remain outside the avatar/config compatibility boundary;
- one shared roll movement meaning exists;
- physics/collision are fixed-tick;
- navigation never directly writes the final physical position;
- collision uses the cached broadphase and introduces no hot-loop allocations;
- the actual Griever GLB triangle mesh is the authoritative movement collider;
- any sphere/capsule fallback is explicit, temporary, and visible in evidence.

### Runtime

- capability resolves on spawn, respawn, and avatar switch;
- W/A/S/D follows the full camera basis for Griever: W/S include pitch, A/D remain horizontal side forces;
- force is applied and position changes through physics;
- release follows the configured momentum/friction behavior;
- the body rests on a floor without increasing Z;
- wall and corner behavior is bounded and observable;
- the visible mesh tumbles on multiple axes;
- server position is installed and remains bounded during valid movement;
- invalid lifecycle/sequence/finite/bounds reports are rejected with reasons;
- remote clients receive the accepted actor state.

### Damage/NPC, when those slices are implemented

- spike contact uses a gameplay contact event and server-authoritative damage;
- repeated overlap follows the explicit deduplication policy;
- NPC target/navigation intent uses the same roll kernel;
- NPC Griever recovers from a simple corner and does not teleport through walls.

### Human acceptance still required

No source search, build, JSONL record, or self-test alone proves:

- that the Griever visually looks like a convincing rolling spiked ball;
- that the controls feel good at normal and high render FPS;
- that bounce/friction feels correct;
- that two clients see stable, readable motion;
- that the Zombie Tower encounter is fun or scary.

Those are separate human-review claims and must be recorded as such.

## 15. Related authority

- `docs/specs/movement/movement.md` — fixed-tick movement and shared movement contract;
- `docs/architecture/collision/collision.md` — broadphase, exact contacts, performance, and collision ownership;
- `docs/specs/animations/animations.md` — rig, movement capability, clips, arbitrary parts, and rendering;
- `docs/specs/networking/networking.md` — prediction, snapshots, reconciliation, and authority direction;
- `docs/specs/gamemodes/zombie-tower.md` — Griever role and rolling navigation intent;
- `docs/architecture/player-npc-systems/npc-movement.md` — objective, navigation, intent, shared movement, and collision separation;
- `docs/specs/debug-logging/debug-logging.md` — structured bounded diagnostics;
- `docs/workflows/runtime-scenario-validation.md` — real executable evidence.

## 16. Attempt log — fill this in during implementation

Use one entry per implementation/runtime attempt. Keep the wording concrete:
what was tried, what visibly happened, what the journal proved, and what the
first divergence was. Do not call a build a feature success.

### Attempt 1 — current reported baseline, 2026-10-15

2026 10 15 1521 serv pos errors sometimes , like greiver wont have serv pos replication , and still hoevering up slowly 

**Status:** Failure; user report recorded before a fresh controlled rerun.  
**Avatar loading:** The Griever avatar appears to load correctly, but this is
tentative until a fresh executable records capability/rig/GLB collision evidence.  
**Observed networking problem:** The server position does not update correctly;
the Griever has a large or persistent `server_pos_error`. The actor may be
moving locally without the server maintaining the corresponding authoritative
position.  
**Observed physics problem:** The Griever still slowly hovers/climbs instead of
sitting on the ground.  
**Desired input correction:** Camera forward must be the force direction,
including pitch. Looking down and holding W must apply downward force.  
**Desired collision correction:** Griever must use the actual GLB triangles and
reuse the existing player bounce response, while preventing repeated resting
contacts from adding upward velocity.  
**Reference systems requested:** Reuse the proven grenade-launcher network
lifecycle and the `object_spawn` GLB triangle/physical-object path where their
ownership is actually complete.  
**First suspected divergence:** Not yet proven. Start with capability/lifecycle
resolution and accepted server-transform records before changing collision
response.  
**Required next evidence:** Fresh executable identity, `EVENTS_JSONL_PATH`,
client/server capability records, movement report accepted/rejected records,
`server_pos_error`, GLB triangle contact labels, and final velocity after all
collision/floor-recovery writers.

att 2 - good rolling but doesnt bounce, needs to bounce a lot lot lot 
we also need to 