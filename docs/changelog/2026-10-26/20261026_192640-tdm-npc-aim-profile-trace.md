# TDM NPC aim profile trace

- Result: PASS (source/configuration trace); runtime confirmation not performed.
- Time: 2026-10-26T19:26:40Z (2026-10-26 15:26:40 EDT, America/New_York).
- Branch: `20261011zt`; HEAD: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`.
- Pre-existing worktree changes: many modified configuration, source, and specification files were present. `config/behavior-profiles.json` was already modified, including `balanced.aim_error_deg: 50.0`; these edits were preserved. No existing change was attributed to this trace.

## Finding

TDM teams in `config/gamemodes/tdm.json` name `elite_npc` as their NPC role. `config/roles.json` assigns that role the `elite_squad` behavior profile. The same profile is also assigned to `elite_fighter`. `config/behavior-profiles.json` line 72 is inside the `balanced` profile, so its `aim_error_deg: 50.0` does not affect TDM NPCs.

`src/npc/npc-behavior.cpp` applies the `elite` tier baseline of `aimErrorDeg = 0.06f`, then overrides it only if that profile itself defines `aim_error_deg`. `resolveNpcBehavior()` copies the resolved profile's aim error to the NPC. `NpcCombat::effectiveAimErrorDegrees()` gives an active behavior profile precedence over the shared `npc-difficulty.json` setting, and the shot path uses this result for aim deviation. Therefore, an override must be placed in the `elite_squad` object to tune these TDM NPCs.

Suggested TDM-profile-only tuning for ten times the current elite error: add `"aim_error_deg": 0.6,` beside `"skill_tier": "elite"` in the `elite_squad` object in `config/behavior-profiles.json`. This is a recommendation only; this session did not edit that profile. It affects other roles using `elite_squad` too, including `elite_fighter`; the current JSON schema does not provide a TDM-only NPC behavior profile override for these roles.

## Scope and evidence

- Read routed specifications: `docs/specs/npc-behavior/npc-behavior.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/teamdeathmatch.md`, and `docs/specs/networking/networking.md`.
- Read architecture/workflow/review/completion documents: `docs/architecture/player-npc-systems/npc-movement.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- Source/configuration trace only. No executable build, runtime scenario, event journal inspection, tests, or human playtest was performed. The user reported that changing line 72 did not alter TDM aim; this trace identifies the profile mismatch but does not independently verify a live session.
- Files changed by this session: this changelog only. No regression record added because the reported issue is explained by editing a different active profile and no implementation regression was confirmed.
