# Ragdoll mode suppresses RMB aim-FOV

Time created: 2026-11-04T01:00:28Z

## Scope

Implemented the requested behavior: while the player is in ragdoll mode, holding
the right mouse button no longer narrows the camera field of view. RMB continues
to extend the right arm through the ragdoll system, but the FOV stays at the
configured base value and is restored immediately when ragdoll mode begins.

## Source changes

- `src/engine/engine-tick-camera.cpp`: the RMB aim-FOV block now treats
  `player.ragdollModeActive` like the existing melee-authored-pose suppression.
  It resets the aim-FOV blend to zero and forces `held = false`, so both the
  gamemode `aim_fov` override and the `aimbody.json` fallback apply the base
  camera FOV while ragdolled. Non-ragdoll aiming is unchanged.
- Added one bounded `StructuredCategory::Ragdoll` /
  `StructuredLevel::Important` transition event, `ragdoll.aim_fov_suppressed`,
  emitted only when suppression starts or ends. Fields include
  `ragdoll_active`, `rmb_held`, `aim_fov_held`, `base_fov`, and `applied_fov`.
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`: documented that RMB does
  not narrow the camera FOV while ragdolled.

## Build and checks

- Dev-loop build succeeded and was published as artifact
  `2026-11-04T00:56:38.338227Z`; the running client reported the same build ID
  and `stale=False`.
- Exact artifact version output was captured from
  `C:\mimita-v9\.dev\builds\2026-11-04T00-56-38.338227Z\mimita.exe`; its
  printed `EVENTS_JSONL_PATH` was
  `logs/11-03-2026/20261103_200036/events.jsonl`.
- `python devscripts/quality/agent-gate.py` was intentionally skipped at the
  user's explicit request for this session. No gate pass is claimed.
- Source review: the edited camera block is the only RMB aim-FOV owner; no
  second FOV path exists in the ragdoll system.

## Runtime evidence

Dev-loop run `20261104_005746`, client PID 12816, canonical journal
`C:\mimita-v9\logs\2026-11-04\20261104_005746\events.jsonl`.

Right mouse was held on the live mimita window (posted `WM_RBUTTONDOWN`), and
ragdoll mode was toggled through the dev-loop terminal command
`ragdoll_mode_toggle`. The journal recorded:

- `RAGDOLL_1` `ragdoll.aim_fov_suppressed`: `ragdoll_active=true`,
  `rmb_held=true`, `aim_fov_held=false`, `base_fov=100.0`,
  `applied_fov=100.0`.
- `RAGDOLL_2` `ragdoll.aim_fov_suppressed`: `ragdoll_active=false`,
  `rmb_held=true`, `aim_fov_held=true`, `base_fov=100.0`,
  `applied_fov=99.93` (normal RMB zoom resuming).
- `RAGDOLL_3` `ragdoll.aim_fov_suppressed`: `ragdoll_active=true`,
  `rmb_held=true`, `aim_fov_held=false`, `base_fov=100.0`,
  `applied_fov=100.0` (zoom suppressed again on re-entry while RMB held).

These records show the changed path was reached and that the applied FOV equals
the base FOV whenever ragdoll mode is active, even with RMB held.

## Remaining acceptance

Human visual/gameplay confirmation that the in-game FOV feels unchanged while
holding RMB in ragdoll mode is still required. The user reported that they can
enter the game but the controls do not work in their session, so this run used
posted right-mouse input and the dev-loop command path rather than manual play.

## Worktree note

The worktree contained many unrelated user changes before this task. They were
preserved. This record describes only the ragdoll RMB FOV suppression, its
specification sentence, and its runtime diagnostic.
