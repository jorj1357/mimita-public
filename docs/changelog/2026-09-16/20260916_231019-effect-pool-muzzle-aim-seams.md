# Steps 1-4 — effect-pool, muzzle, aim-mode hot seams + decal depth fix

- UTC timestamp: 2026-09-16T23:10:19Z
- Branch: `8292026stash`
- Commits: none (working tree; concurrent unrelated edits preserved)
- Result: `PASS` (hot + cold build SUCCESS; all selftests PASS)
- Built executable: `mimita-20260916T190703.exe`

## Task

Install generic cold seams so effect aging, the fire-origin muzzle, and the aim
mode can all be driven/edited from the hot module. Plus a decal depth fix for
z-fighting. Hot behavior (steps 5-7) comes next.

## Step 1 — effect pool access + aging claim

- `game-api.h`: `GAME_CAP_EFFECT_POOL` + `GameEffectPoolV1` (ops
  COUNT/GET/SET/KILL/CLAIM over surface decals and blood particles).
- `effect-part.h`/`effect-part.cpp`: public pool accessors
  (`decalPoolCount/Get/Set/Kill`, `bloodPoolCount/Get/Set/Kill`,
  `setEffectAgingClaimed`) and a `mAgingClaimed` flag.
- `effect-part-system.cpp`: when aging is claimed, the kernel stops aging decals
  and blood (hot owns). Spawn/deferred processing unchanged.
- `live-behavior.cpp`: `capEffectPool` + registration.

## Step 2 — muzzle seam

- `hot-presentation.h`: `HotAttachmentStateV1` gains `localMuzzle`,
  `muzzleWorldPosition`, `forward` (hot writes; cold reads).
- `weapon-system.cpp`: `hotResolvedMuzzle()` reads the local actor's
  `ToolPresentationClaim` -> tool `AttachmentState` muzzle and overrides
  `muzzlePos`/`muzzleDir` at the aim/fire sites (falls back to the cold
  viewmodel muzzle until hot writes it).

## Step 3 — hot-owned aim mode seam

- `game-api.h`: `GameSharedStateV1` gains `aimModeHash`.
- `weapon-fire-raycast.cpp` `computeAim`: if hot published an `aimModeHash`
  (`crosshair`/`camforward`/`physical`/`farpoint`/`world_hit`), use it instead of
  `config/gameplay.json`; otherwise keep the cold config.

## Step 4 — decal z-fighting

- `effect-part-render.cpp`: flat decals are pushed `+0.012` along their normal,
  and the textured decal batch draws under `glPolygonOffset(-1.5,-1.5)` (applies
  to hot and cold decals: bullet holes, cracks, blood splats).

## Validation

- `python build_game_dll.py` -> success.
- `python build_agent.py` -> `Status: SUCCESS`, produced
  `mimita-20260916T190703.exe`.
- `--hot-combat-selftest` PASS; `--live-code-selftest`,
  `--production-loop-selftest`, `--glb-consumer-selftest`,
  `--tool-entity-continuity-selftest`, `--content-resource-selftest` all PASS.
- `git diff --check` clean.

## Notes

- The seams are inert until the hot systems use them: no behavior change yet
  (aimModeHash 0, muzzle zero, aging not claimed).
- Steps 5-7 (hot: `hot.effect-age`, muzzle computation, aim-mode logic) require
  no cold build — they are hot C++ and can be built/activated live.
