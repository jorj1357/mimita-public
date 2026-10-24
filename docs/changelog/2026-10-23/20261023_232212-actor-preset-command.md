# Per-actor actor preset command

Created: 2026-10-23T23:22:12Z  
Displayed: 2026-10-23 19:22:12 EDT  
Branch: `20261011zt`  
Base commit: `2103321b012f546c07bb418add26269c66bbace0`

## Result

Added `actor_list` as an alias for the existing actor listing command and added
host-only `actor_preset_actor <actor-id> <preset-id|index>`. The server keeps
the selected preset for the live actor ID, resolves it through the normal
authoritative spawn profile, and retains it across deaths and roster rebuilds.
It clears the override on disconnect or actor removal. Match-critical values
apply at the next spawn/respawn boundary. Preset indices use the alphabetical
order printed by `actor_preset_list`; in the observed current list, index `6`
is `zombie_tower_brute`.

The client command uses the existing `ServerCommandPacket` host path. The
server validates the host, actor ID, active match, and preset before accepting
the change. `DuelStatePacket` now carries the preset applied to each actor life
so a targeted player's client changes its local preset at the same spawn
boundary. Protocol version is 44.

## Specification and focused review

- `docs/specs/actor-presets/actor-presets.md` §5 now records the explicitly
  requested per-actor host command as an authoritative override path.
- Added the command/lifetime contract under §8: actor ID scope, alphabetical
  index, spawn boundary, persistence through death and roster rebuild, and
  removal on disconnect.
- Reviewed `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/networking/networking.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/regressions/README.md`.
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW. The
  existing resolution order is extended only for the command explicitly
  requested in this session. Human multiplayer confirmation remains pending.
- `docs/skills/terminal-command-checker-v1.md`: PASS_WITH_HUMAN_REVIEW. The
  alias, argument validation, stable ID forwarding, host gate, and rejection
  output use the existing command path.
- `docs/skills/logging-checker-v1.md`: PASS_WITH_HUMAN_REVIEW. One structured
  Duel event records accepted assignments; existing server-command results
  carry rejection/success feedback. Runtime log observation remains pending.

## Changed source and specification

All paths below are repository-relative. Quotes are exact excerpts from the
base and final source.

### `docs/specs/actor-presets/actor-presets.md`

- Lines 177, 216-227. Replaced the resolution-order sentence that required a
  gamemode schema declaration for every host override and added the temporary
  per-actor command contract.
- Before: `5. A mode-level or host override may supersede a role only when the mode schema explicitly declares that override and the server applies it authoritatively.`
- After: `5. A mode-level override may supersede a role only when the mode schema explicitly declares it. A server operator may also make a temporary per-actor override with \`actor_preset_actor\`; the server applies it authoritatively through that actor's spawn profile.`
- Added text begins: `### Temporary server assignment for one actor` and states
  `actor_preset_actor <actor-id> <preset-id|index>`, alphabetical indexing,
  spawn/respawn application, actor lifetime, no JSON write, and per-actor
  isolation.

### `src/terminal/actor-commands.cpp`

- Lines 48-57: replaced unchecked `std::stoi` for preset indices with guarded
  `std::stoull` and bounds checking.
- Before: `const int index = std::stoi(token);`
- After: `const unsigned long long index = std::stoull(token);` inside a
  `try`/`catch`, returning null for malformed or out-of-range values.
- Lines 100-132: server actor listing now shows `preset_override`; replicated
  listing shows the applied `preset_index`.
- Lines 140-198: registered `actor_list` as a call to existing `actorlist`;
  added `actor_preset_actor`, accepting a numeric actor ID and preset ID/index,
  applying directly on the host or forwarding a stable preset ID through
  `mpSendServerCommand` for server validation.
- Before: no `actor_list` registration and no `actor_preset_actor`
  registration.
- After: `"actor_list"` and `"actor_preset_actor"` registrations with usage
  `actor_preset_actor <actor-id> <preset-id|index>`.

### `src/network/server-packet-chat.cpp`

- Lines 585-615: added handling for `actor_preset_actor` after the existing
  host gate. Validates one decimal actor ID and one preset token, calls
  `serverSetActorPreset`, and returns a result through the existing ACK.
- Before: the host command branch had no `actor_preset_actor` case.
- After: `constexpr const char* actorPresetCommand = "actor_preset_actor ";`
  and a validating handler that returns `rejected` or
  `assigned: <preset> to actor <id> (next spawn; persists until actor removal)`.
- Lines 763 and 1055: clear actor preset state on `npc_delete_all` and timeout
  removal.
- Before: `for (uint32_t id : toRemove) players.erase(id);`
- After: call `serverClearActorPreset(id)` before erasing.
- This file also contains a separate concurrent `start`/`startnow` change; it
  was preserved and is not part of this task.

### `src/network/server-gamemode.h` and `src/network/server-gamemode.cpp`

- Header lines 179-182 and 434-436: added server-session actor override and
  current-life applied-preset maps, plus set/clear/mark APIs.
- Before: `ServerGamemodeState` had no per-actor preset maps and only exposed
  mode-wide `serverActivateActorPreset` / `serverResetActorPreset`.
- After: `actorPresetOverridesByActorId`, `actorPresetAppliedByActorId`,
  `serverSetActorPreset`, `serverClearActorPreset`, and
  `serverMarkActorPresetApplied`.
- Source lines 273-506: `serverResolveActorSpawnProfile` selects the targeted
  preset and overlays supported server-owned health, movement, weapon set,
  starting weapon, avatar, avatar policy, NPC behavior profile, and ammo fields.
- Before:
  `if (def->actorPreset) out.actorPresetId = def->id; else if (!def->actorPresetId.empty()) out.actorPresetId = def->actorPresetId;`
- After:
  `out.actorPresetId` first checks `actorPresetOverridesByActorId`, then
  applies that preset through the spawn profile while preserving the actor's
  role/team descriptor.
- Source lines 508-548: added server setter, assignment event, actor cleanup,
  and marking of the preset applied only when the spawn profile reaches the
  actor.
- Source lines 1447-1470: replicated the currently applied actor preset index
  in `DuelStatePacket`; pending assignments are not presented as already
  applied.
- Source lines 3231, 3409, 3591, 5072-5077, and 6562: actor removal cleanup,
  spawn-boundary application, and stale actor-map pruning.
- The same file has concurrent authored-map spawn changes at lines 1915 and
  1988; they were preserved and are not part of this task.

### Packet and client identity

- `src/network/packets.h:27,1247`: protocol version 43 became 44; added
  `participantActorPresets[64]`.
- Before: `constexpr uint16_t PROTOCOL_VERSION = 43;` and the participant
  packet had only role and state arrays.
- After: protocol 44 and a 1-based applied-preset index array (0 means none).
- `src/network/community-match-client.h:104-115`: added
  `ReplicatedActorIdentity::actorPresetIndex`.
- `src/network/community-match-client.cpp:603,656-670`: copies the packet
  index and applies the local actor's preset only when the server reports it
  applied for the current life.
- Before: no per-participant actor-preset value or client application branch.
- After: `identity.actorPresetIndex = packet.participantActorPresets[i];`
  followed by local actor preset application through `applyActorPreset()`.

### Spawn and actor lifetime owners

- `src/network/server-players.cpp:493`: `resetPlayerForSpawn` now calls
  `serverMarkActorPresetApplied(player.id, profile.actorPresetId)`.
- `src/network/server-npcs.cpp:340,480`: NPC adoption and respawn mark the
  resolved preset at the same spawn boundary.
- `src/network/server-gamemode.cpp:3409`: map-spawn NPC path marks its applied
  preset.
- Before: these paths stored `actorPresetId` on the current life but did not
  mark a per-actor override as applied for replication.
- After: they call `serverMarkActorPresetApplied` immediately after setting
  the actor preset ID.
- `src/network/server-packets.cpp:1358` and `src/network/server-ice.cpp:598`:
  direct and deferred player removal clear the override before erasing the
  player.
- Before: removal erased the player only.
- After: `serverClearActorPreset(playerId)` runs before erase.
- `src/network/server-ice.cpp` already contained separate in-progress ICE
  startup logging edits; only the new include and removal cleanup belong to
  this task.

## Pre-existing and concurrent workspace edits preserved

Initial inventory showed edits to `config/accounts/default.json`,
`config/analytics.json`, `config/audio/music-settings.json`,
`config/lighting.json`, `config/maps/zombietower4.json`,
`docs/jorj-docs/20261023plan.md`, `docs/specs/open-gameplay-issues.md`,
`src/gui/gui-main.cpp`, and `src/network/server-ice.cpp`, plus four existing
untracked changelogs under `docs/changelog/2026-10-23/`.

During the shared-workspace session, additional edits appeared in
`config/roles.json`, `config/weaponsets.json`, `src/gamemode/map-config.cpp`,
`src/gamemode/map-config.h`, `src/map/map-authored-geometry.cpp`,
`src/network/server.h`, and concurrent sections of
`src/network/server-gamemode.cpp` and `src/network/server-packet-chat.cpp`.
They were left intact. Only this changelog was added for this session.

## Validation and evidence

- `git diff --check` on the task paths: PASS. Git printed line-ending notices
  for two files; no whitespace errors were reported in the task paths.
- Canonical build request through `python devscripts/dev-loop.py --request
  build --wait-for-build --build-timeout 600`: FAILED, request
  `2026-10-23T23:20:54.938Z-7782a6e2`, exit code 1. No new executable was
  published. The current shared edits include `src/gamemode/map-config.cpp`;
  the earlier active-build diagnostic reported
  `src/gamemode/map-config.cpp:277:41: error: 'readVec' was not declared in this scope`.
  That file was not changed by this task.
- The dev-loop still had an older live run on build 2908 and latest successful
  build 2911 from source generation 12 while current source had advanced.
  Neither stale executable was used for feature claims.
- No tests were added or run. No `--versioninfo` journal or runtime scenario
  exists for this change because the canonical build did not publish a new
  executable. No multiplayer or human acceptance is claimed.
- Runtime acceptance still needed: issue `actor_list`, assign
  `actor_preset_actor 100333 6`, confirm actor 100333 uses Brute at its next
  spawn, verify death/respawn retains Brute, and verify removal/disconnect
  clears the assignment.

## Historical TODO noticed

`docs/specs/actor-presets/actor-presets.md:264` still contains the raw note
`2026 10 19 1834 jroj todo explain this stuff is so cool like`. It was not
edited. Suggested expansion: explain which actor preset fields apply at spawn,
which client presentation fields apply from replicated identity, and how a
temporary server assignment lasts through respawn and ends when the actor is
removed.
