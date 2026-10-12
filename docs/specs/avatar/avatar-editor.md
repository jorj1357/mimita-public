2026 10 11 1717 jorj todo 
the  rotations of the boomer model  "C:\mimita-v9\assets\entity\player\monsters\boomer\mimita-boomer-v7.glb"
i even put it to be XYZ in blender
and rotate and stuff
but it wont  rotate the right way

havent applied like  scale or offsets

ok im gonna do hmmmm
apply rotaiton s

2026 10 11 1720 ITS BC  
U HAVE TO APPLY ROTATIONS
AND ALSO DO ORIGIN
THE ORIGIN = THE POINT THAT ROTATES AROUND Mkay
so
set the origins to be right
and make rotiatons applied to be 0 
scale is fine
offset its fine

2026 10 11 1811 jorj so 
griver is just  aball
it rolls at u
so its like
head orso left rigt arm?
n
no no rellly
but the wasp too
left rig tharm?n 
no its wings 
so
ueah 
need like ball 
ragdoll modw or ball mode idk


# Avatar editor and authored GLB workflow

Date: 2026-10-11

## Purpose

The Avatar Editor lets a developer or player create an avatar from an
`avatar.json`, preview it, save it, and test it in the real game. A custom
character exported from Blender as a `.glb` is also an avatar body model. The
editor must preserve the transforms authored in that GLB so the preview and
the game use the same body layout.

This specification covers the editor's custom body-model workflow, including
GLB discovery, selection, loading, body-part validation, transform ownership,
preview, save/load, and live diagnostic evidence.

## Does not

- Replace Blender's export settings or define a Blender rigging tutorial.
- Reinterpret mesh vertices, silently retarget a skeleton, or invent a body
  layout when the GLB contains one.
- Remove the legacy default-body configuration used by the default model.
- Make cosmetic GLBs into body models; cosmetics remain separate attachments.
- Claim that a model is correct from a successful build or a JSON parse alone.

## User goal

The current practical goal is:

1. Export a monster character from Blender as a GLB.
2. Put the GLB under `assets/entity/player/...`.
3. Select it in the Avatar Editor's Player body model control.
4. Preview it with the avatar's textures and cosmetics.
5. Save the selected model in `assets/avatars/<name>/avatar.json`.
6. Start the real game with that avatar and verify that the body is in the
   same position, orientation, scale, and part relationships as in Blender.

The first target models use body-part nodes named exactly:

```text
plrOrigin
head
torso
leftArm
rightArm
leftLeg
rightLeg
```

`plrOrigin` may be named `plrOrigin`, `plrOrigin.001`, `plrOrigin.914`, or
another Blender duplicate suffix. The loader must accept the suffix for root
discovery, while the six gameplay body-part names remain exact unless a
future explicit node-mapping feature is specified.

## Problems and their causes

### 1. Two transform authorities move the same body

Blender stores each node's authored translation, rotation, and scale in the
GLB. MiMITA also has `config/bodyparts.json` defaults and an optional
`avatar.json.bodyparts` override object. Applying both to a custom model moves
the body twice and makes the editor disagree with Blender.

### 2. Default-body values are not custom-model values

`config/bodyparts.json` contains placement values for the default body model.
Those values are compatibility configuration, not a universal correction for
every character. They must not be applied to a custom GLB unless the avatar
explicitly opts into the legacy override behavior.

### 3. Root naming is too rigid for Blender exports

Blender can produce duplicate names such as `plrOrigin.001`. Treating only
the exact name `plrOrigin` as the root causes authored root transforms and
ragdoll/hybrid bind discovery to be lost or misidentified.

### 4. A preview success can hide a runtime failure

The editor may load a model while gameplay still uses a stale avatar, a
failed async model load, missing body parts, or a second transform layer.
The exact executable and canonical `events.jsonl` must be used for runtime
proof.

## Transform contract

`avatar.json` may contain:

```json
{
  "player_model": "assets/entity/player/monsters/boomer/mimita-boomer-v6.glb",
  "body_transform_source": "model"
}
```

`body_transform_source` has exactly two supported values:

- `model`: use the GLB node transforms as authored by Blender. Do not apply
  `config/bodyparts.json` or `bodyparts` to the six body nodes. This is the
  default whenever `player_model` is a custom GLB.
- `legacy_overrides`: apply the established default-body configuration and
  then the avatar's explicit `bodyparts` values. This exists only for old
  avatars that intentionally depend on the old correction layer.

When no custom `player_model` is selected, the default body continues to use
the default-body configuration. When `body_transform_source` is invalid, the
avatar load must keep the last valid avatar state and emit a structured
validation event; it must not silently guess.

The `model` path is authoritative for position, orientation, scale, and root
frame. `avatar.json` remains authoritative for texture selection, colors,
cosmetics, and the explicit transform-source choice. The editor may display
diagnostic values, but it must not write generated offsets into `bodyparts`
when the source is `model`.

## GLB import and validation behavior

Selecting **Apply body model** must:

1. Accept only a regular `.glb` below `assets/entity/player/`.
2. Save a repository-relative normalized path.
3. Load the node hierarchy and retain every node's authored local transform.
4. Accept a root named `plrOrigin` with an optional dot-and-number suffix.
5. Require `head`, `torso`, `leftArm`, `rightArm`, `leftLeg`, and `rightLeg`.
6. Reject the model with a visible editor error and a structured reason when
   it is missing, unreadable, outside the asset root, or missing required
   parts.
7. Apply the same model and transform-source policy to the preview and the
   next gameplay load; no manual restart-only correction is allowed.

Validation must report model path, root node, required-node results, source
(`model` or `legacy_overrides`), and whether any override layer was applied.

## Save/load behavior

- Loading an avatar restores its model path and transform source before
  applying it to the preview.
- Saving preserves the selected model path and never converts model-authored
  transforms into guessed offsets.
- Existing avatars without `body_transform_source` retain legacy behavior if
  they use the default model; custom-model avatars resolve to `model`.
- A custom model with `legacy_overrides` must visibly identify that choice in
  the editor so an accidental old correction is explainable.
- Replacing or removing the body model clears stale per-model preview state.

## Runtime ownership and data path

```text
Avatar Editor selection
  -> AvatarSystem save/load and transform-source resolution
  -> Player::loadModel / async model-load handoff
  -> GLB node hierarchy and authored rest transforms
  -> body-part validation and physics/render skeleton
  -> preview or real player
  -> StructuredLogger events.jsonl
```

`AvatarSystem` owns avatar data and policy. `Player`/`player-loader` owns GLB
loading, node hierarchy, and application of the selected transform source.
Ragdoll and animation consume the resulting skeleton; they do not add a second
custom-avatar placement correction.

## Required diagnostics

Use the existing `StructuredLogger` category `Avatar`. Bounded events must
cover:

- `avatar.model.selected`: avatar, path, source, and save/load result;
- `avatar.model.validation`: root and six required-node results;
- `avatar.model.transforms`: one record at model application/rebind with
  source, override count, and authored root/body-part transforms;
- `avatar.model.runtime`: one record after the real player or preview reaches
  the loaded state, including executable identity and avatar name.

These events must be in the active run's canonical `events.jsonl`; no avatar
local debug file is allowed. They are evidence for the data path, not a
replacement for human visual acceptance.

## Acceptance criteria

### Editor acceptance

- A Blender-exported GLB with `plrOrigin.001` and the six exact body-part names
  can be selected and previewed.
- The preview uses the GLB-authored body transforms with zero applied legacy
  overrides.
- Save, reload, and re-apply produce the same model path and transform source.
- An invalid or incomplete GLB is rejected with a useful visible reason.

### Runtime acceptance

- A fresh real executable loads the same avatar and emits the model path,
  source, root, required parts, and override count in `events.jsonl`.
- For `body_transform_source=model`, the journal proves
  `override_count=0` and the authored transforms were retained.
- The human sees the monster body in the intended Blender-authored placement
  in the editor and in gameplay.
- If the avatar is used in hybrid/ragdoll mode, the same root and body-part
  frame remains valid there; this requires separate visual acceptance.

## Implementation slices

1. Make this transform contract explicit and route it through synchronous and
   asynchronous model loading.
2. Add suffix-tolerant root validation and bounded structured model evidence.
3. Update the editor's model status/error presentation.
4. Exercise a real monster GLB through the dev loop and inspect its fresh
   `events.jsonl` before tuning anything else.

## Related documents

- `docs/specs/entity-editor/entity-editor.md` when that specification exists.
- `src/avatar/DESIGN.md` for the existing texture/avatar format.
- `docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md` for the
  avatar evidence pattern.
- `docs/specs/debug-logging/AI-fix-live.md` for implementation and runtime
  investigation procedure.
