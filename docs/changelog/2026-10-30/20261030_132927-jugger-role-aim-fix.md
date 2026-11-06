# Juggernaut Fighter preset and aim correction

- Result: `PASS_WITH_HUMAN_REVIEW`.
- Timestamp: `2026-10-30T13:29:27Z` (UTC; 09:29 EDT).
- Branch: `20261011zt`.
- Commits: none; HEAD unchanged.
- Pre-existing edits: preserved all unrelated edits in the shared tree, including `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/collision.json`, `config/roles.json`, `config/weapons/rocket_launcher/config.json`, `devscripts/blender-ai`, `devscripts/config_selftest.py`, melee specifications, collision and projectile code, packet/server files, and the existing untracked changelogs `20261030_040341-horde-fast-presets.md` and `20261030_040849-configurable-weapon-clash.md`. `config/gamemodes/jugger_horde.json` already had a user-edited 10% kill heal; it was retained.

## Final state

- `src/network/server-gamemode.cpp`, participant packet send loop (lines 1819-1862): the participant identity list still has a fixed 64-actor bound, but the server now sends a per-recipient copy and guarantees each client receives its own actor role/team/state and actor-preset identity. Before this, a 68-actor Horde roster could omit a client from the shared first-64 list.
- `src/network/community-match-client.cpp`, `onState` (lines 410-430, 680-807, 879-890): resolve local role presets for all role-based modes; handle an ID that is both a role and an actor preset; keep a resolved role preset from being overwritten by the shared mode preset on each incoming state; apply role-owned FOV and perspective overrides generically. Added bounded identity and role-preset transition events.
- `src/network/community-match-client.h` (line 303): store the last local identity fingerprint so identity diagnostics are emitted only on change.
- `config/actor-presets/juggernaut_fighter.json` (line 7): `force_fov` changed from `false` to `true`, so the Fighter's configured 100 FOV wins over shared Juggernaut 50 FOV. Existing fast `retrograd_fast` movement remains.
- `config/gamemodes/jugger_horde.json` (line 216) and `config/gamemodes/juggernaut.json` (line 233): `aim.body_mode` changed from `hybrid` to `default` to match Sandbox's arm-aim path. `aim.mode: physical` and `body_enabled: true` remain. Physical weapon firing still uses the weapon barrel.
- `docs/specs/gamemodes/juggernaut.md` (lines 687-704): updated the aim behavior and retained the role-specific Horde preset documentation (lines 772-775).
- Added `docs/regressions/npc-movement/jugger-role-preset-REG-2026-10-30.md` as `ATTEMPTED FIX (1)`. The user must visually accept the FOV, speed, and Revolver RMB pose before it can be marked solved.

## Evidence

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/juggernaut.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/networking/networking.md`, `docs/specs/weapons/weapons.md`, `docs/specs/open-gameplay-issues.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/dev-loop/dev-loop.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/logging-checker-v1.md`.
- `python devscripts/gamemode_json_sync.py --check`: `PASS`.
- `git diff --check`: `PASS` (Git reported existing LF-to-CRLF normalization warnings for dirty working-tree files).
- Canonical build through `python devscripts/dev-loop.py --request build --wait-for-build`: success, build 3858, source generation 44.
- Exact artifact `C:\mimita-v9\.dev\builds\3858\mimita.exe --versioninfo`: confirmed executable path and working directory. The standalone version-info journal was `logs/10-30-2026/20261030_092820/events.jsonl`.
- Canonical dev-loop launch mode 44 (`juggernaut horde`): run `20261030_132821`, journal `logs/2026-10-30/20261030_132821/events.jsonl`; status was running, client ready, server alive, and client/server build identities matched.
- The live journal reports `client.gamemode.local-actor-identity` with `found: true`, `role: juggernaut_fighter`, team 0, actor preset `juggernaut_fighter`; `client.gamemode.local-role-preset` reports FOV 100 forced and `retrograd_fast`. The role-preset event was emitted once, confirming the shared preset no longer cycles over the role. `client.gamemode.presentation.applied` reports effective aim mode `physical` and effective body mode `default`.
- Human review: the actual Revolver RMB pose and movement feel were not observed under player input. The live journal proves role/config application, not visible aim correctness.
- Tests: none added or run; the user did not request tests. Runtime behavior was checked in the actual live dev-loop run.

## Focused review

- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`. The original mode's hybrid body setting differed from Sandbox's default aim path. The corrected modes keep physical barrel firing and select default body aiming. Visual confirmation of the Revolver pose remains open.
- `docs/skills/logging-checker-v1.md`: `PASS`. Local identity and preset selection diagnostics are transition-based; the live run emitted one role-preset application event. No per-frame diagnostic was added.
- Remaining review: human playtesting of Fighter FOV/movement and Revolver aiming with RMB in Horde and the original Juggernaut mode.
