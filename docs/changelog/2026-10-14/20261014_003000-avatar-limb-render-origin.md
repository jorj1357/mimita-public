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
