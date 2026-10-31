# Horde fast Fighter presets

- Result: `PASS_WITH_HUMAN_REVIEW`.
- Timestamp: `2026-10-30T04:03:41Z` (UTC; 00:03 EDT).
- Branch: `20261011zt`.
- Commits: none created; HEAD unchanged.
- Pre-existing edits: preserved the shared working tree, including the prior Horde implementation and unrelated ongoing changes.

## Final state

The Horde continues to reuse the established Juggernaut mode Fighter and Juggernaut roles. Updated the shared Fighter role `juggernaut_fighter` in `config/roles.json` and its actor preset in `config/actor-presets/juggernaut_fighter.json` from `movement_preset: source` to `movement_preset: retrograd_fast`. Keeping the role and actor preset aligned makes the server role profile and client preset movement agree. The preset still forces FOV 100.

The Horde's Juggernaut team still uses `juggernaut_mode_juggernaut`, which references `juggernaut_arcade`: `juggernaut` movement and forced FOV 50. No new role or actor preset was added. Updated the Horde specification in `docs/specs/gamemodes/juggernaut.md` to record these assignments.

## Evidence and review

- Read `docs/ROUTER.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/juggernaut.md`, `docs/architecture/json-configuration/json-configuration.md`, and `docs/architecture/time-and-formatting/time-and-formatting.md`.
- Config hot reload is enabled in `config/hotreload.json`. The client polls `MatchRoleRegistry` and refreshes the active actor preset; the server polls role changes, and NPC respawn resolves the current role movement profile.
- The running dev loop was already on build 3852 / launch mode 44 when these JSON edits were made. It was launched before the edit, so its initial roster journal still reports the prior `source` movement; the updated role applies to clients when their actor preset refreshes and to server NPCs when they respawn. No code changed and no build was needed.
- Direct config inspection confirms Fighter FOV 100, Juggernaut FOV 50, `retrograd_fast` on both Fighter role and actor preset, and Horde team assignments to the existing roles. Focused `git diff --check` passed.
- Human movement feel and camera acceptance remain open.
