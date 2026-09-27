# afad20a freeze + grounded parity, hot-reloadable

Date: 2026-09-23
Status: cold build succeeded; hot DLL built; deterministic tests pass; human acceptance pending

Related specification: `docs/specs/movement/movement.md`
Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).
Related work: `docs/changelog/2026-09-23/20260923_011901-afad20a-grounded-reset-limb-refresh.md`,
`docs/changelog/2026-09-23/20260923_032941-dash-down-dash-no-buffer-hot-edge.md`

## What was different (afad20a vs on-disk hot path)

The NPC freeze/grounded code is byte-identical to `afad20a` (`src/npc/npc.cpp`
freeze decision and `sensors.touchFloor = npc.body.ground.hasWorldContact`). The
divergence was entirely in the player hot movement path.

Grounded:
- `afad20a` `restoreTouchAvailability` restores dash, down-dash, air-jump **and
  `freeze.available = true`**; a grounded actor synthesizes a Ground contact each
  tick. The cold fallback `movement-step.cpp` already matched.
- The hot path restored dash/down-dash/air-jump but **reset the freeze timer
  instead of restoring freeze availability**, and always fed `freezeAvailable=1`.

Freeze:
- `afad20a` `updateFreeze`: press+available hard-stops stored velocity, consumes
  availability, timer accumulates; stored velocity is **never rescaled**.
  Suppression is applied to the collision/integration velocity via the pow4
  pass-through (`freezeHorizontalPassThrough`) and reconciled afterwards.
- The hot `movement-freeze.cpp` instead **multiplied stored velocity by pow4 every
  held tick** (compounding) and never gated availability, and the freeze timer did
  not persist across the hot boundary at all.

## Changes

State plumbing (one-time EXE change; logic stays hot):
- `src/hot-reload/game-api.h`: `GameMovementRuntimeStateComponentV1` gains
  `freezeActive` / `freezeAvailable`; `MOVEMENT_RUNTIME_STATE_VERSION` -> 3.
- `src/ecs/components.h`: `MovementRuntimeStateComponent` gains the same fields.
- `src/live-code/live-behavior.cpp`: the movement-runtime projections now carry
  `freezeActive`, `freezeAvailable`, and `freezeTimerSeconds` both ways.
- `src/network/server-players.cpp`: the cold server path seeds and mirrors the
  freeze runtime state alongside the other abilities.
- `src/npc/npc.cpp`: the hot NPC result projects freeze state onto the typed body,
  and the per-tick seed carries it into the component.

Hot freeze policy (`src/hot-reload/hot-movement-policy.h`,
`src/hot-reload/modules/movement-freeze.cpp`):
- `restoreTouchAbilities` now restores `freezeAvailable` on contact (it no longer
  resets the pass-through timer), matching afad20a.
- `freezePolicy` is model-aware via `GameFreezePolicyV1::movementModel`:
  - `0` = afad20a: activation hard-stops stored velocity; the stored velocity is
    not rescaled while held.
  - `1` = v2.0.6: the piecewise-quadratic stored-velocity curve still applies
    (`freezeVelocityMultiplierV206`), so v2.0.6 parity is preserved.
- Added `freezePassThrough` (pow4) and `kFreezeReconcileMinPassThrough`.

Hot callers (`movement-system.cpp`, `actor-movement-system.cpp`):
- Persist `freezeActive` / `freezeAvailable` / `freezeTimerSeconds` through the
  runtime-state component instead of forcing availability.
- For afad20a (`movementModel == 0`), scale the collision/integration velocity by
  the pass-through curve and reconcile the stored velocity after the solve
  (un-scale above the threshold, keep the stored velocity below it), mirroring the
  cold `physics-mini` collision velocity view. Momentum is preserved and returns
  when the freeze ends.
- `movement-step.cpp` cold dispatch sets `movementModel` from `config.walkMode`.

## Evidence

Build evidence:
- Hot DLL: `python build_game_dll.py` -> `DLL build success` (82 sources).
- Cold build: `python build.py build-only` -> `BUILD SUCCESS`, linked `mimita.exe`.

Test evidence (deterministic, headless):
- `--movement-algorithm-selftest`: PASS (freeze activation, no-rescale while held,
  release, timer clamp).
- `--movement-v206-parity-selftest`: PASS, including the freeze curve at
  t=0/1.25/2.5/3.75/5 (now model-aware).
- `--afad20a-parity-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--live-code-selftest`, `--collision-selftest`,
  `--air-movement-parity-selftest`, `--hot-authoritative-selftest`,
  `--generic-integrator-selftest`, `--npc-actor-state-selftest`,
  `--npc-entity-selftest`, `--server-spatial-authority-selftest`: PASS.

Runtime evidence:
- Not performed. The behavior needs a live playtest (E freeze on ground repeat,
  freeze mid-air unavailability until a surface is touched, momentum on release).

Human acceptance:
- Pending.

## Limits

- `--gameplay-boundary-selftest` fails on "hot melee NPC tool dealt damage". This
  path does not touch movement/freeze state and the failure is unrelated to this
  change (the melee tools do not read the movement runtime state).
- `movement-parity-selftest` was updated to use fresh entities and to expect the
  afad20a ground bounce instead of vertical rest, consistent with the pre-existing
  uncommitted `movement-selftest.cpp` fix.
- The hot path has no separate external-impulse tank; the pass-through suppression
  is observable through velocity added while frozen (jump/knockback).
- Grounded gravity: the hot gravity policy is skipped while grounded (to avoid
  solver micro-bounce). afad20a applied gravity every tick and let the ground snap
  zero it. This is a pre-existing intentional difference, unchanged here.
