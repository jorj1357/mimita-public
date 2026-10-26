# Juggernaut team avatar assignments

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-26T20:31:44Z (2026-10-26 16:31:44 EDT)
- Branch: 20261011zt
- Base commit: 926c2d12; no commit created.
- Pre-existing edits: `config/accounts/default.json`, `config/camconfig.json`, `config/gamemodes/tdm.json`, `config/healthbar.json`, `config/hitfx.json`, `config/impact_decals.json`, `config/lighting.json`, and `config/movement/movement-source.json` were already modified and were left untouched.

## Final state

- `config/roles.json`, role `juggernaut_fighter`: added `avatar: abusiveboy` and `avatar_forced: true`.
- `config/roles.json`, role `juggernaut_mode_juggernaut`: added `avatar: monsternormal` and `avatar_forced: true`.
- `config/actor-presets/juggernaut_arcade.json`, avatar allowlist: added `monsternormal`; prior list did not permit the selected Juggernaut avatar under the active role policy.
- Existing runtime resolves team roles and forced role avatars into the server-authoritative spawn profile (`src/gamemode/match-roles.cpp:119`; `src/network/server-gamemode.cpp:400`). No gamemode schema change was needed. The masterlist defines allowed gamemode fields; team-specific avatar selection is already owned by role definitions.

## Specification review

Read `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`, and `docs/architecture/json-configuration/json-configuration.md`. The actor-presets spec assigns avatar choice to actor presets/roles and team role assignment to gamemode team entries. Read `docs/skills/spec-behavior-review-v1.md`; finding: current supported owner is the Juggernaut team role, so forced avatar configuration was added there rather than inventing unsupported fields in the mode JSON/masterlist. The role's avatar is force-applied server-side; the actor preset's allowlist is retained as policy and now includes the Juggernaut avatar.

## Validation and acceptance

- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `git diff --check`: PASS (Git emitted a pre-existing line-ending warning for modified `config/gamemodes/tdm.json`; that file was not edited in this task).
- A plain JSON parse attempt on `config/roles.json` failed because this repository file contains comments and is parsed as JSONC. This was not a config-loader failure; no JSONC-aware selftest was run.
- No build or live runtime scenario was run. Human review in a Juggernaut match is still needed to confirm both avatars are visible on their assigned teams.

