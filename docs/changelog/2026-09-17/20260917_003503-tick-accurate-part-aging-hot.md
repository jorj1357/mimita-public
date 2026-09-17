# Tick-accurate part aging, hot-only (no cold build)

- UTC timestamp: 2026-09-17T00:35:03Z
- Branch: `8292026stash`
- Commits: none (working tree; concurrent unrelated edits preserved)
- Result: `PASS` (hot build SUCCESS; all six selftests PASS on a fresh EXE)
- Executable: `mimita-20260916T203438.exe`

## Task

Make effect/part aging run at the fixed 60 Hz tick rate, editable live, with no
cold rebuild.

## Change (all hot DLL sources)

- `src/hot-reload/modules/presentation/effect-age.cpp`: owns
  `std::uint32_t g_hotEffectTick`, incremented once per client fixed tick by the
  `hot.effect-age` system (which also ages decals and blood; see the previous
  changelog).
- `src/effects/effect-part.cpp` `gameUpdateEffects` (the hot part-aging hook):
  steps parts by the number of elapsed ticks at a fixed 1/60 s instead of the
  render-frame dt. Multiple ticks in one frame are applied as multiple fixed
  steps (clamped to 8 after a stall), and a frame with no elapsed tick ages
  nothing, so aging speed is identical regardless of frame rate. If the hot tick
  is not running (e.g. a headless/server context where `g_hotEffectTick == 0`),
  it falls back to the previous dt-based step.

No ABI change and no cold file touched, so this required no EXE rebuild; both
files are part of the replaceable game DLL (`src/effects/effect-part.cpp` and
`src/hot-reload/modules/presentation/effect-age.cpp`).

## Validation

- `python build_game_dll.py` -> success (67 sources).
- `python build_agent.py` -> `Status: SUCCESS`, `mimita-20260916T203438.exe`.
- All selftests PASS on the fresh EXE: `--hot-combat-selftest`,
  `--live-code-selftest`, `--production-loop-selftest`, `--glb-consumer-selftest`,
  `--tool-entity-continuity-selftest`, `--content-resource-selftest`.
- `git diff --check` clean.

## Note

`mimita-20260916T190703.exe` crashed on `--production-loop-selftest`; the newer
builds (`mimita-20260916T191101.exe`, `mimita-20260916T203438.exe`) pass with the
same DLL, i.e. that EXE was a stale/partial link, not a source defect.

## Live confirmation

- Explosion spheres / sparks / smoke / decals should now age at the same speed
  regardless of frame rate, matching the 60 Hz tick.
- Tail effect lifetimes/gravity/fade in `effect-age.cpp` (decals/blood) and
  `effect-part.cpp` (parts) and confirm live, with no restart.
