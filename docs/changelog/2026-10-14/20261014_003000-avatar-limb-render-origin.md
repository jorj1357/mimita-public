# Avatar limb render origin fix

Time: 2026-10-14T02:11:42Z

## Task

Investigate why `mimita-stabber-v7.glb` rendered with arms and legs displaced
from the torso and head, and why the limbs rotated about their middle instead of
the shoulder/hip, then fix it following `docs/specs/debug-logging/AI-fix-live.md`.

## Routed documents read

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/debug-logging/AI-fix-live.md` (avatar transform evidence profile)
- `docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`
- `docs/architecture/player-npc-systems/` (ragdoll/aim body ownership)
- `docs/changelog/2026-10-14/20261014_001800-avatar-root-bind-alignment.md`
- `docs/changelog/2026-10-13/20261014_000500-avatar-shoulder-pivot-live-fix.md`
- `docs/regressions/README.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`

## Root cause (two defects)

`meshLocal = inverse(bodyBindWorld) * nodeWorld`, so `meshLocal[3]` is the
authored COM-to-node-origin bind translation (shoulder/hip). The capsule end
nearest the parent is a collision-artifact point near the middle of the limb.

1. `RagdollModeSystem::visualPartOrigin` built attached part render origins from
   `childLocalAnchor`, not `meshLocal[3]`, displacing the mesh.
2. The derived `childLocalAnchor` was the capsule end, so the joint solver
   pinned the limb at its middle; the authored shoulder/hip orbited that point,
   which is why limbs rotated about their middle.

Evidence (`logs/2026-10-14/20261014_001808/events.jsonl`,
`AIMBODY_AVATAR_BIND_PART`): origin error
`|child_local_anchor - mesh_local_translation|` = 0.20 (head), 0.86 (arms),
2.02 (legs). The previous fix's `final_node_pivot_error ≈ 0` was circular.

## Source changes

File: `src/ragdoll/ragdoll-mode.cpp`

- `visualPartOrigin` returns `position + orientation * meshLocal[3]` for every
  part (root and attached); the `childLocalAnchor` branch is removed.
- `initParts` derives the default attachment anchor from the child's authored
  node origin (`child.body.position + child.body.orientation * meshLocal[3]`)
  instead of the capsule end nearest the parent.
- `logAimTransformSnapshot` (`AIMBODY_AVATAR_BIND_PART`) records
  `expected_visual_pivot_world_position` and `bind_node_pivot_error`
  (rendered origin vs imported rest node position) as a non-circular check.

Physics bodies, collision capsules, COM offsets, network state, `partHandWorld`,
and `solveJoints` anchor math are unchanged; only the anchor value and the
render origin now use the authored node origin.

## Pre-existing edits preserved

- Uncommitted `config/analytics.json` `launch_count` bump — untouched.
- Uncommitted `20261014_001800` torso-only render branch — superseded, not
  reverted.

## Build and runtime evidence

- Builds published through the reused dev loop: `2251` (render-origin fix),
  `2258` (joint-anchor fix). Source generation current.
- Live run: `C:\mimita-v9\logs\2026-10-14\20261014_003832\events.jsonl`.
- Executable: `C:\mimita-v9\.dev\builds\2258\mimita.exe`; server/client build
  identities matched.
- Scenario via the shared command path: `avatar.load monsterstabber`,
  `agent.snapshot`, `agent.aim 1 0 0.2`, `agent.snapshot`.
- `bind_node_pivot_error ≈ 1e-5` for head, both arms, and both legs.
- `|rendered node - torso parent anchor|` mean (build 2258) = 0.02 arms /
  0.02 legs / 0.04 head, down from 0.86 / 1.19 / 0.20 (build 2251). The rendered
  shoulder/hip is now the pinned joint, so the limb hinges about it.
- Full-ragdoll corpse path stable: `ragdoll.corpse.pose_step_trace` parts finite
  and clustered, no crash, client uptime stable.

## Separate claims

- Source change: applied at the single owner.
- Build: `2251` and `2258` published successfully.
- Runtime journal: fresh run proves bind origin and joint pivot both use the
  authored node origin.
- Human visual acceptance: NOT performed this session.

## Regression

Recorded: `docs/regressions/2026-10-14/avatar-limb-render-origin-REG.md`
(`ATTEMPTED FIX (3)`, pending human visual confirmation).

## Human review still needed

Confirm in-game that `monsterstabber` arms and legs hinge at the shoulder/hip
(not the middle), align with the torso and head, and that the weapon mount still
looks correct at several camera pitches.

## Follow-up: Blender node-name suffix tolerance

A second defect in the same session: a GLB whose body-part nodes use Blender
duplicate suffixes (`plrOrigin.091`, `head.001`, `torso.184`, `leftArm.002`,
`rightArm.003`, `leftLeg.004`, `rightLeg.005`) did not work; it required exact
base names. `isPlayerBodyPart` used exact string equality, so canonicalization
was rejected (`reason=root_or_required_parts`) and no part identity matched.

### Source changes

- `src/entities/player.h`, `src/entities/player-loader.cpp`: add
  `playerBodyPartBaseName` (strips a trailing `.<digits>`); use it for
  `isPlayerBodyPart`, `partIndices`, stored `BodyPart`/`PhysicalBodyPart`/
  `Collider` names, bodyparts.json and per-avatar override lookups, and the
  async loader path.
- `src/avatar/avatar.cpp`: the editor skeleton override match
  (`setPartTransform`) compares base names.
- `src/network/server-body-template.cpp`: add the same suffix strip to
  `isBodyPartName` and body-part classification.

### Evidence

- Build `2268`, run `C:\mimita-v9\logs\2026-10-14\20261014_010035\events.jsonl`.
- A suffixed copy of `mimita-stabber-v7.glb` loaded as a temporary avatar:
  `avatar.model.normalized-node` reported six `normalized` records plus
  `success` (`root_index=6`) instead of a rejection, and
  `bind_node_pivot_error ~1e-4` for all six parts.
- Temporary avatar directory and GLB removed after the test; `monsterstabber`
  (exact names) re-verified with `~1e-4` error.

Recorded regression:
`docs/regressions/2026-10-14/avatar-node-suffix-detection-REG.md`.

## Data-driven avatar animation — first implementation slice

### Spec and routing

- Added `docs/specs/animations/animations.md` (authoritative): rig contract,
  `animations.json` schema, shared clip library, state machine, layers, runtime,
  arbitrary-part rendering, live editing, hot reload, diagnostics, replication,
  movement coupling, tests, acceptance, slices.
- Added a router row and a linking section in `docs/specs/avatar/avatar-editor.md`.

### Decisions applied

- New spec doc; shared `config/animations/library/` library; rig combined into
  `avatar.json`; target the full flying monster; start with animation states plus
  a simple gravity/hover flag; the same clips drive players and NPCs.

### Source changes

- `src/entities/avatar-rig.h` (new): `AvatarRigPart`, `AvatarMovementCap`,
  `AvatarRig`.
- `src/entities/player.h`: `AnimInterp`, `AnimKeyframePart.scale`,
  `AnimKeyframe.interp`, `AnimClip.priority/blendInTicks/blendOutTicks`,
  `AvatarStateMap`, `AvatarAnimationSet`, `PhysicalBody.extraParts/extraPartMeshes`,
  `PendingPlayerModel.rigExtraNodes`, Player fields (`animationSet`, `rig`,
  `lockedClip`, `mAnimResolvedAvatar`), `playOneShot`.
- `src/entities/player-avatar-animations.{h,cpp}` (new): per-avatar rig + clip
  registry with `include` resolution (file or directory), global fallback,
  hot-reload polling, `anim.set.loaded` structured event.
- `src/entities/player-animation-config.{h,cpp}`: public `parseAnimClipJson`
  (scale, per-keyframe `interp`, priority/blend).
- `src/entities/player-animation.cpp`: per-avatar set resolution, generalized
  clip lookup, easing, scale channels, state machine (idle/locomotion walk-fly-
  crawl-swim/return_to_idle) + one-shot lock (attack/hurt/death), animate
  render-only rig parts.
- `src/physics/physics-types.h`: `ProceduralPose.scale`.
- `src/physics/movement/physics-gravity.cpp`: per-avatar `gravity_scale`/`hover`.
- `src/entities/player-loader.cpp`: build/cache/copy render-only rig parts;
  cache key includes the rig; async path carries rig nodes.
- `src/entities/player.cpp`: update extra-part world transforms.
- `src/entities/player-render.cpp`, `player-render-mesh.cpp`: draw extra parts;
  VAO pool raised to 64.
- `src/avatar/avatar.{h,cpp}`, `avatar-atlas.cpp`: parse/serialize `rig`; feed
  render-only rig nodes to the loader.
- `src/avatar/avatar-commands.cpp`: `anim.play <clip>`, `anim.reload`.

### Build and runtime evidence

- Builds `2273`/`2274` published successfully through the reused dev loop.
- Fresh run `C:\mimita-v9\logs\2026-10-14\20261014_020934\events.jsonl`.
- A temporary avatar (`zzflytest`) declared `rig.movement.mode=fly`,
  `gravity_scale=0`, `hover=true`, and an `animations.json` that `include`s a
  shared library clip. The journal emitted `anim.set.loaded` with
  `clip_count=4`, `movement_mode=fly`, `rig_parts=6`, `valid=true`, and the
  model still resolved `body_frame_policy=player_canonical`.
- `monsterstabber` (no rig, no animations.json) still loads and normalizes
  unchanged — backward compatibility holds.
- Temporary avatar + test library clip removed after verification.

### Separate claims / not yet done

- Source: compiles and the registry/state-machine path runs (journal evidence).
- Arbitrary render-only part rendering (wings) is implemented and compiles but
  was NOT verified with a real extra-node GLB in this session.
- Human visual acceptance of a flying monster: NOT performed.
- Editor animation panel, replication/NPC parity evidence, and full flight
  steering remain (slices 2/4/5/6 in the spec).

## Follow-up: griever + wasp rigs, and two parser/loader fixes

Time: 2026-10-14T02:33:57Z

### Authored avatars

- `assets/avatars/monstergriever/avatar.json` + `animations.json`: the griever
  GLB uses arbitrary nodes (`Sphere.055`, `Cone.042`..`Cone.059`), so a `rig`
  maps `core` + `spike_00..17` (19 render-only parts) with `movement.mode=crawl`
  and idle/crawl/attack_1/hurt clips.
- `assets/avatars/monstermutantwasp/avatar.json` + `animations.json`: canonical
  six parts declared with `movement.mode=fly`, `gravity_scale=0`, `hover=true`;
  clips idle/fly/attack_1/hurt, with `fly` resolved from the shared library clip
  `config/animations/library/wing_flap.json` via `include`.

### Fixes found during verification

- `player-loader.cpp`: extra rig-part lookup now prefers an exact node-name match
  before base-name matching, so distinct duplicate nodes (`Cone.042` vs
  `Cone.043`) map correctly instead of collapsing to `Cone`.
- `avatar.cpp`: the synchronous `loadAvatar` parser (used by `avatar.load`)
  never parsed `rig`; added a shared `parseAvatarRigJson` used by both the sync
  loader, the per-instance parser, and the animation registry, and made it reset
  the rig so repeated loads do not accumulate parts.
- `player-render.cpp`: the per-part draw path now also activates when only
  render-only rig parts exist (an avatar with no canonical six parts).
- Added bounded diagnostics `anim.rig.parts` (declared/resolved) and
  `anim.set.bound` (per avatar-set bind).

### Runtime evidence

- Builds `2278`/`2282`/`2285` published through the reused dev loop; run
  `C:\mimita-v9\logs\2026-10-14\20261014_023133\events.jsonl`.
- Griever: `anim.set.loaded` (clip_count=4, movement_mode=crawl, rig_parts=19,
  valid=true), `anim.rig.parts` declared=19 resolved=19 (core, spike_00..),
  `anim.set.bound` set_valid=true. Client remained alive while rendering the
  19-part model.
- Wasp: `anim.set.loaded` (clip_count=5, movement_mode=fly, rig_parts=6,
  valid=true) and `anim.set.bound` set_valid=true; `wing_flap` resolved from the
  shared library.
- The registry is exercised for many other avatars (`angel2`, `headlessredux`,
  ...) with no crash and `set_valid=false` fallback for those without files.

### Still not verified

- Locomotion state selection (`crawl`/`fly`) while moving: the local player's
  velocity stayed 0 during `agent.move` in this sandbox run (a separate
  movement/command issue), so `active_animation` stayed `idle`. Not a claim.
- Human visual acceptance of the griever spikes or wasp flight.

## Follow-up: movement input, arbitrary parts, spawn avatar, editor tab

Time: 2026-10-14T03:05:21Z

### Fixes

- `src/input/input-poll.cpp` + `.h`: a scheduled `agent.move` set
  `movementPressed`/`movementJustPressed`, but the following keyboard-only
  assignment overwrote them (so movement reported pressed=false). Now the
  scheduled flag is combined with keyboard state, with a `justStarted`
  out-param for the edge. The scheduled move was also consumed twice per frame
  (`pollInput` and `engineTickNet`); `pollInput` now peeks and `engineTickNet`
  owns the single consume.
- `src/entities/player-loader.cpp`: `appendGenericRenderParts` loads every
  remaining mesh node (arbitrary names such as `Cone.042`, `Sphere.055`) as a
  render-only animated part, so an avatar without the six canonical names is a
  real, rendered, animatable body instead of falling back to the default model.
  Applied only when the model has no canonical parts or declares a rig, so
  existing six-part avatars are unchanged. Exact-name rig matching was added
  earlier so duplicate nodes map correctly.
- `src/render/render-player.cpp`: the local player now adopts
  `GetPlayerSettings().avatarName` on first render, so spawn no longer leaves
  the default fallback model ("green capsule") until a manual load.
- `src/avatar/avatar.cpp` + `avatar-atlas.cpp`: bind the per-avatar rig and
  animation set at avatar-apply/finalize time (not only in the procedural-
  animation call path), so the visible player has its movement capability and
  clips even on frames where animation does not run.
- `src/main-systems.cpp`: `agent.snapshot` now records `on_ground`,
  `stable_on_ground`, `has_world_contact`, `size_scale`, `model_loaded`,
  `freeze_active`, `procedural_frozen`, `ragdoll_active`, `movement_mode`,
  `anim_clip_count`, and `server_pos_error` (schema 3).
- `src/entities/player-animation.cpp`: emit `anim.state.changed` (from/to clip,
  moving, movement mode, velocity) — the moving-state snapshot evidence.
- `src/avatar/avatar-editor-animation.{h,cpp}` (new): data-driven Animation tab
  listing the current avatar's rig parts/roles, clips (with Play), states, and a
  Reload action; registered as the sixth editor tab (`avatar-editor.cpp`,
  `config/gui/avatar-creator.json`).

### Runtime evidence

- Builds `2296`/`2301`/`2305` published through the dev loop; runs
  `20261014_024652`, `20261014_025815`, `20261014_030308`.
- `agent.move.applied` now shows non-zero velocity (e.g. `[40,0,0]`) and the
  player position changes across the move for `jason` and `monstergriever`.
- `anim.state.changed` shows `monstergriever` `idle -> crawl` while moving
  (`movement_mode=crawl`) and back to `idle` when stopped.
- After the apply-time bind, the local player snapshot reports
  `movement_mode=crawl` and `anim_clip_count=4` for `monstergriever`.
- `anim.rig.parts` declared=19 resolved=19 for the griever.

### Investigated but NOT fixed (not avatar-specific)

- In some dev-loop runs the client never receives the local authoritative spawn
  (`server_pos_error=-1`, `hasLocalServerPosition=false`), so the player is
  frozen at the initial position with zero velocity and cannot move. This
  reproduces identically for `jason` and `monstergriever`, so it is a
  client spawn/reconciliation state, not the griever model. In a healthy run
  (`20261014_025225`) both avatars moved and `server_pos_error` was < 0.01 m.
  This remains open.
- Human visual acceptance of the griever spikes, wasp flight, and the editor
  Animation tab.

## Follow-up: force-driven "roll" movement mode

Time: 2026-10-14T03:33:51Z

### Feature

`rig.movement.mode == "roll"` turns an avatar into a physical body driven by a
force in the look direction (WASD = force along camera forward/strafe), with
gravity and the shared swept collision. New `AvatarMovementCap` fields:
`force` (N), `mass` (kg; acceleration = force/mass), `max_speed`, and
`restitution`. The griever (`assets/avatars/monstergriever/avatar.json`) is set
to `mode=roll, force=50, mass=2, max_speed=999999, restitution=0.6`.

### Source changes

- `src/entities/avatar-rig.h`, `player-avatar-animations.cpp`, `avatar.cpp`:
  new roll fields parsed/serialized.
- `src/physics/physics-mini.cpp`: roll branch (client/local path) — force +
  gravity, speed clamp, `doCollisions` integrate/bounce.
- `src/network/server.h`, `server-players.cpp`: `ServerPlayer.movementCap`
  resolved from the avatar rig at spawn; server roll branch (authoritative).
- `src/input/input-poll.cpp`: `pollInput` now owns the single consume of a
  scheduled move (its output feeds the InputPacket), and `engine-tick-net`
  peeks. Previously the wish was consumed before the packet was built, so the
  server never received `agent.move` (server wish stayed 0).
- Bounded diagnostics `roll.client` / `roll.server` (every 30 ticks).

### Runtime evidence

- Builds `2314`/`2317` published; runs `20261014_032622`, `20261014_033139`.
- `monstergriever` snapshots report `movement_mode=roll`, `server_pos_error`
  ~0.02 m (server agrees).
- `roll.server` shows the wish reaching the server (`wish [0,1]`) and the body
  gaining velocity and translating (`vel [0,0.83,1.93]`, position changing),
  i.e. the authoritative roll works.

### Known limitations (not fixed)

- The client does not run local roll prediction in this multiplayer path
  (`roll.client` never fires); the visible player follows the server, so input
  feels server-authoritative.
- Roll velocity does not accumulate to the configured speed because the
  server's per-tick wish from the dev-loop burst is intermittent and the
  collision resolve damps it; held WASD in real play applies a continuous
  force.
- The griever's collision is still the default capsule, not a spike-ball
  radius, so "bouncing off the spikes" is not yet modeled.
- Human visual acceptance of rolling/bouncing.

## Follow-up: roll regression fixes, client prediction, spike collision

Time: 2026-10-14T04:07:56Z

### Regression fixes

- `src/engine/engine-tick-net.cpp`: the scheduled-move peek (previous follow-up)
  left the move un-expired when `pollInput` did not run, so players could not
  stop moving forward / could not air-strafe. Reverted to the original
  double-consume (`pollInput` + `engineTickNet`), keeping only the
  `movementPressed` combination fix.
- `src/network/server-players.cpp` + `server.h`: the server resolved
  `ServerPlayer.movementCap` only at spawn, so switching from the griever (roll)
  to `jason` mid-life left the server running the roll branch for jason and
  launched it (server_pos_error > 300 m). The cap is now re-resolved whenever
  `avatarName` changes. Verified: `jason` `server_pos_error` 0.005-0.01 m,
  grounded; no launch.

### Client-side roll prediction (A)

- `src/engine/engine-tick-net.cpp`: when the local player's
  `rig.movement.mode == "roll"`, the roll force + gravity + collision are
  simulated locally each frame and the packet carries the predicted
  position/velocity. Non-roll avatars are untouched. Verified: the griever
  position changes (`[16.99,-30.07,84.57] -> [17.10,-29.94,84.56]`).

### Spike-ball collision (B, partial)

- `src/entities/player-loader.cpp`: render-only/generic rig parts now also
  build real collision triangles (`appendNodeCollider`).
- `src/physics/movement/actor-collision-mesh.cpp`: `collectActorBodyCollisionMeshes`
  now includes `physicalBody.extraParts`, so a spike-ball avatar's cones/sphere
  participate in triangle collision.
- `avatar-rig.h` + parse/serialize + client bind: new `radius`/`height` on
  `AvatarMovementCap`; the griever sets `radius=3.5` so its movement capsule is
  a ball. `monstergriever/avatar.json` movement now includes these.

### Known limitations

- True player-vs-world triangle-vs-triangle collision is not wired: the
  player's world movement uses the movement capsule, so B currently makes the
  griever a large ball (and adds real triangles for actor-vs-actor collision)
  rather than a literal per-spike solve.
- Client roll prediction is partially overwritten by `mpReconcileLocalPlayer`
  each frame, so the observed dev-loop speed stays low (~1 m/s) with the burst
  command; making roll client-authoritative is the next step. Held WASD applies
  a continuous force.
- Human visual acceptance still required.

## Follow-up: client-authoritative roll + true triangle collision

Time: 2026-10-14T12:25:43Z

### 1. Roll is now client-authoritative

- `src/network/movement-validation.cpp`: after the ownership/lifecycle/sequence
  checks, a `roll` avatar's reported position/velocity is accepted directly
  (like the ragdoll root-authoritative path), with an 8 m/report step clamp so a
  bad tick cannot teleport it.
- `src/network/server-players.cpp`: the server roll branch no longer
  re-simulates; it keeps the client's accepted transform.
- `src/network/multiplayer-reconcile.cpp`: drift correction is skipped for roll
  avatars so the local prediction is not overwritten (epoch hard-snap still
  applies for spawn/teleport).
- `src/engine/engine-tick-net.cpp`: the local roll prediction is the authority;
  a vertical response clamp (`-60..+12` m/s) limits spike-ball ejection.

### 2. True player-vs-world triangle collision

- `src/physics/movement/actor-triangle-solver.cpp`: `runActorTriangleCollisionStep`
  now accepts actors whose geometry is in `extraParts` (rig-only avatars).
- `src/entities/player-loader.cpp`: render-only/generic parts build real
  collision triangles.
- `src/physics/movement/actor-collision-mesh.cpp`: extra parts included.
- `src/entities/player.cpp`: `previousWorldTransform` tracked for extra parts
  so the triangle sweep has both poses.
- `src/entities/avatar-rig.h` + parse/serialize/bind: new `radius`, `height`,
  `root_offset_z`; the griever uses `radius=3.5`, `root_offset_z=3.5`.

### Runtime evidence

- Builds `2335`-`2342` published; runs `20261014_120958`-`20261014_122404`.
- `monstergriever` now translates quickly (velocity tens of m/s) with
  `movement_mode=roll` and small `server_pos_error` (~1-3 m) — the server accepts
  the client's transform.

### Known limitation

- The griever's model origin is at the ball center and the spike-ball triangle
  solve ejects it upward continuously (it climbs instead of resting/rolling on
  the ground). `root_offset_z` did not resolve it. This needs model-origin or
  collision-response tuning (next). Human visual acceptance still required.

## Follow-up: camera-relative roll, speed, settling, rolling mesh

Time: 2026-10-14T12:50:58Z

### Fixes

- **Camera-relative WASD**: the roll force now uses the camera basis
  (`forward * wishForward + right * wishSide`) instead of the raw world axes, so
  W moves along the look direction as the camera turns.
- **Roll gated to `monstergriever` only** (client prediction, server branch,
  movement-validation bypass, and reconciliation skip all require the name).
- **Default avatar**: `render-player.cpp` adopts `jason` when no avatar is
  chosen, so the local player is never left as the default fallback model.
- **Speed**: griever `force=60`, `mass=10`, `max_speed=10` → ~10 m/s, heavy.
- **Settling / no infinite climb**: upward velocity from the spike-ball contact
  is damped (`*0.2` airborne, `0` grounded) and clamped, and `root_offset_z=3.5`
  lifts the ball's origin to its bottom. The griever now comes to rest grounded.
- **Replication**: the server accepts the client roll transform (no clamp) and
  broadcasts it; observed `server_pos_error` is now 0.0001-0.016 m (was 13.6k).
- **Rolling mesh**: the model root rotation accumulates
  `angleAxis(speed*dt/radius, up x velocity)` so the visible ball spins with
  travel; `roll.client` now logs per-part world positions and the model-root
  quaternion (bounded, every 30 ticks).

### Runtime evidence

- Builds `2348`-`2350`; runs `20261014_124441`-`20261014_124854`.
- `roll.client` shows `dir` following the camera, `vel` ~10 m/s, part positions
  changing, and `model_root_quat` rotating from identity (e.g.
  `[0.9,0.01,-0.43,0]`), proving the mesh is not stuck to world axes.
- `monstergriever` settles grounded with `server_pos_error` 0.0001-0.016 m.
- `jason` remains `movement_mode=walk`, grounded.

### Still open

- Human visual acceptance of rolling/bouncing; fine tuning of bounce/restitution.
