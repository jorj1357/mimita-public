// 2026-09-08T16:08:34Z
/* purpose
* record the general gamemode/actor lifecycle implementation completed in this session
* preserve exact source, configuration, validation, and human-acceptance evidence
* distinguish the unified path from the remaining deprecated compatibility runtime
* this file does NOT claim live multiplayer acceptance
* this file does NOT claim bad-connection acceptance
* this file does NOT replace the append-only regression record
*/

# General gamemode actor lifecycle

## Session

- Branch: `8292026stash`
- Base commit: `213995bdc34a07be1c57b3c0aa025874764dbe8d`
- Timestamp: `2026-09-08T16:08:34Z`
- Local display timezone: America/New_York
- Pre-existing changes: the worktree already contained edits to configuration, camera, movement, HUD, documentation, regression history, and new untracked documentation/configuration. Those changes were preserved and are not attributed to this session except where this session intentionally edited `docs/regressions/regressions-v1.md`.

## Implemented changes

### Gamemode JSON and IDs

- `config/gamemodes/bombtag.json`, `duel.json`, `ffa.json`, `sandbox.json`, and `tdm.json` each gained the exact field:

  ```json
  "weapon_set_id": 1
  ```

- `src/gamemode/gamemode.h` gained `Gamemode::weaponSetId`, defaulting to `1`.
- `src/gamemode/gamemode.cpp` now reads `weapon_set_id` with a minimum value of `1`.
- `src/network/server-duel.cpp::serverCommunityStartMatch` applies the resolved gamemode's weapon-set ID before entering the unified match lifecycle.
- The active match packet/client routing now uses gamemode IDs `ffa` and `tdm` instead of the community display IDs `free_for_all` and `team_deathmatch` in the changed server/client/HUD paths.

### Shared participant and score foundations

- `src/network/server-duel.cpp::assignMatchParticipants` now accepts the active NPC map, adds living NPC IDs to the same participant and FFA kill/death maps as players, and assigns TDM team indices across the combined participant list.
- Player and NPC `matchTeam` state is populated from that single assignment operation.
- `src/network/server-damage.cpp` no longer records a player death with killer ID `0` as a player kill.
- `src/network/server-npcs.cpp` reports an NPC-caused player death through `serverDuelOnNpcDeath`.
- `src/network/server-duel.cpp` adds the NPC pending-kill entry point so NPC kills can enter the same active match score processing path.
- FFA leaderboard names now fall back to `NPC <id>` when an entry is not found in the player map.

### Startup, teams, and reset commands

- `src/network/server.cpp` starts the selected non-sandbox gamemode through `serverCommunityStartMatch(false)` after server initialization, allowing the GUI-selected server mode to use the same lifecycle as `modestart`.
- The unified FFA/TDM waiting state now starts with either two active human players or one active human plus at least one NPC.
- `src/network/server-duel.cpp` adds `serverActiveTeamList` and `serverRequestTeamChange`.
- `src/network/server-packet-chat.cpp` handles `teamlist` and `teampick <number>` before the host-only administrative command gate, validates through the active gamemode's team list, updates the player authoritatively, and broadcasts a server chat message on success.
- `src/terminal/debug-commands.cpp` registers `teamlist`, `teampick`, and `respawn_all` and routes connected-client actions through the server command transport.
- `src/network/server-duel.cpp::serverRespawnAllActors` resets active player spawn state, NPC health/lifecycle state, pending kills, and broadcasts fresh match state.

### Map lifecycle

- `src/network/server-duel.cpp::reloadDuelMap` now gives every active player a fresh authoritative spawn after the new map is committed.
- Manual and automatic map-change paths reset/reseed NPC state from the new map's spawn points and clear the NPC adoption set so the shared NPC simulation re-adopts the reset actors.

### Regression record

- Appended one unresolved, system-level entry to `docs/regressions/regressions-v1.md` describing the split duel/map-only lifecycle, player-only scoring, non-JSON mode routing, missing team controls, and incomplete actor reset behavior. It records the current correction and the remaining acceptance requirements.

## Exact behavior evidence

- `python build_agent.py`: `Status: SUCCESS`; canonical `C:\mimita-priv-v8\mimita.exe` linked at `2026-09-08 12:07:21` local build time.
- All five gamemode JSON files parsed successfully and reported IDs plus `weapon_set_id=1`.
- Headless server smoke reached transport readiness, loaded `funworld3`, built collision data, and spawned three startup NPCs. It later stopped on the existing ICE gather timeout; this is not proof of multiplayer acceptance.
- `git diff --check` was run. It reported pre-existing trailing whitespace in already-modified documentation files; no new source compilation error remained after the final successful build.

## Focused skills and documents

- `docs/ROUTER.md`
- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/networking/networking.md`
- `docs/specs/weapons/weapons.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/terminal-commands/terminal-commands.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/terminal-command-checker-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

Result: `PASS_WITH_HUMAN_REVIEW` for source/build/configuration evidence.

## Remaining human review and known limits

- No two-client or human-plus-NPC gameplay session was run.
- The scoreboard wire shape still carries the existing compact fields; full per-actor kills/deaths and JSON-defined Red/Blue rows remain future work.
- The old map-only runtime remains present as deprecated compatibility code and has not yet been deleted.
- `respawn_all` resets server-side actor state, but live visual explosion/death presentation and client convergence need runtime confirmation.
- Good-connection map switch, team switching, automatic GUI startup, continuous rounds, and weapon inventory acceptance remain unverified.
- Bad-connection testing was intentionally deferred until good-connection behavior passes.
