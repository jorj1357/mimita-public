# Payload Team Spawn Positions and Runtime Trace

Time created: 2026-10-24T01:10:40Z
Time last updated: 2026-10-24T01:10:40Z

Branch: `20261011zt`
HEAD: `2103321b`
Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Set the lower Payload attacker spawn to the captured 1137-height position and
the higher defender spawn to the captured 2256-height position in
`config/maps/payload1.json`, then inspect whether the game uses those authored
points when a new player joins after a live JSON edit.

## Files changed

### `config/maps/payload1.json`

The file was already untracked in the working tree before this session. It is
user-owned input; preserve that state and all other pre-existing edits.

- Attacker entity `payload_attacker_spawn`, position at lines 25-29:
  - Before: `[-312.0, -28.0, 1137.0]`
  - After: `[-312.589, -28.124, 1137.356]`, copied from the supplied screenshot.
- Attacker tag at line 39:
  - Before: `attackers`
  - After: `attacker_spawn`, matching Payload's configured `spawn_group`.
- Defender entity `payload_defender_spawn`, position at lines 59-63:
  - Before: `[-292.0, -64.0, 12256.0]`
  - After: `[-292.559, -64.828, 2256.304]`, copied from the supplied screenshot.
- Defender tag at line 73:
  - Before: `defenders`
  - After: `defender_spawn`, matching Payload's configured `spawn_group`.

No prior text was changed in any other file.

## Source and specification findings

- `docs/specs/gamemodes/payload.md` section 8 specifies one fixed spawn for
  each team and says the locations should be editable in JSON.
- `docs/specs/entity-editor/entity-editor.md` records that initial player join
  uses `HeadlessWorld::spawnPoints`, whereas gamemode-managed selection reads
  enabled map-config spawn entities. It also identifies this as incomplete
  spawn-path coverage.
- `src/map/map-authored-geometry.cpp:240-260` appends enabled JSON spawn
  entities to the server's `HeadlessWorld::spawnPoints` when authored geometry
  is applied.
- `src/network/server-packets.cpp:941-951` handles initial join by calling
  `serverPickSafeSpawn(world, ...)` before team assignment.
- `src/network/server-players.cpp:315-375` selects and validates a point from
  that loaded world list; it does not inspect the current map-config registry
  or filter by team.
- `src/network/server-gamemode.cpp:1802-1852` has a separate team-aware path
  that reads `MapConfigRegistry` entities and requires an empty tag or exact
  equality with the team's `spawn_group`. The old `attackers` and `defenders`
  tags did not equal `attacker_spawn` and `defender_spawn`; the updated tags
  satisfy this consumer.
- The server polls `MapConfigRegistry::pollReload()` on a bounded interval at
  `src/network/server-gamemode.cpp:5402-5410`. That refreshes the registry,
  but the initial-join path still reads the already-built `HeadlessWorld`
  list. `applyAuthoredMapGeometry` is called while loading a map, not as part
  of that registry poll. Therefore a live JSON edit can reach registry-based
  team spawn selection while leaving initial-join spawn data stale until the
  map world is rebuilt.

## Runtime evidence

Reused the already-running Payload dev-loop session without restarting it.
The active server was PID `23964`, executable
`.dev/builds/2953/mimita.exe`, map `payload1`, run ID `20261024_010238`.
Its journal was
`logs/2026-10-24/20261024_010238/events.jsonl`.

- At sequence 10 (`2026-10-24T01:02:39.892Z`),
  `map-authored-geometry.applied` reported `authored_spawn_count=2` and
  `spawn_points=2` on the server.
- At sequence 98 (`2026-10-24T01:03:12.566Z`),
  `spawn.server-selected` reported `source=authored`, `spawn_index=0`,
  `validated=true`, and position `[-304.0, -32.0, 1435.1245]` for a new
  player. This confirms initial join uses an authored spawn from the server's
  loaded list, but that selected position is not either requested coordinate.
- The map JSON was edited after that join event. The current process journal
  contains no post-edit new-join event, so the edited coordinates themselves
  have not been proven through a fresh join.
- Ran `--versioninfo` on the exact active executable path. That identity-only
  invocation reported build `2953` and its own journal path
  `logs/10-23-2026/20261023_211118/events.jsonl`; it was not the active server
  process journal.

No build or code change was required for this JSON-only edit. The active
server/client session is on build `2953`, while the dev-loop status reported
published build `2957`; it was left running. JSON parsing and the two resulting
ID/tag/position values were checked successfully. No fresh human multiplayer
acceptance was performed.

## Focused review

- `docs/skills/spec-behavior-review-v1.md`: `PASS_WITH_HUMAN_REVIEW`; the
  current implementation has separate initial-join and team-aware spawn paths.
- `docs/skills/logging-checker-v1.md`: `PASS_WITH_HUMAN_REVIEW`; existing
  server records show authored-spawn selection but do not record the team or
  selected entity ID for the initial join.
- Unresolved: one new player needs a live multiplayer check after the map
  world has been rebuilt, and initial-join team selection needs a code change
  if Payload requires each joining player to land on their assigned team's
  point immediately.

## Pre-existing working-tree edits

Preserved all edits that existed before this session: `config/accounts/default.json`,
`config/analytics.json`, `config/audio/music-settings.json`,
`config/gamemodes/tdm.json`, `config/lighting.json`,
`config/maps/zombietower4.json`, `config/movement.json`,
`config/onlinemodes.json`, `config/roles.json`, `config/weaponsets.json`,
`devscripts/dev-launch-modes.json`, `docs/ROUTER.md`,
`docs/jorj-docs/20261023plan.md`,
`docs/specs/actor-presets/actor-presets.md`,
`docs/specs/gamemodes/gamemodes.md`,
`docs/specs/gamemodes/teamdeathmatch.md`,
`docs/specs/open-gameplay-issues.md`, `src/game/game-cli.cpp`,
`src/gamemode/gamemode.cpp`, `src/gamemode/gamemode.h`,
`src/gamemode/map-config.cpp`, `src/gamemode/map-config.h`,
`src/gui/gui-main.cpp`, `src/map/map-authored-geometry.cpp`,
`src/network/community-match-client.cpp`,
`src/network/community-match-client.h`,
`src/network/multiplayer-physical-entities.cpp`, `src/network/packets.h`,
`src/network/physical-entity-replication.cpp`,
`src/network/server-gamemode.cpp`, `src/network/server-gamemode.h`,
`src/network/server-ice.cpp`, `src/network/server-npcs.cpp`,
`src/network/server-packet-chat.cpp`, `src/network/server-packets.cpp`,
`src/network/server-players.cpp`, `src/network/server.h`,
`src/physics/physical-entity.h`, `src/terminal/actor-commands.cpp`,
`src/terminal/crate-commands.cpp`, `config/gamemodes/payload.json`,
`docs/changelog/2026-10-23/20261023_182134-clarify-gameplay-issues-intake.md`,
`docs/changelog/2026-10-23/20261023_182820-extend-room-startup-wait.md`,
`docs/changelog/2026-10-23/20261023_184545-audio-format-support-audit.md`,
`docs/changelog/2026-10-23/20261023_224504-set-zombietower-radii.md`,
`docs/changelog/2026-10-23/20261023_232212-actor-preset-command.md`,
`src/gamemode/payload-runtime.cpp`, and `src/gamemode/payload-runtime.h`.
