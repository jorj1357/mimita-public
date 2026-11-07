# Rocket Duel role loadouts

- Recorded: 2026-11-07T19:49:12Z (14:49:12 EST)
- Branch: `20261011zt`
- HEAD: `64d28fe1ca2b4c374e1ed11e0c6ae748e285b463`
- Result: `PASS_WITH_HUMAN_REVIEW`; runtime confirmation pending.

## Change

The Rocket Duel mode already declares `weapon_set_id: 3` and `force_weapon_set_for_all_actors: true`, so its shared mode policy selects the Rocket Launcher set. The role references were still shared with standard Duel, and those roles had no Rocket Launcher starting weapon. To make the NPC's initial and respawn loadout explicit without changing standard Duel:

- Added key `rocket_launcher` to weapon set ID 3 in `config/weaponsets.json`; the set still contains `rocket_launcher` and the `nothing` slot.
- Added `rocket_duel_red`, `rocket_duel_blue`, `rocket_duel_red_npc`, and `rocket_duel_blue_npc` to `config/roles.json`. Each references the Rocket Launcher set and starts with `rocket_launcher`; NPC roles retain the balanced behavior profile.
- Updated `config/gamemodes/rocket_duel.json` team player/NPC role references to those mode-specific roles. The mode still forces ID 3 for all actors.
- Updated the Rocket Duel spec in `docs/specs/gamemodes/duel.md` to describe the dedicated roles and shared set.
- Removed `--no-npcs` from Rocket Duel launch preset 43 in `devscripts/dev-launch-modes.json`; that launch preset previously prevented the requested NPC scenario from spawning any NPC.

The existing role resolver composes `weapon_set` and `starting_weapon` into the server spawn profile. The server applies the profile's weapon set at the mode-wide override boundary, and the NPC spawn and respawn owners reapply the resolved role loadout. Standard Duel continues to use its original role IDs.

## Validation and limits

- JSONC parsing of `weaponsets.json`, `roles.json`, `rocket_duel.json`, and `dev-launch-modes.json`: `PASS`.
- Focused role consistency check: both teams' player and NPC roles resolve to the `rocket_launcher` key, the key resolves to set ID 3, all four start with `rocket_launcher`, and the mode force flag remains true: `PASS`.
- `python devscripts/gamemode_json_sync.py --check`: `PASS`.
- Focused `git diff --check`: `PASS`.
- Runtime validation was not performed. The existing dev-loop run owns an unrelated Infinite World server/client; `--start-server --launch-mode 43` was rejected with “server is already running.” I preserved that session rather than switching its mode. The dev-loop was building another generation at the time of completion; no new runtime journal proves this loadout yet.
- Required runtime evidence: a Rocket Duel server journal showing `npc.roster-profile-resolved` with role `rocket_duel_*_npc`, `weapon_set_id: 3`, and `weapons` restricted to `rocket_launcher` plus the empty `nothing` slot; spawn/respawn should show the NPC equipped with `rocket_launcher`. Human gameplay/visual review is also pending.

## Documents reviewed

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/gamemodes/duel.md`
- `docs/specs/actor-presets/actor-presets.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/specs/dev-loop/multi-instance.md`
- `docs/workflows/multi-client-dev-testing.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md`
