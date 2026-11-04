# RMB weapon aim investigation

- Timestamp: 2026-11-04 00:28:18 EDT (2026-11-04T04:28:18Z)
- Branch/commit: `20261011zt` / `53faec75`
- User report: Revolver and Deathball appear to point behind the player while RMB aiming; expected forward aim.
- Pre-existing edits: preserved all worktree changes. In particular, `config/gamemodes/funcounterstrike.json` and the broad vehicle/config/docs changes were already dirty; no source or configuration was changed in this investigation.

## Routed evidence

Read `docs/ROUTER.md`, Deathball and weapon specifications, Fun Counter Strike and Counter Strike specifications, entity-editor, networking and contact-authority specifications, runtime scenario validation, quality gates, debug logging, terminal commands, build/EXE and task completion procedures, regression README, and `docs/skills/spec-behavior-review-v1.md`.

Relevant requirements:

- `docs/specs/weapons/deathball.md`: Deathball is a projectile weapon; its aiming and acceptance are not specified as a special backward-facing case.
- `docs/specs/gamemodes/funcounterstrike.md`: Deathball is part of the TDM-plus-grenades loadout; weapon contact/damage remains shared.
- `config/aimbody.json` says RMB aims the right arm toward a player-local point and “then aim[s] the weapon along camera forward.”

## Source finding

The first suspicious divergence is in `RagdollModeSystem::applyRightArmPointMotor()` (`src/ragdoll/ragdoll-mode.cpp`). It measures each weapon's grip-to-muzzle direction and maps that axis into the arm frame before aiming at the camera ray (around lines 999–1038), then unconditionally post-multiplies the result by `rightArmPointingRotationDegrees()` (around lines 1040–1043). The active shared value in `config/aimbody.json` is `[90, -90, 0]`. That final weapon-independent arm rotation can undo the preceding per-weapon barrel alignment, so both Revolver and Deathball can share the same visible wrong-way aim. This is the leading source-level explanation, not a runtime-confirmed cause.

The earlier unresolved tracker `docs/regressions/2026-09-30/hybrid-arm-camera-aim-REG.md` already records backward/sideways weapon aim and specifically requires comparison across weapons. It remains `ATTEMPTED FIX (5)` with human review open. Current aim logging is bounded through `AIMBODY_LIVE_SAMPLE`; it captures RMB state, camera forward, arm targets/orientations, and final limb orientation. It does not directly log the derived weapon barrel world direction before and after the configured correction, so it may not by itself identify the exact sign flip.

Weapon-specific viewmodel config also differs: Deathball has a 90-degree Y rotation while Revolver has zero. However, the physical aim path explicitly consumes each weapon's transform and measured grip/muzzle axis. The common post-correction is a stronger shared-code candidate than either model config alone.

## Evidence and limits

- Worktree inspection: `git status --short`; pre-existing edits preserved.
- Live dev-loop status: `lifecycle=build_failed`, `run=(none)`, `events=(none)`. No live executable/journal is available for this investigation. Existing build result status did not provide a current runtime artifact.
- No code/config changes, build, tests, runtime input, or human visual review performed. Thus this is an investigation finding, not a confirmed root cause or fix.
- Focused skill result: `docs/skills/spec-behavior-review-v1.md` — source/spec comparison identifies a plausible shared aim-axis override and a missing direct diagnostic; runtime evidence remains required.
- Regression result: linked existing tracker; no new regression record added because cause and current reproduction were not confirmed in runtime.

## Next evidence needed

With the dev loop healthy, run the real game holding RMB with Revolver and Deathball and inspect the exact executable's journal. Compare `camera_forward`, the derived weapon grip-to-muzzle world axis, `normal_arm_aim_target_wxyz`, and final right-arm pose at matching ticks. The first difference before/after the configured `[90, -90, 0]` correction will distinguish a shared correction error from weapon/model transform errors. Human visual confirmation is required to establish whether the visible weapon points forward.
