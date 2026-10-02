# Girl avatar rotation and hybrid aimbody investigation

Time (UTC): `2026-10-02T01:32:03Z`
Branch: `afad20a-rebuild`
Status: investigation only; no gameplay or asset files changed

## Question

Investigate why the girl avatar can appear rotated differently from the
default avatar, especially with `config/aimbody.json` set to hybrid mode.

## Confirmed source and asset evidence

- Active `config/aimbody.json` selects `"mode": "hybrid"` and
  `"arms_mode": "hybrid"`.
- `assets/entity/player/default/mimita-char-no-animations-v4.glb` has an
  identity `plrOrigin` and a 90-degree X rotation on each body-part node.
- `assets/entity/player/girl/mimita-girl-v7.glb` has identity body-part nodes
  but a 90-degree X rotation on `plrOrigin`. This is the same coordinate
  correction stored at a different hierarchy level, not proof by itself of a
  bad asset.
- `assets/avatars/demongirl/avatar.json` selects `mimita-girl-v7.glb` and adds
  explicit per-part overrides. Its `rightLeg` override is especially notable:
  rotation `[0, 180, 0]` and scale `[-1, 1, 1]`. The other offsets also differ
  from the default body-part layout.
- The active loader applies the global `config/bodyparts.json` transforms and
  then replaces matching transforms with the avatar overrides in
  `src/entities/player-loader.cpp:186-290`. Avatar selection supplies those
  overrides through `src/avatar/avatar-atlas.cpp:769-802`.

## Hybrid path that can expose a frame mismatch

- `RagdollModeSystem::initParts()` uses the post-loader skeleton world
  transforms to derive a canonical yaw-only physics frame and stores the
  residual model transform in `part.meshLocal`
  (`src/ragdoll/ragdoll-mode.cpp:691-783`).
- Hybrid captures the current procedural pose before physics at
  `src/ragdoll/ragdoll-mode.cpp:325-341`, physically simulates the body, then
  applies the hybrid arm correction at `src/ragdoll/ragdoll-mode.cpp:375-402`.
- The physical body writes the visible skeleton through
  `syncToPlayer()`/`syncAimToPlayer()`, while the torso orientation becomes the
  model root rotation (`src/ragdoll/ragdoll-mode.cpp:1434-1478` and
  `src/entities/player.cpp:157-190`).
- Therefore a transform that is harmless in the normal procedural renderer can
  become visibly wrong in hybrid if the GLB root correction, avatar override,
  `meshLocal`, and physical torso frame do not agree.

## Current conclusion

The repo supports the user's recollection, but it does not yet prove one
single root cause for the reported visual. The strongest concrete suspects are:

1. The girl GLB moves the 90-degree coordinate correction to `plrOrigin`,
   while the default GLB stores it on every body part.
2. The girl avatar overrides replace the standard rest transforms and include a
   special mirrored/180-degree right-leg transform.
3. Hybrid is active and derives physical bind/render transforms from the
   resulting skeleton, so it can reveal a root/mesh-local mismatch or make the
   right-leg override visibly dynamic.

The earlier investigation did not produce a focused runtime transform dump, so
the exact rotated node remains unproven. Triangle count is not an orientation
proof.

## Evidence limits

- No source files, configs, or assets were edited in this investigation.
- No build was run.
- No live client visual acceptance or runtime transform log was captured.
- The current working tree contained unrelated pre-existing modifications; they
  were preserved.

## Best next diagnostic

Run one controlled A/B with the same player pose and capture, for both default
and demongirl: effective `plrOrigin` world quaternion, each body-part node
world quaternion, `part.meshLocal`, physical body orientation, and final
skeleton world quaternion. Compare hybrid against `aimbody` default. This will
separate GLB/override composition from hybrid physics composition before any
fix is selected.

