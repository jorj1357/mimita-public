# Task

- Task ID: arm-ragdoll-reuse
- Summary: Reuse the ragdoll arm-extend motor for per-weapon right-arm ragdoll melee and make per-weapon arm config hot-reloadable.
- Status: implemented and runtime-validated; human feel review pending
- Date, time, timezone: 2026-10-30T02:17:41Z
- Branch: current working branch
- Base commit: 1681b393
- Final commit: not committed

# Pre-existing changes

- Exact status output: `git status --short` contains many modified config, source, scenario, and documentation files from earlier sessions, plus untracked `devscripts/scenarios/arm_ragdoll_probe_v1.json` and `src/gamemode/gamemode-runtime-resolver.*`.
- Files not created or modified by this session: `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, and unrelated avatar/config changes. This session modified only the files listed below.

# Requested behavior

- The per-weapon `components.melee.arm_ragdoll` mode should use the exact ragdoll-mode arm behavior when LMB is held, not a separate PD motor.
- All arm-ragdoll fields must be live-editable and actually affect the active arm without re-equipping or restarting.
- The arm must not jitter when LMB is held.

# Specification alignment

- Current specification paths: `docs/specs/weapons/weapons.md` (`Per-weapon right-arm ragdoll melee mode`), `docs/specs/animations/animations.md` (`Per-weapon right-arm physical pose`).
- Exact requirements: only the right arm is physical; LMB drives the shared ragdoll arm motor; fixed melee animation does not start; world collision, damping, gravity, capsule, and stretch are configurable; released arm preserves follow-through.
- Why the change follows the specification: the per-weapon path now calls the same `extendArm` used by ragdoll mode, and the spec/docs now describe the shared motor and per-weapon field names.
- Conflicts or decisions: `arm_max_swing_deg` was removed per user decision; the right-arm rotation limits come from `config/ragdoll.json` `attachments.rightArm.rotation_limit_deg`. `weak_look_strength` remains as a multiplier on the released-arm follow motor.

# Exact implementation changes

## File: `src/ragdoll/ragdoll-mode.cpp`

- `extendArm`: extracted the per-arm body of `processExtend` into one shared method used by ragdoll mode and per-weapon arm ragdoll.
- `processExtend`: now a thin input adapter that calls `extendArm` for the left/right arms.
- `activateArmRagdollBody`: removed the separate `aimOffset` mapping and old motor fields; applies `max_stretch`, `angular_damping`, `extend_max_speed`, capsule overrides, and the ragdoll config generation.
- `updateArmRagdoll`: re-activates when the per-weapon config or global ragdoll config generation changes; skips activation while the player skeleton/physical body is not ready.
- `stepArmRagdoll`: LMB held uses `extend_strength`/`extend_max_speed`/`stretch_force`; released uses `extend_strength * weak_look_strength` with high-speed falloff. Sets `rightArmExtending` for the shared rotation-limit skip.
- `applyArmRagdollMotor`: deleted.
- `solveJoints`: per-weapon arm stretch applies only while `rightArmExtending`.
- `solveRotationLimits`: static bodies no longer receive rotation corrections, so the pinned torso cannot be rotated by the right-arm limit pass.
- `updateNpcAim`: re-activates on per-weapon/global config changes and skips NPCs without a skeleton/physical body.
- `armragdoll.activate`/`armragdoll.motor`: logs now include all applied fields and the actual shoulder-to-hand axis dot against camera forward.

## File: `src/ragdoll/ragdoll-mode.h`

- Added `ArmExtendTuning` and `extendArm(...)`.
- Removed `applyArmRagdollMotor(...)`.
- Added `RagdollBody::armRagdollConfigGeneration`.

## File: `src/combat/weapon-types.h`

- `ArmRagdollConfig` now uses ragdoll arm names: `extendStrength`, `extendMaxSpeed`, `maxStretch`, `stretchForce`, `weakLookStrength`, `angularDamping`, `gravityScale`, collision flags, capsule fields, and `aimRotationDegrees`.
- Removed `lmbTorqueGain`, `lmbMaxAngularSpeed`, `shoulderMaxStretch`, and `armMaxSwingDeg`.

## File: `src/combat/weapon-json-config.cpp`

- Parses the new ragdoll arm fields with bounded ranges.
- `reloadBuiltinWeaponsIfChanged` now calls `ActorPresetWeapons::refresh()` so mode-presentation active-definition copies do not hide reloaded weapon definitions.

## File: `src/combat/actor-preset-weapons.cpp`

- `refresh()` now re-applies the mode-presentation overlay when no actor preset is active, so hot-reloaded weapon definitions are visible to mode clients.

## Files: `config/weapons/swordsword/config.json`, `config/weapons/mace/config.json`, `config/weapons/chainsaw/config.json`

- Migrated `arm_ragdoll` to the ragdoll arm field names.
- Seeded ragdoll-mode defaults: `extend_strength=500`, `extend_max_speed=25`, `max_stretch=0.25`, `stretch_force=50`, `weak_look_strength=0.1`, `angular_damping=2.5`, `gravity_scale=2.0`, world/self collision true.

## Files: `devscripts/scenarios/arm_ragdoll_probe_v1.json`, `devscripts/scenarios/melee_mace_probe_v1.json`

- Removed the `startmelee` step. It overrode the launch weapon set and produced a flaky terminal completion result; the probe already launches with weapon set 29 and can equip the mace directly.

## Files: `docs/specs/weapons/weapons.md`, `docs/specs/animations/animations.md`

- Updated the arm-ragdoll behavior and tunable-field documentation to the shared ragdoll extend motor.

# Diagnostics

- Owner/category: `RagdollModeSystem`, StructuredCategory::Avatar.
- Input: equipped weapon `armRagdoll` config, primary-held state, camera forward, ragdoll config generation.
- Decision: activate/re-activate, run shared extend motor, apply weak falloff on release.
- Output: `armragdoll.activate` and `armragdoll.motor` events.
- Failure or rejection reason: activation is skipped when the actor lacks a skeleton/physical body.
- Rate limiting: motor sample every 30 ticks.

# Validation

- Focused skill paths and results: behavior change review covered by repository workflow; no separate skill command run.
- Tests and exact commands:
  - `python devscripts/dev-loop.py --request build --wait-for-build` -> build 3832 success after forcing a full object rebuild. A first incremental build linked an object compiled against the intermediate `ArmRagdollConfig` layout, which produced shifted runtime values; deleting `build/obj-debug` and the PCH and rebuilding fixed it.
  - `python devscripts/dev-loop.py --scenario-file devscripts/scenarios/arm_ragdoll_probe_v1.json --json` -> passed.
  - `python devscripts/dev-loop.py --scenario-file devscripts/scenarios/melee_mace_probe_v1.json --json` -> passed.
  - `python devscripts/dev-loop.py --scenario-file devscripts/scenarios/melee_attack_identity_v1.json --json` -> passed.
  - `python devscripts/weapon_json_sync.py --check` -> `masterlist=1 definitions=34 errors=0`.
  - `python devscripts/config_selftest.py` -> FAIL on pre-existing avatar schema warnings; weapon section loaded 34 definitions.
- Runtime or hot-reload evidence:
  - `logs/2026-10-30/20261030_023536/events.jsonl`: mace activation with correct values (`extend_strength=500`, `extend_max_speed=25`, `capsule_radius=0.208`, `capsule_half_height=0.592`, `weak_look_strength=0.1`, `gravity_scale=2.0`) and stable motor samples.
  - `logs/2026-10-30/20261030_023604/events.jsonl`: `melee_mace_probe_v1` passed.
  - `logs/2026-10-30/20261030_023626/events.jsonl`: `melee_attack_identity_v1` passed.
  - `logs/2026-10-30/20261030_023659/events.jsonl`: swordsword activated with `extend_strength=450`; after editing the JSON live to 475, the active arm re-activated with `extend_strength=475` without re-equip.
- Output files: `.dev/builds/3832/mimita.exe`, `.dev/current/mimita.exe`.

# Measured evidence

- Before values: swordsword old `weak_look_strength=10.1`, `lmb_torque_gain=0.01`, `lmb_max_angular_speed=999`, `angular_damping=22.5`; mace old `weak=0.1`, `lmb=0.1`, `max=30`, damping `0.5`, gravity `3.0`.
- After values: all three weapons use the shared ragdoll arm names and defaults listed above.
- Timestamps: build 3832 requested 2026-10-30T02:33:52Z; live reload evidence around 2026-10-30T02:37:08Z.
- Tick/frame/network measurements:
  - Mace motor samples settle from about `2.26 -> 2.92 -> 1.29 -> 1.30 rad/s` then zero, with `hand_axis_camera_dot` rising to about `0.999`; no alternating-sign high-speed spikes.
  - Swordsword live-reload activation event observed at run `20261030_023659` with `extend_strength=475`.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: live per-weapon arm config refresh never worked in the current implementation, so this is a bug fix rather than a break of previously working behavior.
- Related regression paths: none.

# Human acceptance

- Visual review: not performed by this session.
- Gameplay review: not performed; the live feel still needs human tuning and weapon-vs-wall contact review.
- Multiplayer review: server-side NPC path was exercised, but no manual multiplayer acceptance.
- Still unverified: whether the new ragdoll-mode defaults feel right for each weapon; `extend_strength`, `extend_max_speed`, `max_stretch`, damping, and gravity may need human tuning.

# Related feature record

- Feature path: none.
