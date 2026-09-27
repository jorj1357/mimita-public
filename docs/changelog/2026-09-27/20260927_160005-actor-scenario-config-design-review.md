// 2026-09-27 16:00 UTC

# Task

- Task ID: actor-scenario-config-design-review
- Summary: Implement the first JSON-controlled Counter-Strike preset slice using existing gamemode, role, movement, weapon, avatar, camera, and HUD owners.
- Status: implemented; runtime human acceptance still required
- Date, time, timezone: 2026-09-27T16:00:05Z, ISO 8601
- Branch: afad20a-rebuild
- Base commit: 4df94c0acb8a55186623c577990e11b142510eec
- Final commit: not applicable

# Pre-existing changes

- Exact status output: modified `config/accounts/default.json`, `config/analytics.json`, `config/weapons.json`, `devscripts/dev-loop.py`, `devscripts/dev-profiles/npc-navigation.json`, `src/combat/weapon-data.cpp`, `src/effects/effect-part-render.cpp`, `src/effects/effect-part.h`, and `src/network/multiplayer-projectiles.cpp`; untracked prior changelogs and regression file were preserved.
- Files not created or modified by this session: all pre-existing files above were preserved.

# Requested behavior

Implement the first Counter-Strike preset: forced first person, FOV 70, hidden world healthbars, fixed team avatars, Counter-Strike movement, rifle-only role loadouts, and the existing balanced NPC behavior.

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/gamemodes.md`; `docs/architecture/player-npc-systems/player-npc-systems.md`; `docs/architecture/live-development/hot-kernel.md`; `docs/architecture/live-development/live-development.md`; `docs/skills/spec-behavior-review-v1.md`.
- Exact requirements: players and NPCs share gameplay paths; gamemodes are JSON-defined rule sets; gameplay policy belongs behind the hot boundary; build evidence and runtime evidence remain separate.
- Why the change follows the specification: the mode selects existing role/movement/weapon/behavior owners and applies camera/HUD rules in memory without overwriting subsystem JSON.
- Conflicts or decisions: whether a scenario may force local-player camera/HUD policy, and whether its actor roster is authoritative for a server, remain product decisions before implementation.

# Exact implementation changes

- `config/gamemodes/counterstrike.json`: added `force_first_person`, `hide_healthbars`, role counts, and the role weapon-set selection.
- `config/roles.json`: added `counterstrike_red` and `counterstrike_blue` roles with fixed avatars, `counterstrike` movement, rifle loadout, and `balanced` NPC behavior.
- `config/weaponsets.json`: added the role-only `counterstrike_rifles` set using `projectile_rifle`.
- Gamemode/role/server spawn code: parsed and resolved mode flags and role avatar names, applying the role avatar on player and NPC spawn/respawn.
- Client presentation: forced first person in memory and restored the prior camera mode on reset/mode exit; added an in-memory healthbar visibility override that never writes `config/healthbar.json`.

# Diagnostics

- Owner/category: configuration and architecture review.
- Input: current role, gamemode, NPC behavior, camera, healthbar, movement, collision, and weapon configuration paths.
- Decision: do not make one scenario write or overwrite the existing subsystem JSON files at runtime.
- Output: Counter-Strike configuration and code path implemented.
- Failure or rejection reason: none.
- Rate limiting: not applicable.

# Validation

- Focused skill paths and results: `docs/skills/spec-behavior-review-v1.md` read; current owners and reload paths inspected.
- Tests and exact commands: `python -c "import json; paths=['config/gamemodes/counterstrike.json','config/roles.json','config/weaponsets.json']; [json.load(open(p,encoding='utf-8')) for p in paths]; print('JSON OK:', ', '.join(paths))"` passed; `git diff --check` passed.
- Build status: `python build.py build-only` completed with `BUILD SUCCESS`; 1 translation unit compiled, 481 skipped, executable linked. A later `build.py --help` invocation was not a help-only operation and launched the existing executable; it was left running.
- Runtime or hot-reload evidence: no Counter-Strike match was started for this slice; runtime visual acceptance remains open.
- Output files: executable build output and this changelog.

# Measured evidence

- Before values: Counter-Strike had FOV 70 but no first-person, healthbar, avatar-role, or rifle-role policy.
- After values: Counter-Strike declares first person, healthbar hiding, two fixed team roles, `counterstrike` movement, `counterstrike_rifles`, `projectile_rifle`, and `balanced` behavior.
- Timestamps: inspection performed 2026-09-27 UTC.
- Tick/frame/network measurements: not applicable.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: this was a new implementation slice; no confirmed regression was observed.
- Related regression paths: none.

# Human acceptance

- Visual review: not requested or performed.
- Gameplay review: not requested or performed.
- Multiplayer review: not requested or performed.
- Still unverified: live Counter-Strike match behavior, visible avatar application, actual no-healthbar rendering, weapon-slot presentation, and human play feel. Advanced NPC behavior remains intentionally out of scope.

# Related feature record

- Feature path: no feature record exists for this proposed scenario/profile layer.
