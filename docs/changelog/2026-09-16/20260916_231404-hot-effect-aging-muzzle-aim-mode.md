# Steps 5-7 — hot effect aging, muzzle, and aim mode

- UTC timestamp: 2026-09-16T23:14:04Z
- Branch: `8292026stash`
- Commits: none (working tree; concurrent unrelated edits preserved)
- Result: `PASS` (hot build SUCCESS; all selftests PASS on the seam EXE)
- Hot DLL: `build/mimita-game.dll` (65 sources)

## Task

Use the seams installed in steps 1-4 to make effect aging, the fire-origin
muzzle, and the aim mode hot-editable with no cold build. (Step 6 included: it is
the muzzle computation the under-ground fire origin needed.)

## Step 5 — hot effect aging (`hot.effect-age`)

- New `src/hot-reload/modules/presentation/effect-age.cpp`: a client-only fixed
  60 Hz system that ages the existing surface-decal and blood pools, fades alpha
  over the tick, and kills on expiry; it claims aging via `effect.pool` so the
  cold pool stops aging them. All curves/values are in this `.cpp`.
- Part / effect-particle aging stays on the existing hot `gameUpdateEffects` hook
  (`src/effects/effect-part.cpp`), which is already hot-editable.

## Step 6 — hot muzzle

- `attachment.cpp`: `writeToolPresentation` stores the recipe `muzzleOffset`;
  `attachmentTick` computes `muzzleWorldPosition` + `forward` from the resolved
  tool transform and writes them to the attachment state.
- `weapon-system.cpp` (`hotResolvedMuzzle`) already reads them for the shot /
  tracer / muzzle-flash origin (step 2 seam), so the bullet now leaves the visible
  gun. Grip/muzzle stay tunable live via the recipe.

## Step 7 — hot aim mode

- New `src/hot-reload/modules/presentation/aim-mode.cpp`: publishes the hot-owned
  aim mode (`g_aimMode`, default `"crosshair"`) into `GameSharedStateV1.aimModeHash`
  each client tick; the cold `computeAim` reads it instead of
  `config/gameplay.json`. Includes an `aimmode <mode>` command to switch at
  runtime. Edit the string in the `.cpp` or use the command — no JSON, no rebuild.

## Validation

- `python build_game_dll.py` -> success (65 sources).
- `mimita-20260916T190703.exe --hot-combat-selftest` PASS; `--live-code-selftest`,
  `--production-loop-selftest`, `--glb-consumer-selftest`,
  `--tool-entity-continuity-selftest`, `--content-resource-selftest` all PASS.
- `git diff --check` clean.

## Live confirmation still required

- Fire the weapon: tracer/muzzle flash start at the gun (no under-ground).
- Effects: decals/blood fade over their lifetime at 60 Hz; z-fighting stable.
- `aimmode physical` vs `aimmode crosshair` changes where shots go, live.
- Edit `effect-age.cpp` / `tool-visuals.cpp` grip+muzzle / `aim-mode.cpp` and see
  the change in the same running world.

## Notes / limitations

- Part/effect-particle aging is hot but still driven with frame dt via
  `gameUpdateEffects`; decals/blood are now fixed 60 Hz. Making parts 60 Hz needs
  either the cold call-site moved to the client tick or part fields exposed in the
  pool ABI (small follow-up).
- The muzzle is resolved in the render domain, so the fire path uses the previous
  frame's muzzle (one frame). Visually negligible.
