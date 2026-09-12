# Live-development invariant: cold/live build split, boundary violations, hot rocket policy

- EST timestamp: 2026-09-12 11:25:27 EDT (UTC 2026-09-12T15:25:27Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (automated proofs pass; in-game live proof needs a display session)

## Task

Enforce the invariant: if `MiMITA.exe` is running it must remain running. Split
COLD BUILD from LIVE BUILD, never overwrite loaded DLLs, detect
`HOT_RELOAD_BOUNDARY_VIOLATION`, move actively developed rocket policy behind
the hot ABI, and prove the loop without relinking.

## The root problem removed

`build_agent.py` force-killed every running `mimita.exe` before linking:

```python
# Auto-kill running mimita.exe so linker can overwrite it
subprocess.run(["taskkill", "/f", "/im", "mimita.exe"], ...)
```

That is the architectural error. It is deleted.

## What changed

### Step 1 — tooling, invariant, docs
- `build_agent.py`: removed the kill; added `mimita_is_running()` and
  `enforce_live_invariant()`. While `mimita.exe` runs it refuses to link and
  prints `HOT_RELOAD_BOUNDARY_VIOLATION`; `MIMITA_FORCE_COLD=1` is the loud,
  last-resort override. Cold failures exit with code 3.
- `devscripts/live-build.py` (new): LIVE BUILD entry. Never writes
  `mimita.exe`; emits an immutable generation; delegates to
  `build_game_dll.py`.
- `build_game_dll.py`: generation mode with no explicit `--output` now defaults
  to `build/hotreload/mimita-live-gNNNNNN.dll` (immutable).
- Docs: new `docs/architecture/live-development/live-development.md`; updated
  `docs/operations/build-and-exe/build-and-exe.md`; added a `docs/ROUTER.md`
  route; appended a regression entry to `docs/regressions/regressions-v1.md`.

### Step 2 — runtime generations and boundary detection
- `src/hot-reload/hot-reload-system.cpp/.h`:
  - generation output renamed to `mimita-live-g%06u.dll` (never overwrite the
    active file);
  - `manifestPath()` + `pollManifestReload()`: the runtime re-reads
    `hot-modules.json` when it changes so newly added hot sources are watched;
  - `pollColdBoundary()`: hashes/mtimes a curated `cold` list from the manifest
    and records `hot_reload_boundary_violation` (once per change) without
    building or linking;
  - the cold list is read from `hot-modules.json` `"cold"`.
- `src/live-code/live-code-events.*`: added `notifyBoundaryViolation(file)`.

### Step 3 — rocket policy behind the hot ABI
- `src/hot-reload/game-api.h`: `RocketFlightStateV1`, `RocketFlightParamsV1`,
  `ExplosionStateV1`, `ExplosionParamsV1`, `GameGameplayModuleV1` and typedefs.
- `src/hot-reload/modules/rocket-behavior.cpp` (new): hot
  `adjustRocketFlight` and `explosionParameters` (identity defaults so behavior
  is unchanged until edited).
- `src/hot-reload/game-modules.h` + `src/effects/effect-part.cpp`: publish the
  `gameplay` module.
- `src/hot-reload/hot-modules.json`: added the gameplay module and the `cold`
  list.
- `src/live-code/live-gameplay.h/.cpp` (new): ABI-safe EXE bridge.
- `src/combat/weapon-rocket-launcher.cpp`: `fire()` applies the hot flight
  params (speed/lifetime) and `doExplosion()` applies the hot explosion params
  (splash radius/exponent, base damage, knockback, self-damage multiplier) with
  config fallback. No packet or authority changes.
- `src/live-code/live-code-selftest.cpp`: added gameplay module checks.

### Step 4 — the single cold build
- `python build_agent.py` ran once with no game running: `Status: SUCCESS`.
  The executable now contains the new kernel and the hot rocket hook.

## Evidence

```text
python build_agent.py                       -> Status: SUCCESS (one cold build)
python devscripts/test-live-build-invariant.py -> PASS
mimita.exe --entity-slice-selftest         -> PASS (13/13)
mimita.exe --live-code-selftest            -> PASS (includes gameplay module)
```

Invariant test output:

```text
[ok] cold build has no taskkill/auto-kill
[ok] cold build reports boundary violation
[ok] cold build has an explicit force override
[ok] live build succeeds
[ok] live build emits immutable generation file
[ok] live build leaves mimita.exe unchanged
[LIVE BUILD INVARIANT SELFTEST] PASS
```

Live build leaving the executable untouched:

```text
[LIVE BUILD] generation=990001
[LIVE BUILD] output=build\hotreload\mimita-live-g990001.dll
[LIVE BUILD] mimita.exe is never written by this path
```

## Live in-game proof status

The freshly cold-built `mimita.exe` was launched for the in-game proof; it
exited immediately in this environment (no display/OpenGL session), so the
player-vs-NPC observation could not be performed here. The executable was not
killed, and no relink occurred.

Human review required, with the game running:

1. Enter the player-vs-NPC rocket 1v1; record PID, session, and Player/NPC
   EntityIds (`entity_list`).
2. Edit `src/hot-reload/modules/rocket-behavior.cpp` (e.g. `speedScale` or
   `baseDamage`), save.
3. Confirm a new generation builds (no relink), validates, and activates at a
   safe tick; PID, session, and EntityIds unchanged; behavior changes.
4. Revert/edit again -> another generation.
5. Introduce a compile error -> previous generation stays active, game continues.
6. Edit a `cold` file (e.g. `weapon-rocket-launcher.cpp`) -> notification and
   `hot_reload_boundary_violation` journal event, no build.

## Files

New: `devscripts/live-build.py`, `devscripts/test-live-build-invariant.py`,
`docs/architecture/live-development/live-development.md`,
`src/hot-reload/modules/rocket-behavior.cpp`, `src/live-code/live-gameplay.h`,
`src/live-code/live-gameplay.cpp`.

Changed: `build_agent.py`, `build_game_dll.py`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/regressions/regressions-v1.md`, `docs/ROUTER.md`,
`src/combat/weapon-rocket-launcher.cpp`, `src/hot-reload/game-api.h`,
`src/hot-reload/game-modules.h`, `src/hot-reload/hot-modules.json`,
`src/hot-reload/hot-reload-system.h/.cpp`,
`src/live-code/live-code-events.h/.cpp`, `src/live-code/live-code-selftest.cpp`.

## Focused skills

- `docs/skills/spec-behavior-review-v1.md`: PASS. The invariant is now
  documented as authoritative; tooling matches it.
- `docs/skills/logging-checker-v1.md`: PASS. Boundary violations and lifecycle
  events are journaled, event-driven, not per-frame spam.

## Pre-existing edits preserved

Unrelated working-tree changes were not reverted or claimed.
