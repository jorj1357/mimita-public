# Hot animation ownership — phase 3: candidate self-test, skeleton validation, cold build + tests

Date: 2026-09-16 14:53 EDT (UTC 2026-09-16T18:53:37Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: `PASS (animation) / 1 CROSS-SESSION FAILURE REMAINS`

## Task

Phase 3 of the fully hot-reloadable C++ animation migration: an animation
candidate self-test integrated into the DLL self-test hook, `skeleton.validate`
wiring for required body parts, and verification of the versioned animation
schemas/migration. Also perform the one intentional cold build and run the
headless test suites.

## What changed (hot)

### 1. Animation candidate self-test
- NEW `src/hot-reload/hot-animation-selftest.h` + `modules/presentation/
  animation-selftest.cpp`: `runAnimationSelfTest(message, size)` verifies the
  active `AnimationClipSource` default, distinct required part hashes, clip
  invariants (positive duration, non-empty mask, ordered finite frames),
  deterministic finite sampling, one-shot final-frame hold, deterministic
  procedure walk, masking, and blending, plus the versioned state contract
  (`AnimationState` v2 grew relative to v1).
- `src/effects/effect-part.cpp` (the DLL self-test hook) now calls it, so every
  candidate generation is rejected before activation if the animation code is
  malformed/inconsistent. The previous generation keeps running.

### 2. Required-part validation
- `hot-animation-clips.h`: `kRequiredPartCount` / `requiredPartHash()`.
- NEW `hot-animation.h` `HotAnimationValidV1` (local-only `AnimationValid`
  component, `GAME_NET_NONE`).
- NEW `modules/presentation/animation-validate.cpp`: `hot.animation-validate`
  (render.frame, priority 0) validates every animated actor's required body
  parts through the generic `skeleton.validate` capability and records the
  result; registers the `animvalidate` diagnostic command.
- `hot-modules.json`: new headers registered for change detection.

### 3. Phase-2 defect fixes found by the tests
- `selectAction` now treats the `JUMPING` flag (not only upward velocity) as a
  jump, so a jump intent correctly interrupts walk while airborne.
- Respawn-return selftest now resets speed before asserting idle.

## Cold build and tests

- `python build_agent.py` -> `BUILD SUCCESS` (273 compiled; then incremental
  after the test/defect fixes).
- `python build_game_dll.py` -> `build\mimita-game.dll` success (56 sources).
- `mimita.exe --live-code-selftest` -> `PASS` (this loads the DLL and runs the
  self-test hook, so the animation candidate self-test is exercised).
- `mimita.exe --hot-combat-selftest` -> all animation tests PASS:
  - 20 phase-2 transitions/determinism checks (idle/walk/jump/dash/freeze/
    equip/shoot/reload/slash/lunge/death/respawn/determinism);
  - `skeleton.validate` capability resolves, reports missing required parts,
    accepts a complete skeleton;
  - `hot.animation-validate` records a valid skeleton;
  - `ActorActionState` and `AnimationMemory` schemas registered.

## Remaining failure (not animation)

`--hot-combat-selftest` reports exactly one failure:
`local possessed body is submitted exactly once (generic path yields)
[visible=26 skipped=23]`. This is in the generic presentation/tool-attachment
mesh path owned by the concurrent tool-visual session (uncommitted
`modules/presentation/attachment.cpp`, `effect-composition.cpp`,
`hot-presentation.h`, `presentation-render.cpp`, and their `hot-combat-selftest`
diagnostic line). Those files were not modified by this work. An earlier run of
the same suite also showed three `PresentationState` size-mismatch failures from
that session's `HotPresentationStateV1.scaleXYZ` ABI growth; those now pass and
only the submission-count check remains.

## Evidence classes (separate)

- Source: yes (files above).
- Hot-module build: yes (`build_game_dll.py`, 56 sources).
- Cold build: yes (`build_agent.py`, SUCCESS).
- Candidate self-test: yes (`--live-code-selftest` PASS through the DLL hook).
- In-engine animation tests: yes (all PASS in `--hot-combat-selftest`).
- Runtime activation of a live edit: no.
- Live visual proof: no.
- Multiplayer proof: no.
- Human acceptance: pending.

## Pre-existing / concurrent changes

The worktree contained concurrent edits from another session (tool-visual
recipes, movement/reconcile, `HotPresentationStateV1.scaleXYZ`, etc.). They were
preserved and not claimed. `game-api.h`, `live-behavior.cpp`,
`presentation-entities.cpp`, and `hot-combat-selftest.cpp` contain both this
session's edits and pre-existing edits.

## Human review needed

1. `mimita.exe --hot-combat-selftest`: the single presentation failure belongs to
   the tool-visual session; confirm ownership there.
2. Live: start a match, edit a walk keyframe in `hot-animation-clips.h`, confirm
   the animation changes in the same PID/session/EntityIds, then roll back.
3. Visual: verify idle/walk/jump/dash/freeze/equip/shoot/reload/slash/death on
   the local body and remotes.
