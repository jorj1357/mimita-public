# Hot visual seams + tool/effect fixes + data-driven explosion timeline

- UTC timestamp: 2026-09-16T21:03:11Z
- Branch: `8292026stash`
- Commits: none (working tree; concurrent unrelated edits preserved)
- Result: `HOT BUILD PASS; COLD BUILD/SELFTESTS PENDING (mimita.exe running)`
- Evidence class: source + hot-DLL compile only so far

## Task

Make the tool-visual/effect pipeline hot-driven so bugs in this area are fixable
live, and fix the reported issues: wrong/blue weapon texture, rocket model not
rendering, model spinning ~25-57x, stale model after switch/unequip, effects
visible through walls, missing firing sounds, and the cube explosion (replace with
an editable, tick-based, client-only timeline).

## Cold (one build required; installs the hot seams + fixes the base defects)

- `src/render/presentation-render.cpp`
  - `GpuMesh` gains an embedded base-color texture; `loadGlbMesh` extracts the
    GLB's first material `baseColorTexture` image and uploads it. `submitMesh`
    uses `textureResourceId == 0` => the model's own texture (recipes now pass 0).
    Retire deletes it.
  - Multipart non-skinned GLBs now draw each part with its node/bind transform
    (fixes the RPG/other multi-node weapons collapsing to the origin).
  - Added `mesh.hexagon` primitive (registered + applied).
- `src/live-code/live-behavior.cpp`
  - `capEffectPart` maps the new mesh/texture/per-axis-scale fields.
  - `capSocketQuery` converts `TransformComponent.yaw` degrees -> radians (fixes
    the ~25-57x attached-model spin).
- `src/hot-reload/game-api.h`
  - `GameEffectPartV1` gains `meshResourceId`, `textureResourceId`, `scaleXYZ[3]`.
  - New `GAME_DOMAIN_CLIENT_TICK` (client-only fixed 60 Hz).
- `src/effects/effect-part.h` / `effect-part-render.cpp`
  - `EffectPart` gains `meshResourceId`/`textureResourceId`/`scaleXYZ`; the render
    path draws such effects as real meshes, depth-tested, in the world/FBO pass
    (so effects no longer show through walls).
- `src/hot-reload/generic-runtime.cpp` / `src/sim/simulate-tick.cpp`
  - `GAME_DOMAIN_CLIENT_TICK` runs only in the client fixed tick; the server never
    simulates presentation.

## Hot (live, no restart)

- `src/hot-reload/modules/presentation/tool-visuals.cpp`
  - `sounds.fire` fixed to real assets: `revolvershoot`, `shotgunshoot`,
    `rocketlauncher/rocketshoot`, `grenadelauncher/grenadelaunchershoot`,
    `weapon/hafs/hafsswing` (fixes all silent firing sounds).
  - `textureId = 0` for tools => each model uses its own embedded texture.
- `src/hot-reload/modules/presentation/attachment.cpp`
  - Owned-but-unequipped tools lose `PresentationState`/`AttachmentState`, so a
    stale model no longer persists after a switch/unequip.
- `src/hot-reload/modules/presentation/hit-visuals.cpp`
  - New data-driven explosion timeline: `ExplosionLayer` table (start/end tick,
    mesh id, offset + per-tick rise, rotation, radius, per-axis scale, color,
    alpha) + `hot.explosion` system on `GAME_DOMAIN_CLIENT_TICK` +
    `ExplosionState` component. Defaults: ticks 1-2 big red sphere, 3-10 smaller
    darker red, 11-25 smallest/darkest fading to 0, plus a rising gray smoke
    layer. Add/remove layers freely in this `.cpp`. Uses the depth-correct mesh
    effect primitive.
- `src/hot-reload/modules/presentation/effect-composition.cpp`
  - `composeExplosion` now emits sound + the tick timeline instead of blue cubes.

## Validation so far

- `python build_game_dll.py` -> `DLL build success` (hot compile PASS, 62 sources).
- `python build_agent.py` -> **refused**: `mimita.exe` is running; per the live
  invariant no process was killed and no force-cold was used. Cold build +
  selftests still required.

## Outstanding

- Cold build + `--hot-combat-selftest`, `--live-code-selftest`, and the other
  selftests once `mimita.exe` is closed.
- Live acceptance: weapon textures, rocket model, no spin, no stale model,
  firing sounds, explosion timeline + smoke, effects occluded by walls.
- Concurrent sessions are editing `tool-visuals.cpp` / animation code.
