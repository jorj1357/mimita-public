// 2026-09-28T16:12:14Z
/* purpose
* record the reusable actor-preset/rules implementation
* preserve the existing role, movement, weapon, avatar, and actor lifecycle owners
* does NOT claim live multiplayer or human visual acceptance
*/

# Task

- Summary: Add one reusable actor-preset rule layer and connect the first
  `counter_strike` preset without rewriting subsystem configuration.
- Status: CODE_COMPLETE / DEV_BUILD_VERIFIED / JSON_VALIDATED /
  HUMAN_RUNTIME_REVIEW_REQUIRED
- Date, time, timezone: 2026-09-28T16:12:14Z, ISO 8601 UTC; display timezone
  America/New_York.
- Branch/commit: existing working branch; no commit created.

# Current owners and implementation

- `src/gamemode/match-roles.*`: existing shared role/profile owner now also
  loads `config/actor-presets/*.json`, keeps preset IDs separate from numeric
  list indices, and exposes sorted preset lookup. Movement and weapon values
  remain references, not copies.
- `config/actor-presets/counter_strike.json`: new schema-1 preset referencing
  movement `counterstrike`, weapon set `counterstrike_rifles`, forced FOV 70,
  forced first person, forced existing avatar `jason`, health 100, and allowed
  teams 0 and 1.
- `src/gamemode/gamemode.*` and
  `config/gamemodes/counterstrike.json`: the gamemode now references
  `actor_preset: counter_strike`; duplicate Counter-Strike camera/loadout and
  team-role declarations were removed from the gamemode.
- `src/network/server-gamemode.cpp`: the selected preset is resolved into the
  shared actor descriptors and effective camera rules before spawn; player and
  NPC spawn/respawn already consume the common `ActorSpawnProfile` path.
- `src/network/community-match-client.cpp`: first-person enforcement resolves
  from the active actor preset while preserving the existing restore-on-exit
  behavior.
- `src/terminal/actor-commands.cpp`: added `actor_preset_list`,
  `actor_preset <id|index>`, `actor_preset_info`, and
  `actor_preset_current`. Numeric indices are generated from alphabetical
  sorting and are never identity values. `actor_preset` reports the resolved
  rules; applying an active preset remains owned by gamemode start/spawn.
- `config/roles.json`: removed the unreferenced duplicate
  `counterstrike_red` and `counterstrike_blue` role definitions.

# Pre-existing work

The worktree contained unrelated deletions and modifications before this
session, including documentation, physics, analytics, build-loop, and other
configuration changes. They were preserved and are not claimed here.

# Validation

- JSON validation passed for `config/actor-presets/counter_strike.json` and
  `config/gamemodes/counterstrike.json`.
- `python build.py build-only` completed successfully; changed translation
  units were rebuilt and the incremental linker reported no failure.
- Focused `git diff --check` found no whitespace errors in the changed actor
  preset files. Existing unrelated whitespace in other working-tree files was
  preserved.
- No source config is written by actor-preset application. The runtime keeps
  the lower-level movement, weapon, avatar, and user settings files unchanged.

# Required human/runtime review

- Start a live Counter-Strike match and verify the runtime path: preset load,
  actor descriptors for a human and NPC, movement `counterstrike`, rifle-only
  inventory, avatar `jason`, health 100, FOV 70, and forced first person.
- Leave the mode and verify the user's original camera/FOV choices remain
  unchanged. Verify respawn reapplies the actor preset for both controllers.
- Exercise `actor_preset_list`, `actor_preset 1`,
  `actor_preset counter_strike`, `actor_preset_info counter_strike`, and
  `actor_preset_current` in the running build.
- No confirmed regression was identified in this session.

# Routed documents and focused skills

- `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/movement/movement.md`
- `docs/specs/weapons/weapons.md`
- `docs/architecture/player-npc-systems/player-npc-systems.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
