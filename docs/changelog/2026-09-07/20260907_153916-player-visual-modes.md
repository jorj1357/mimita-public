# Player visual modes revision

- Timestamp: 2026-09-07T19:39:16Z
- Task: replace the confusing filled expanded-mesh appearance with explicit player visual modes.
- Branch and commit: unchanged; no commit created in this session.

## Pre-existing worktree state

Unrelated edits were already present in `docs/regressions/regressions-v1.md`, `src/combat/weapon-rocket-launcher.cpp`, `src/engine/engine-tick-camera.cpp`, and an untracked replay changelog. Those changes were preserved.

## Implemented

- Replaced the one-line `config/playervisuals.json` with readable documented JSON using `comment_*` fields because strict JSON has no comment syntax.
- Documented and added the modes `none`, `outline`, `capsule`, and `wireframe`.
- Added per-category mode selection for `self`, `enemy`, and `teammate`.
- Changed outline rendering from expanded filled geometry to line rendering with configurable line width, color, alpha, wall visibility, and death behavior.
- Added capsule settings for collision geometry source, scale, color, alpha, depth test/write, wall visibility, and front/back culling. The capsule uses the existing collision capsule plus cylinder/end-cap primitives.
- Added body-part wireframe rendering with configurable line width, color, alpha, wall visibility, and death behavior.
- Extended the hot-reload parser and preserved atomic last-valid configuration behavior.

## Exact configuration examples

- `enemy.mode = "outline"`
- `enemy.mode = "capsule"`
- `enemy.mode = "wireframe"`
- `enemy.outline.thickness`
- `enemy.outline.color`
- `enemy.capsule.frontFaceCull`
- `enemy.capsule.backFaceCull`
- `enemy.capsule.visibleThroughWalls`
- `enemy.wireframe.lineWidth`

Equivalent settings exist under `self` and `teammate`.

## Validation

- `config/playervisuals.json` parsed successfully.
- `git diff --check` completed without content errors.
- `python build_agent.py` completed with `Status: SUCCESS` and relinked the canonical `mimita.exe`.
- No runtime visual acceptance was claimed. Human review is still required for actual outline shape, capsule backdrop appearance, wireframe readability, occlusion, and hot reload while players already exist.
