# Avatar Editor and Player-Model Transform Normalization

Date: 2026-10-11
Status: SPECIFICATION / NORMALIZATION PLAN — NOT YET IMPLEMENTED
Owner: `AvatarSystem` owns avatar policy; `Player`/`player-loader` owns GLB
parsing and player-model normalization.

## Purpose

The Avatar Editor lets a developer or player select a rigid multipart `.glb`
body model, preview it, save it in `avatar.json`, and use it in the real game.
The model may have been authored in Blender with unapplied rotations, scale,
non-canonical axes, misplaced object origins, unusual parent/root transforms,
or inconsistent Euler displays. A player model must still enter MiMITA with a
deterministic body frame so animation, weapon attachment, collision, hybrid
AimBody, ragdoll, replay, and rendering agree about the same six parts.

This document defines:

1. the Blender authoring/export contract;
2. the explicit opt-in policy that enables player-body normalization;
3. the exact canonical frame and six-part contract;
4. the normalization algorithm for rotations, scale, location, axes, and
   shoulder/hip pivots;
5. failure, ambiguity, last-good, and backward-compatibility behavior;
6. ownership, configuration, diagnostics, implementation slices, tests, and
   runtime/human acceptance.

The goal is that a creator can export a reasonable six-part character from
Blender without manually producing a new GLB version for every axis or origin
mistake, while the engine remains strict and explainable when the input is too
ambiguous to repair safely.

## Does not

- Normalize arbitrary GLBs loaded as maps, weapons, cosmetics, props, effects,
  destructible objects, or other non-player resources.
- Silently change an existing avatar that explicitly requests authored-frame
  behavior.
- Guess a semantic body part from an arbitrary mesh with no required node name.
- Treat a successful GLB parse, build, or JSON parse as proof of correct visual
  placement or camera aiming.
- Replace Blender's modeling, rigging, UV, material, or texture workflow.
- Retarget skeletal animation, skin weights, armatures, or authored animation
  clips. The first supported player body is rigid multipart geometry: six mesh
  nodes under one player root.
- Resize a character to a universal human height. Normalization may bake scale
  into mesh data and reset node scale, but it preserves the authored body size
  unless an explicit future size policy is selected.
- Apply `config/bodyparts.json` or avatar bodypart overrides to a custom model
  when its transform source is `model`.
- Use a second player-only animation or weapon-aim implementation to compensate
  for an unnormalized asset.

## Historical context

The original informal report was:

> the rotations of the boomer model will not rotate the right way

The important follow-up observation was:

> THE ORIGIN = THE POINT THAT ROTATES AROUND

The current Boomer revisions demonstrated two separate asset problems:

1. some revisions had arm geometry aligned to local Z while the runtime/default
   body frame expects a local Y arm frame;
2. some revisions moved the 90-degree coordinate correction to
   `plrOrigin`, which also rotates child translations, not only visible mesh
   orientation.

Changing Blender's Euler display from XYZ to another order cannot fix a wrong
coordinate basis, a wrong node location, or a mesh origin at the limb center.
The normalizer defined here addresses those as separate transform stages.

## Supported model classes

There are three distinct classes. The loader must know which class is active
before it changes any transforms.

### 1. Default body

No custom `player_model` is selected. The existing default-body path remains
authoritative and continues to use its established compatibility transforms.

`config/bodyparts.json` may continue to own those legacy values.

### 2. Custom player body

`avatar.json.player_model` points below `assets/entity/player/` and the model
contains the six required body-part nodes. This path may use either:

- `authored`: preserve the GLB hierarchy and authored local transforms exactly;
- `player_canonical`: normalize the six-part player body into MiMITA's
  canonical frame before the model reaches animation, physics, or rendering.

The normalizer is available only on this explicit player-body path.

### 3. Other GLB resource

Maps, weapons, cosmetics, effects, props, destructibles, and arbitrary GLB
resources continue through their existing owners. They never call the player
body normalizer merely because they happen to contain a node named `leftArm`.

## `avatar.json` contract

The existing transform-source field remains:

```json
{
  "player_model": "assets/entity/player/monsters/boomer/mimita-boomer-v14.glb",
  "body_transform_source": "model",
  "body_frame_policy": "player_canonical"
}
```

### `body_transform_source`

Exactly two values are supported:

- `model`: use the selected model's body transforms. The default for a newly
  selected custom model is `model`.
- `legacy_overrides`: use the established default-body configuration and
  explicit avatar bodypart overrides. This is for old avatars that intentionally
  depend on the compatibility correction layer.

`body_transform_source` controls whether the old configuration layer applies.
It does not itself select the coordinate normalizer.

### `body_frame_policy`

Exactly two values are supported for a custom player model:

- `authored`: preserve the GLB's local hierarchy as authored. No automatic
  axis, pivot, root, location, or scale normalization is applied.
- `player_canonical`: run the player-body normalizer. The normalizer produces
  an in-memory canonical body representation and records every adjustment.

For a new custom body selected through the Avatar Editor, the editor should
default to `player_canonical` and visibly show that choice. Existing avatars
without this field retain `authored` behavior until the user explicitly opts
into normalization. This prevents an old avatar from changing appearance just
because the loader gained a new capability.

`body_frame_policy` is invalid for a non-player GLB and must be rejected
without changing the last valid avatar.

### Optional normalization controls

The first implementation uses deterministic automatic inference and does not
require per-model numeric tuning. A future explicit override object may be
added only if diagnostics prove that inference cannot resolve a legitimate
model. It must remain owned by `AvatarSystem` and be versioned.

The first planned override shape is:

```json
{
  "body_normalization": {
    "version": 1,
    "forward_axis": "+X",
    "up_axis": "+Z",
    "left_axis": "+Y",
    "part_pivots": {
      "leftArm": "nearest_torso_endpoint",
      "rightArm": "nearest_torso_endpoint",
      "leftLeg": "nearest_torso_endpoint",
      "rightLeg": "nearest_torso_endpoint"
    }
  }
}
```

This is not a license to add arbitrary per-avatar corrections during the first
slice. The normalizer must first prove that its inferred result is wrong and
must report the exact ambiguous stage.

## Blender authoring and export contract

The normalizer is designed to tolerate common transform mistakes, but Blender
exports should still follow this contract. Correct authoring improves confidence
and makes the model useful in `authored` mode too.

### Required object hierarchy

Create one root Empty and six direct child mesh objects:

```text
plrOrigin
├── head
├── torso
├── leftArm
├── rightArm
├── leftLeg
└── rightLeg
```

The names are case-sensitive. Blender duplicate suffixes are accepted only for
the root, for example `plrOrigin.001`. The six gameplay names must remain exact.

Do not put an Armature, Collection Instance, weapon, cosmetic, or helper mesh
between a body part and `plrOrigin` in the first supported format. Extra helper
nodes are allowed outside the six required branches only if they are excluded
from the player-body export.

### Blender coordinate recommendation

Use:

- Blender up: `+Z`;
- MiMITA canonical up: `+Z`;
- MiMITA canonical forward: `+X`;
- MiMITA canonical lateral axis: `+Y`;
- left/right are determined by the named part, not by guessing from mesh color;
- torso center is the body-frame reference;
- arms extend from their shoulder endpoint toward the hand;
- legs extend from their hip endpoint toward the foot;
- head extends away from the neck/torso endpoint.

The exporter must not rely on a 90-degree rotation on `plrOrigin` as a hidden
coordinate conversion. If a conversion is needed, the exporter should either
apply it to the part geometry/node transforms or mark it in the optional GLB
metadata described below. A root rotation is valid input to
`player_canonical`, but it must not be mistaken for a harmless visual-only
rotation because it also rotates child locations.

### Origin recommendation

In Blender, set the object origin for each part to the joint endpoint:

- `head`: neck-side endpoint;
- `leftArm`/`rightArm`: shoulder-side endpoint;
- `leftLeg`/`rightLeg`: hip-side endpoint;
- `torso`: torso center/reference point;
- `plrOrigin`: player root/feet reference used by the game.

The object origin is the intended pivot. The normalizer can repair a misplaced
origin for `player_canonical`, but it cannot infer the creator's intended pivot
if a part is detached, symmetric, or disconnected from its torso relationship.

### Blender transform recommendation

For the clean export path:

1. Select each body-part object.
2. In Object Mode, use `Ctrl+A -> Rotation & Scale`.
3. Confirm object rotation is approximately `0,0,0` and scale is
   approximately `1,1,1`.
4. Do not apply location if location is the intentional joint position.
5. Confirm the mesh itself is positioned relative to the object origin, with
   the joint endpoint at the origin and the limb extending away from it.
6. Select the root and six parts for export.
7. Export one `.glb` with the same axis/up and geometry settings used by the
   project; do not export an animated armature for this rigid-part format.

`player_canonical` must also accept an export where steps 2–5 were not done.
The exporter checklist is a recommendation, not the only way to produce a
loadable model.

### Optional GLB metadata

The preferred exporter/Blender helper should write root `extras.mimita_player`
metadata when available:

```json
{
  "extras": {
    "mimita_player": {
      "schema": 1,
      "body_parts": ["head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"],
      "source_up": "+Z",
      "source_forward": "+X",
      "root_is_player_frame": true,
      "origins_are_joint_pivots": true,
      "transforms_applied": true
    }
  }
}
```

Metadata is evidence and a preference, not an unquestioned authority. The
loader must validate it against the actual node hierarchy and mesh bounds. A
false or contradictory metadata declaration produces a visible validation
warning and prevents silent canonicalization.

### Export validation checklist

Before selecting the file in the editor, the asset validator must check:

- extension is `.glb` and the file is below `assets/entity/player/`;
- GLB magic/version/chunk lengths are valid;
- exactly one usable player root is discoverable;
- all six required node names exist exactly once;
- each required part has one supported rigid mesh primitive;
- POSITION data is finite and non-empty;
- normals, when present, are finite and match the vertex count;
- no required part has a zero or non-finite transform determinant;
- no required part has an extreme scale beyond the configured safety bound;
- no required part has an empty or unreasonably large bound;
- there are no unsupported skin/animation requirements for this rigid format;
- root/part metadata, if present, agrees with the actual model.

The validator must report the exact part and field for every rejection.

## Canonical player frame

The canonical frame is the frame consumed by the existing player, animation,
weapon, collision, and ragdoll systems:

- `+X`: player forward;
- `+Y`: player left/lateral direction according to the named left/right parts;
- `+Z`: world up;
- root translation: player origin/feet reference;
- root rotation: identity relative to the canonical model frame;
- body-part nodes: direct children of the canonical root;
- part local rotation: the bind orientation of that part after normalization;
- part local translation: the joint/reference point relative to the canonical
  root;
- mesh vertices: local to the normalized part node;
- scale: baked into mesh data when required, with node scale reset to one.

The canonical frame does not require all models to have the same dimensions.
It requires the same meaning for forward, lateral, up, joint pivot, and part
local axes.

## Player-body normalization algorithm

Normalization runs once when a model is parsed or rebound, never once per
render frame and never as a second transform layer in the animation loop.
The result is immutable model data copied into each `Player`, just like the
existing parsed-model cache.

### Stage 0: policy gate

1. `AvatarSystem` resolves `player_model`, `body_transform_source`, and
   `body_frame_policy`.
2. Only `body_frame_policy=player_canonical` enters this algorithm.
3. `authored` bypasses it and retains the current authored-transform path.
4. Non-player GLB owners bypass it regardless of node names.

### Stage 1: parse without mutation

Read the GLB into an immutable intermediate representation containing:

- original node names, parent indices, local matrices, and world bind matrices;
- raw mesh POSITION/NORMAL/TEXCOORD/INDEX data;
- primitive and material references;
- accessor bounds and finite-value results;
- optional `extras.mimita_player` metadata;
- a stable source path and content hash.

No source GLB bytes are rewritten during gameplay loading. The normalizer must
be able to reject the candidate while preserving the last valid model.

### Stage 2: discover the root and parts

Find the unique root whose name is `plrOrigin` or `plrOrigin.<digits>`. Find
the exact six gameplay nodes. Reject duplicates or missing parts. Record the
original node index, parent, matrix, and world bind transform for diagnostics.

All six parts must be descendants of the selected root. The first supported
normalization mode requires each part to be a direct child; a future version
may flatten a validated helper chain, but it must not silently do so in v1.

### Stage 3: resolve source basis

Resolve the input basis in this order:

1. validated `extras.mimita_player` basis metadata;
2. an explicit `body_normalization` avatar override;
3. deterministic geometric inference;
4. visible rejection if the result is ambiguous.

Geometric inference may determine a basis only when the torso and named parts
provide enough evidence. It must not decide between mirrored or front/back
interpretations from arbitrary texture colors.

The inferred basis records:

- source up, forward, and lateral vectors;
- root correction quaternion/matrix;
- confidence level: `explicit`, `inferred`, or `ambiguous`;
- warnings and the evidence used.

### Stage 4: bake root and node transforms

Convert every required part's authored world bind transform into the canonical
root frame. The normalizer may:

- absorb a rotated/scaled root into the part bind transforms;
- bake non-uniform node scale into mesh vertices and reset node scale;
- convert quaternion, Euler, or matrix-authored transforms into one canonical
  matrix representation;
- preserve the resulting body size and relative placement;
- reset the normalized root rotation to identity.

It must not apply the root correction twice. A root rotation that is already
baked into child nodes is detected by comparing the original hierarchy and
world bind matrices.

The output of this stage is a canonical bind matrix for each part plus a
canonical mesh-local transform. The original and normalized matrices are both
retained for diagnostics.

### Stage 5: derive each part's geometry axis and pivot

For every part, transform its finite mesh vertices into the canonical root
frame and compute:

- oriented bounds;
- principal long axis using deterministic covariance/PCA or an equivalent
  allocation-bounded method;
- cross-section radii;
- endpoint A and endpoint B along the long axis;
- mesh centroid;
- candidate joint endpoint.

Choose the joint endpoint by the named-part rule:

- arms: endpoint nearest the torso's lateral shoulder candidate;
- legs: endpoint nearest the torso's lower hip candidate;
- head: endpoint nearest the torso's upper neck candidate;
- torso: canonical center/reference point.

The torso candidates are derived from its canonical bounds and the named
part's authored bind position. If both endpoints are equally plausible within
the configured ambiguity tolerance, reject canonicalization with reason
`pivot_ambiguous` instead of silently choosing one.

The normalized part node is placed at the selected joint endpoint. Mesh
vertices are transformed into that node's local frame. This guarantees that a
rotation of the part rotates the limb around the joint rather than around its
centroid.

### Stage 6: derive canonical local axes

The normalized part frame must have:

- the long limb direction extending from local origin toward the hand/foot;
- stable local up/right axes derived from the canonical body frame;
- a right-handed, finite, orthonormal basis;
- determinant near `+1` unless a mirrored mesh is explicitly preserved as a
  mesh-local mirror while the node frame remains right-handed.

The normalizer must not force every part's mesh to use the same local axis if
doing so would alter the visible rest shape. Instead it converts the mesh and
part frame together so the runtime receives the canonical meaning. The output
must expose the derived `long_axis`, `joint_endpoint`, `mesh_local_rotation`,
and determinant for each part.

### Stage 7: normalize scale

Scale is handled per part as follows:

- uniform positive scale is baked into mesh data and node scale becomes one;
- non-uniform positive scale is also baked into mesh data when finite and below
  the safety bound;
- negative scale/mirroring is preserved only when the resulting frame can be
  made right-handed without changing the intended visible side;
- zero, NaN, infinity, or extreme scale rejects the model;
- no global character-size normalization occurs in this stage.

The collider is rebuilt from the normalized mesh bounds. Old default-body
capsule axis/offset values must not override a canonical custom model unless a
future explicit policy says so.

### Stage 8: build the immutable normalized model

Produce one `CanonicalPlayerModel` containing:

- normalized six-part nodes and parent data;
- normalized render meshes and texture/UV references;
- normalized collision meshes and bounds;
- per-part joint/pivot and long-axis data;
- the original source path/hash;
- normalization policy/version and warnings;
- a transform signature used to detect same-name model replacement;
- a `valid` flag and rejection reason.

## Runtime consumers of the normalized result

All consumers use the same normalized representation:

```text
GLB
  -> AvatarSystem policy
  -> PlayerModelNormalizer (only for player_canonical)
  -> CanonicalPlayerModel
  -> render skeleton
  -> procedural animation
  -> weapon/hand attachment
  -> collision and hit bodies
  -> hybrid AimBody/ragdoll
  -> replay and replicated presentation
```

Animation must apply pose rotations to the normalized part node frame. Weapon
attachment must use the normalized right-arm/hand frame. Collision and ragdoll
must use the normalized long axis and derived joint endpoint, not a universal
default axis copied from `config/ragdoll.json`. Replay and remote replicas must
receive the same normalized model identity and transform signature.

There must be one normalization owner. No separate Boomer fix, weapon-specific
90-degree correction, or avatar-name branch is permitted.

## Configuration ownership and reload

`AvatarSystem` owns:

- `body_transform_source`;
- `body_frame_policy`;
- optional future normalization overrides;
- editor selection, persistence, validation display, and policy hash.

`Player`/`player-loader` owns:

- GLB parsing;
- immutable normalized-model construction;
- model cache identity;
- application of the normalized result to a `Player`.

`RagdollModeSystem`, animation, weapons, collision, and replay consume the
result. They do not normalize independently.

For a changed `avatar.json` or changed GLB source:

1. poll the existing avatar hot-reload boundary;
2. parse and normalize off the active render state;
3. validate the complete result;
4. atomically replace the last valid model only after success;
5. rebind the player/replicas at the lifecycle-safe boundary;
6. preserve the previous model on failure;
7. emit the selected, validation, normalization, and runtime records.

The normalizer itself is load/rebind work, not per-frame work. It must not run
inside the fixed-tick collision or render loop after the immutable result is
installed.

## Failure and ambiguity behavior

Reject and preserve the last valid model when any of these occurs:

- invalid GLB container or unreadable file;
- missing/duplicate root or required body part;
- unsupported skin/armature requirement;
- non-finite vertex, normal, node, or scale data;
- ambiguous source basis;
- ambiguous shoulder/hip/head pivot;
- singular or mirrored frame that cannot be represented safely;
- extreme bounds or scale outside safety limits;
- contradictory metadata;
- normalized result fails finite, determinant, or part-separation checks.

The editor shows a human-readable error containing the part, stage, and reason.
The runtime keeps the last valid avatar/model and emits a structured validation
record. It must never silently fall back to `config/bodyparts.json` for a
custom model with `body_transform_source=model`.

## Required structured diagnostics

All records use the existing `StructuredLogger` Avatar category and the active
canonical `events.jsonl`. No avatar-specific debug file is allowed.

### `avatar.model.selected`

One bounded record per selection/load request:

- avatar name;
- source path and normalized repository-relative path;
- content hash;
- `body_transform_source`;
- `body_frame_policy`;
- policy/schema version;
- save/load/apply result.

### `avatar.model.validation`

One record per validation result:

- root name/index and parent result;
- six required-node results;
- primitive/skin/animation results;
- finite/bounds/scale results;
- metadata result;
- rejection reason if any.

### `avatar.model.normalization`

One record per successful or rejected normalization:

- source and output policy;
- confidence (`explicit`, `inferred`, `ambiguous`);
- source basis and canonical basis;
- root input/output matrices;
- scale-bake count;
- warning/rejection list;
- normalized-model hash/signature;
- per-part pivot, long axis, bounds, determinant, input matrix, output matrix,
  and mesh-local correction.

### `avatar.model.runtime`

One record after the preview or real player reaches loaded state:

- executable path/build identity;
- process role and run ID;
- avatar name and model hash;
- normalization policy/signature;
- model-ready result;
- active player/NPC/corpse/replay context.

### `avatar.model.bind.sample`

When hybrid/ragdoll diagnostics are enabled, emit a bounded sample containing:

- lifecycle/avatar/model signature;
- simulation tick;
- normalized joint positions and long axes;
- target, physical, and final orientations;
- weapon grip/muzzle frame for the equipped right-arm weapon;
- first divergence classification if target, physical, and final frames differ.

## Editor behavior

The Avatar Editor's Player body model control must show:

- selected model path;
- model class: `custom player body`;
- transform source: `model` or `legacy_overrides`;
- body frame policy: `authored` or `player_canonical`;
- normalization status: not run, valid, warning, or rejected;
- root and six-part validation;
- per-part pivot/axis summary;
- a button to revalidate/reapply;
- a clear warning when `authored` mode is selected for a model with suspicious
  root rotation, scale, centered limb origins, or axis mismatch.

The preview and gameplay load must use the same normalized model result and
policy. Preview-only corrective transforms are forbidden.

The editor may offer an advanced “export normalized copy” action later. If
implemented, it must write a new `.glb`, never overwrite the source asset, and
must include a report that maps source nodes/meshes to normalized nodes/meshes.

## Blender helper plan

To make the workflow approachable, provide a Blender helper or written export
checklist that performs only authoring-side preparation:

1. validate the six names and direct-parent structure;
2. display each part's object origin and inferred joint endpoint;
3. display the canonical forward/up/lateral axes;
4. warn about unapplied rotation/scale, rotated roots, centered limb origins,
   mirrored transforms, and non-finite/extreme dimensions;
5. optionally apply rotation and scale to mesh data;
6. optionally move the mesh in Edit Mode so a selected joint endpoint is at the
   object origin;
7. write `extras.mimita_player` metadata;
8. export a new GLB without changing the original Blender file unless the user
   explicitly saves it.

The helper must not bake a guessed correction without displaying the chosen
axis, pivot, and basis. The engine normalizer remains authoritative at runtime.

## Tests and validation

### Pure/model tests

These tests isolate mathematical/model invariants and may run without a game:

- root suffix discovery;
- required-node and direct-parent validation;
- quaternion/Euler/matrix equivalence;
- root correction applied exactly once;
- uniform and non-uniform scale baking;
- finite/determinant/extreme-bound rejection;
- PCA/long-axis stability for known boxes/cylinders;
- endpoint selection for arms, legs, head, and ambiguous symmetric meshes;
- left/right named-part preservation;
- mirrored mesh handling;
- normalized output bounds and pivot invariants;
- cache invalidation when source hash or policy changes;
- last-good preservation after malformed or ambiguous replacement.

### Real runtime scenario

For any behavior change or investigation, use the dev-loop workflow:

1. build/publish a fresh executable through `python devscripts/dev-loop.py`;
2. run that exact executable with `--versioninfo` and record its
   `EVENTS_JSONL_PATH`;
3. load a control avatar using `mimita-char-no-animations-v4.glb`;
4. load a deliberately messy Boomer-style model using
   `body_frame_policy=player_canonical`;
5. compare model selection, normalization, runtime bind, camera look, shotgun
   grip/muzzle, movement, hybrid AimBody, and ragdoll records;
6. inspect the active journal for the first divergence;
7. repeat with `body_frame_policy=authored` to prove the policy boundary;
8. test a map/weapon/cosmetic GLB to prove it was not normalized;
9. report build, runtime journal, and human visual acceptance separately.

### Acceptance scenarios

The minimum real scenarios are:

1. Jason/default control: level look, look up, look down, move, fire shotgun.
2. Boomer canonical: same inputs and weapon; arm swings from shoulder and
   shotgun follows the camera/barrel.
3. Boomer authored: no hidden normalization; authored behavior is preserved.
4. Bad export: unapplied root rotation/scale and centered arm origins; canonical
   policy succeeds with a warning if unambiguous.
5. Ambiguous export: symmetric/disconnected limbs; canonical policy rejects and
   preserves the previous valid model.
6. Live replacement: change v13 to a new source hash during the supported reload
   boundary; replicas/rebinds use the new signature without stale transforms.
7. Hybrid/ragdoll: normalized bind frame remains consistent through activation,
   simulation, deactivation, death corpse, respawn, and remote replication.
8. Non-player GLB: weapon/map/cosmetic loading is byte-for-byte unaffected by
   the player normalizer.

## Implementation plan

### Slice 1 — contract and inventory

- Add `body_frame_policy` to `AvatarDefinition` serialization, parsing, and
  validation.
- Define schema/version constants and policy matrix.
- Inventory every active custom player model and classify authored versus
  canonical policy.
- Keep existing custom avatars on `authored` until explicitly migrated.

### Slice 2 — immutable model intermediate

- Refactor the player loader's GLB parse result into an immutable intermediate
  that contains raw nodes, mesh data, bounds, and source hash.
- Preserve the current authored path unchanged.
- Add bounded validation diagnostics before normalization.

### Slice 3 — canonical normalizer

- Implement `PlayerModelNormalizer` under the player-loader/model owner.
- Add root/basis resolution, transform baking, scale handling, axis/pivot
  derivation, and normalized cache identity.
- Do not alter non-player GLB loaders.
- Add pure model tests for every mathematical invariant.

### Slice 4 — consumer convergence

- Make render, procedural animation, weapon attachment, collision, and ragdoll
  consume normalized part frames.
- Remove/default-disable custom-model dependence on universal bodypart axis and
  attachment overrides when `player_canonical` is active.
- Add model-signature-triggered AimBody/ragdoll rebind.
- Keep `authored` as a true no-normalization compatibility path.

### Slice 5 — editor and Blender helper

- Show policy, confidence, warnings, pivots, and axes in the editor.
- Add the Blender validator/export helper or a project-owned checklist.
- Add optional normalized-copy export only after runtime normalization is proven.

### Slice 6 — live proof and migration

- Run the control/Boomer matched scenario through the dev loop.
- Inspect fresh canonical journals and compare the first divergence.
- Visually accept Jason, Boomer, weapon grip/muzzle, hybrid, ragdoll, and
  reload behavior.
- Migrate only explicitly approved avatars to `player_canonical`.

## Completion boundary

The specification is complete when the implementation has:

- one explicit policy owner;
- one player-only normalization owner;
- deterministic source/basis/pivot/axis/scale handling;
- no effect on unrelated GLBs;
- authored-mode compatibility;
- visible editor diagnostics and last-good failure behavior;
- pure model validation;
- fresh runtime JSONL evidence;
- human visual acceptance for control and normalized avatars.

Build success alone is not completion. A normalized model is not accepted until
the real game proves that the visible body, shotgun, camera aim, collision,
hybrid body, ragdoll, and replicated presentation use the same frame.

## Related documents

- `docs/ROUTER.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/asset-checker-v1.md`
- `docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`
- `src/avatar/DESIGN.md`
