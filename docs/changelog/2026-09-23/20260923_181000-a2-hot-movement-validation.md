# A2: hot server movement-validation policy behind `net.movement.validate`

Date (UTC): 2026-09-23T18:10:00Z
Status: implemented; cold build and selftests verified; hot provider resolved and exercised

## Scope

Stage A2 of the live-networking migration. Move the server movement-report
accept/correct/reject policy out of cold code and behind the generic
`net.movement.validate` capability, using the same header-only
shared-implementation pattern as A1 so one policy serves the cold fallback and
the hot provider.

## Changes

1. **New `src/hot-reload/hot-movement-validation.h`**: the POD request
   (`GameMovementValidateV1` — context, ServerPlayer projection, report
   projection, config projection, and the cold-precomputed world facts
   `crossesBlockingGeometry`/`belowVoidFloor`/`acceptedStateFinite`) and the ONE
   shared policy (`MimitaNet::HotMovementValidationImpl::validate`). It is the
   exact previous `validateClientMovementReport` decision logic, so behavior is
   preserved.

2. **New hot module `src/hot-reload/modules/movement-validation.cpp`**: registers
   the `net.movement.validate` capability provider (signature
   `sig.net.movement.validate.v1`); picked up by the existing `modules/*.cpp`
   glob.

3. **`src/network/movement-validation.cpp` rewritten as the cold bridge**:
   `validateClientMovementReport` now projects `ServerPlayer` + `ClientMovementReport`
   + `MovementValidationConfig` into the POD request, precomputes the world
   sweep and void-floor fact (the policy never sees the world), resolves the hot
   provider (falling back to the shared implementation when no package is
   loaded), and maps the result back into `MovementValidationResult`. The public
   API is unchanged. Deleted the dead helpers the policy no longer needs:
   `horizontalLength`, `clientElapsedSeconds`, `tickTooOld`, `tickTooFuture`,
   `invalidAbilityTransition`, `hasContactResetEvidence`,
   `componentMagnitudeAllowed`, `validationBoundsMin/Max`, `insideBounds`,
   `reject`, and the `kTickSeconds`/`kFinitePositionLimit`/`kFiniteVelocityLimit`
   constants. `makeMovementValidationConfig`, the correction/freshness
   classifiers, counters, and the lifecycle/impulse reset bridges are unchanged.

4. **`src/live-code/live-code-selftest.cpp`**: added three checks that resolve the
   hot policy through the doorway and run it: "hot movement-validation provider
   resolves", "hot movement-validation accepts a valid report", "hot
   movement-validation rejects a stale generation".

5. **`src/hot-reload/hot-modules.json`**: added
   `src/hot-reload/hot-movement-validation.h` to `headers`.

## Behavior parity note

The existing `GAME_EVENT_MOVEMENT_VALIDATION` override (in
`modules/rocket-behavior.cpp`, dispatched from `server-packets.cpp`) is kept
unchanged. The shared policy reproduces the previous raw computed decision
exactly, so the override produces the same final decision as before. The bridge
also reproduces the previous `reject()` shape (a rejected report leaves the
accepted state default, not the projected report).

## Evidence

- Source changes: `src/hot-reload/hot-movement-validation.h` (new),
  `src/hot-reload/modules/movement-validation.cpp` (new),
  `src/network/movement-validation.cpp`, `src/live-code/live-code-selftest.cpp`,
  `src/hot-reload/hot-modules.json`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`
    (`build/mimita-game.dll`, sources=90).
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`,
    `mimita-20260923T180547.exe`.
- Automated tests (test evidence), all on `mimita-20260923T180547.exe`:
  - `--live-code-selftest` -> PASS, including the three new hot-policy checks.
  - `--movement-selftest` -> PASS.
  - `--movement-parity-selftest` -> PASS.
  - `--snapshot-chunk-selftest` -> PASS (A1 unaffected).
- Runtime / human acceptance: pending.

## Not done

- `src/network/movement-validation.cpp` is still listed in the manifest `cold`
  block. It is now a mechanism-only bridge (projection + world sweep + apply +
  the non-policy public helpers); removing it from `cold` requires deleting the
  cold fallback, which needs the headless selftests to load the hot DLL first.
- The Stage 0 listen-thread barrier and ICE callback drain remain deferred.

## Unrelated observed failure (not from this change)

`--hot-combat-selftest` reports 20 `[FAIL]` lines, all animation-policy checks
(`hot animation policy selects walk/death`, `phase2 ...`, `cold action-state
bridge publishes generic action facts`). None touch movement, validation,
snapshot, or codec. The working tree has large concurrent, uncommitted
audio/animation edits (`modules/movement-system.cpp`, `effect-composition.cpp`,
`hot-audio-policy.h`, `audio-policy.cpp`, `audio-commands.cpp`,
`hot-combat-selftest.cpp`) that refactored `playActionSound` into
`playActionRecipe`/`hotEmitRecipeSound`. These files were not touched by A1/A2;
the failures are attributed to that in-flight work and are recorded here so they
are not mistaken for a regression from this change.
