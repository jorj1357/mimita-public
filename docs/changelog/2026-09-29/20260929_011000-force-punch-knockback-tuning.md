# Force Punch timing and movement knockback resistance

Time created: 2026-09-29T01:09:21Z
Time last updated: 2026-09-29T01:15:00Z

Status: PASS_WITH_HUMAN_REVIEW

Branch: `afad20a-rebuild`

## Requested behavior

- Move Force Punch startup/active/recovery timing into `config/weapons.json`.
- Add a movement-preset multiplier where `1.0` preserves knockback, `10.0`
  gives one tenth knockback, and `0.1` gives ten times knockback.
- Set the Heavy movement preset to `50.0`.

## Implementation

- `config/weapons.json`: Force Punch now has `attackStartupTicks: 15`,
  `attackActiveTicks: 1`, and `attackRecoveryTicks: 15`.
- `src/combat/weapon-quick-hit.{h,cpp}`: local QuickHit collision waits for
  the configured startup phase before checking NPC contacts.
- `src/network/server-attack.cpp`: the authoritative QuickHit timeline now
  reads those three weapon custom parameters instead of hardcoded `15, 8, 15`.
- `src/physics/movement/movement-types.h`: added
  `externalImpulseResistanceMultiplier` with default `1.0`.
- `src/config/movement-config.cpp`: parses and validates
  `external_impulse_resistance_multiplier`.
- `src/physics/movement/movement-step.cpp`: divides the one-tick consumed
  external impulse by that multiplier.
- Active movement presets received the field; `movement-heavy.json` is set to
  `50.0`, all other active presets to `1.0`.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/skills/spec-behavior-review-v1.md` — PASS_WITH_HUMAN_REVIEW
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

## Validation

- Changed C++ translation units compiled successfully during
  `python build_agent.py`.
- `git diff --check` passed for the changed source files.
- Full executable link was attempted but failed on pre-existing unrelated
  unresolved terminal/config symbols (`registerWeaponCommands`,
  `registerDebugCommands`, `pollWorldCrosshairConfig`, and similar).
- A retry of `python build_agent.py` returned `SUCCESS / Nothing changed`; it
  did not relink a new executable, so the new source is not active in a new
  runtime binary yet.
- No running MiMITA process was stopped or replaced.

## Human review still needed

- Resolve the unrelated linker failure and produce a new executable.
- Test Force Punch against an NPC and confirm the delay is 15 fixed ticks.
- Test the Heavy preset and confirm received knockback is approximately 1/50
  of the same force under the default preset.
- Confirm `0.1`, `1.0`, and `10.0` values behave as documented.

Pre-existing unrelated worktree edits were preserved.
