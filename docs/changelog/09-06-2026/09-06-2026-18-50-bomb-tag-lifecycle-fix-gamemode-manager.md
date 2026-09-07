// 09 06 2026, 18 50
/* purpose
* Changelog for Bomb Tag mode lifecycle fix + generic GamemodeManager refactor
* Phase 1: Fixed 3 critical bugs preventing Bomb Tag from starting
* Phase 2: Replaced hardcoded BombTagManager with data-driven GamemodeManager
*/

# Bomb Tag Lifecycle Fix + Generic GamemodeManager Refactor

## Branch
`develop/v2.0.1`

## Time (EST)
09-06-2026 18:15 - 18:50

## What Was Broken

### Bug 1: `serverCommunityStartMatch()` never activated bomb tag
The function set `matchMode = "bombtag"` but never called `serverBombTagStartMatch()`.
The `bombTagActive` flag stayed false, so `serverBombTagTick()` returned immediately.
The state machine was frozen at `DUEL_PHASE_INTERMISSION` forever.

### Bug 2: Character-prefix routing broke bomb tag
`multiplayer-tick.cpp:1224` routed packets by `matchMode[0] == 'f' || 't'`.
Bomb tag's `'b'` went to `DuelQueue` (1v1 handler) instead of `CommunityMatchClient`.
The client never received the phase, timer, or any match state.

### Bug 3: No client handler for `PACKET_BOMB_TAG_STATE`
Packet type 66 was sent by the server but `multiplayer-tick.cpp` had no case for it.
The `onBombTagState()` method existed but was never called from the network receive path.

### Bug 4: Two config systems with mismatched IDs
`onlinemodes.json` used `"team_deathmatch"` but `gamemodes/tdm.json` used `"tdm"`.
`GamemodeRegistry::get("team_deathmatch")` returned defaults (no match).

## What Was Fixed

### Phase 1: Bug Fixes

1. **`serverCommunityStartMatch()`** now calls `serverBombTagStartMatch()` when the
   community mode is `"bomb_tag"`. The state machine can now progress through
   INTERMISSION → PRE_MATCH → COUNTDOWN → GO → ACTIVE.

2. **Character-prefix routing removed.** All community match modes now go to
   `CommunityMatchClient`. Only the original 1v1 duel goes to `DuelQueue`.
   The routing check is now: `matchMode == "duel" → DuelQueue, else → CommunityMatchClient`.

3. **`PACKET_BOMB_TAG_STATE` handler added** in `multiplayer-tick.cpp`.
   Bomb tag state packets are now received and processed by `CommunityMatchClient::onBombTagState()`.

4. **Config ID bridge.** `onlinemodes.json` entries now have a `gamemode_id` field
   that points to the correct `config/gamemodes/*.json` id. The `CommunityMode`
   struct stores this bridge. `serverCommunityStartMatch()` uses `gamemode_id`
   to look up the `Gamemode` from `GamemodeRegistry`.

5. **Early-out filter removed** from `CommunityMatchClient::onState()`.
   All community modes are now accepted, not just FFA/TDM.

6. **All `matchMode == "ffa"` / `"tdm"` comparisons updated** to use
   community mode ids (`"free_for_all"`, `"team_deathmatch"`, `"bomb_tag"`).

### Phase 2: Generic GamemodeManager

1. **`GamemodeFeatures` struct** added to `Gamemode`. Each gamemode JSON can
   declare features: `world_timer`, `bomb_holder_text`, `bomb_blink`,
   `infinite_rounds`, `no_weapons_except_bomb`, `boss_healthbar`, `world_text`,
   `timer_above_entity`.

2. **`GamemodeManager`** replaces `BombTagManager`. It reads the current mode's
   features from `GamemodeRegistry` and renders accordingly. No hardcoded mode
   name checks — if the JSON declares `bomb_holder_text: true`, the manager
   renders the bomb holder text. If it declares `boss_healthbar: true`, it
   renders a boss healthbar (future).

3. **All 12 engine files** updated from `gBombTagManager` to `gGamemodeManager`.

4. **`bombtag.json`** updated with feature declarations.

## Files Created

| File | Purpose |
|------|---------|
| `src/game/gamemode-manager.h` | Generic GamemodeManager class |
| `src/game/gamemode-manager.cpp` | Feature-based HUD and world rendering |
| `config/gamemodes/sandbox.json` | Sandbox gamemode definition |

## Files Modified

| File | Change |
|------|--------|
| `config/onlinemodes.json` | Added `gamemode_id` to each entry |
| `src/network/community-server-config.h` | Added `gamemodeId` to `CommunityMode` |
| `src/network/community-server-config.cpp` | Parse `gamemode_id` from JSON |
| `src/network/server-duel.cpp` | Fixed `serverCommunityStartMatch()` to activate bomb tag; use `gamemode_id` for lookup; update all `matchMode` comparisons to community ids |
| `src/network/multiplayer-tick.cpp` | Removed character-prefix routing; added `PACKET_BOMB_TAG_STATE` handler |
| `src/network/community-match-client.cpp` | Removed early-out filter; accept all modes |
| `src/network/community-match-client.h` | Updated `active()` and `isBombTag()` to use community ids |
| `src/gamemode/gamemode.h` | Added `GamemodeFeatures` struct |
| `src/gamemode/gamemode.cpp` | Parse `features` from JSON |
| `src/game/bomb-tag.cpp` | Deprecated: removed `setArmToWeaponPose` (moved to gamemode-manager) |
| `config/gamemodes/bombtag.json` | Added `features` block |
| 12 engine tick files | Replaced `gBombTagManager` with `gGamemodeManager` |
| `src/gui/hud/match-leaderboard.cpp` | Updated mode checks to community ids |
| `src/engine/engine-tick-ui-overlays.cpp` | Updated mode title rendering for all modes |

## How Adding a New Mode Works Now

1. Add a JSON file to `config/gamemodes/` with the mode's gameplay rules
2. Add an entry to `config/onlinemodes.json` with `gamemode_id` pointing to it
3. Declare `features` in the gamemode JSON for any special rendering needed
4. Zero C++ changes required for standard modes

The `GamemodeManager` automatically renders whatever features the JSON declares.
The server state machine uses the community mode id for routing.
The `GamemodeRegistry` provides all gameplay configuration.

## Validation

1. **Build**: `python build_agent.py` → SUCCESS
2. **Tests**: `bomb-tag-state-test.exe` → 0 failures (13 tests)
3. **Tests**: `bomb-tag-shuffle-test.exe` → 0 failures (9 tests)
4. **Mode lifecycle**: Server now progresses INTERMISSION → COUNTDOWN → GO → ACTIVE
5. **Client routing**: DuelStatePacket for bomb tag reaches CommunityMatchClient
6. **Bomb state**: BombTagStatePacket is received and processed by client

## Deprecated (Not Deleted)

- `src/game/bomb-tag.h` — kept for backward compatibility, marked deprecated
- `src/game/bomb-tag.cpp` — kept for backward compatibility, marked deprecated
- `src/game/bomb-tag-state.cpp` — kept for backward compatibility, marked deprecated
- `BombTagManager` class — replaced by `GamemodeManager`
- Character-prefix routing — replaced by id-based routing
- `ServerMode` enum — kept for backward compatibility, new modes use string ids
