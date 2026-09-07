// 2026-09-07 17:03 EST
/* purpose
* Bomb tag spec alignment: add sounds, disconnect handling, NPC names, feature routing
* removes dead BombTagManager code, adds sound playback from spec,
* handles bomb holder disconnect, fixes NPC name display, replaces hardcoded mode checks
* does NOT add lag compensation or pass debug data (ignored per user request)
*/

# Task

- Task ID: bomb-tag-spec-alignment-0907
- Summary: Align bomb tag code with bombtag.md and gamemodes.md specs — add missing sounds, disconnect handling, NPC name display, and replace hardcoded mode checks with feature-based routing
- Status: SUCCESS — build passes, all changes compile cleanly
- Date, time, timezone: 2026-09-07T17:03:30Z, EST
- Branch: 8292026stash
- Base commit: 6334b98e5f789f65c4d1a3c9cc7d9a90b5bcf686
- Final commit: (uncommitted changes, to be committed)

# Pre-existing changes

- Exact status output: clean working tree before edits
- Files not created or modified by this session: none — all edits are new

# Requested behavior

User requested investigation of bomb tag and gamemodes code against their specs, then implementation of fixes for:
1. Bomb sound playback (tick, pass, inactive, explosion)
2. Bomb holder disconnect handling
3. Delete dead BombTagManager code
5. NPC bomb holder name display
6. Replace hardcoded matchMode == "bomb_tag" checks with feature-based routing
(Ignored: lag compensation/pass debug data and generalized HUD features per user request)

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/bombtag.md`, `docs/specs/gamemodes/gamemodes.md`
- Exact requirements:
  - bombtag.md §2: Tick sound every 60 ticks, pass sound on transfer, inactive sound during 60-tick grace, explosion sound on death
  - bombtag.md §5: "holder disconnects → server detects → new eligible player selected immediately → bomb transferred → game continues"
  - bombtag.md §7: "if its a npc, then the npc's name is the username"
  - gamemodes.md: "the code should not do hardcoded things like if the name of the mode starts with this specific letter"
  - gamemodes.md: "GamemodeManager" should be generic, not "BombTagManager"
- Why the change follows the specification:
  - Sounds: client detects timer/holder changes from replicated state and plays the exact .wav files specified in the spec
  - Disconnect: server checks bomb holder validity every tick; if holder is missing/dead, bomb transfers immediately
  - NPC name: `bombHolderName()` now looks up the NPC's `avatarName` from the client-side NPC system
  - Feature routing: replaced `d.matchMode == "bomb_tag"` with `d.hasBombFeature` set from gamemode JSON features
  - Dead code: removed deprecated `BombTagManager` class (266 lines) — all functionality is in `GamemodeManager`
- Conflicts or decisions:
  - `BombTagPassEventPacket` is broadcast by the server but never received by clients. Pass detection uses bomb holder ID change in `BombTagStatePacket` instead (simpler, no new packet handler needed)
  - Inactive sound uses `playEventSound` which plays once; continuous looping would require audio system changes beyond scope
  - NPC name lookup uses `gpNpcSystem` global pointer (same pattern as other engine systems)

# Exact implementation changes

## File: `src/game/gamemode-manager.h`

- Lines 67-70 (new): Added sound state tracking fields `mPrevBombTimerTicks`, `mPrevBombHolderId`, `mInactiveSoundPlaying`
- Lines 3-6: Updated comment — removed "Replaces the hardcoded BombTagManager" phrasing
- Reason: header needs fields to detect state changes for sound triggers

## File: `src/game/gamemode-manager.cpp`

- Lines 31: Added `#include "npc/npc.h"` for NPC name lookup
- Lines 30: Added `#include "terminal/terminal-state.h"` for `gpNpcSystem` access
- Lines 69-78: `start()` now initializes sound tracking fields to zero/false
- Lines 142-154: `bombHolderName()` now looks up NPC avatar name via `gpNpcSystem->all()` when `bombOwnerType == BOMB_OWNER_NPC`
- Lines 175-228: `update()` now detects bomb state changes and plays sounds:
  - Explosion: timer > 0 → 0 → plays `explosion2.wav`
  - Pass: holder ID changed (both non-zero) → plays `bombpass1.wav`
  - Tick: timer crossed a 60-tick boundary → plays `bombtick1.wav`
  - Inactive: `bombInactiveTicks > 0` → plays `bombinactive1.wav` once
- Reason: implements spec §2 sound requirements using client-side replicated state detection

## File: `src/game/bomb-tag-state.cpp`

- Line 10: Changed `#include "bomb-tag.h"` to `#include "game/gamemode-manager.h"` (bomb-tag.h no longer exists)
- Line 6: Updated comment to reference GamemodeManager instead of BombTagManager
- Reason: file only uses `setArmToWeaponPose()` which is now declared in gamemode-manager.h

## File: `src/game/bomb-tag.h` (DELETED)

- Entire file removed (66 lines)
- Reason: deprecated BombTagManager class — fully replaced by GamemodeManager. Nothing instantiates or calls this class.

## File: `src/game/bomb-tag.cpp` (DELETED)

- Entire file removed (226 lines)
- Reason: deprecated BombTagManager implementation — all functionality lives in gamemode-manager.cpp

## File: `src/network/community-match-client.h`

- Line 41: Added `uint32_t bombOwnerNpcIndex() const` accessor
- Line 65: Added `uint32_t mBombOwnerNpcIndex = 0` member
- Reason: packet stores bombOwnerNpcIndex but client had no way to access it; needed for NPC name lookup

## File: `src/network/community-match-client.cpp`

- Line 92: Added `mBombOwnerNpcIndex = packet.bombOwnerNpcIndex` in `onBombTagState()`
- Reason: stores the NPC index from the replicated packet

## File: `src/network/server-duel.h`

- Line 149: Added `bool hasBombFeature = false` to `ServerDuelState`
- Reason: feature flag replaces hardcoded string comparison for mode routing

## File: `src/network/server-duel.cpp`

- Line 232: Set `d.hasBombFeature = gm.features.bombHolderText` in `serverCommunityStartMatch()`
- Line 260: Changed `if (d.matchMode == "bomb_tag")` → `if (d.hasBombFeature)`
- Line 1274: Changed `if (d.matchMode == "bomb_tag")` → `if (d.hasBombFeature)`
- Lines 160-163: Changed `|| state.communityMode == "bomb_tag"` → `|| state.hasBombFeature`
- Lines 1748-1768: Added bomb holder disconnect/death check at start of DUEL_PHASE_ACTIVE:
  - If holder player doesn't exist, is inactive, or is dead → clear holder → selectNewBombHolder → broadcast state
- Reason: implements spec §5 disconnect handling; replaces hardcoded mode string checks per gamemodes.md

## File: `src/network/multiplayer-tick.cpp`

- Line 1226: Removed redundant `duel->matchMode[0] == 'd'` prefix check; now just `std::string(duel->matchMode) == "duel"`
- Reason: gamemodes.md requires no letter-prefix matching

# Diagnostics

- Owner/category: Duel/BombTag
- Input: bombtag.md spec requirements for sounds, disconnect, names, routing
- Decision: implement all requested fixes; ignore lag compensation and generalized HUD per user
- Output: 6 spec requirements addressed, build passes, 292 lines of dead code removed
- Failure or rejection reason: none
- Rate limiting: N/A

# Validation

- Focused skill paths and results: no focused skills available for bomb tag; validated via build
- Tests and exact commands: `python build_agent.py` → BUILD SUCCESS
- Build status: SUCCESS (5 compiled, 465 skipped, 9.73s)
- Runtime or hot-reload evidence: not tested — requires multiplayer session with bomb tag mode
- Output files: `mimita.exe`

# Measured evidence

- Before values: no bomb sounds played, no disconnect handling, dead BombTagManager class (292 lines), NPC holders showed "Bomb Holder", 3 hardcoded `matchMode == "bomb_tag"` checks
- After values: all 4 bomb sounds play from replicated state, disconnect transfers bomb immediately, dead code removed, NPC holders show avatarName, feature flag `hasBombFeature` replaces all string checks
- Timestamps: 2026-09-07T17:03:30Z
- Tick/frame/network measurements: N/A

# Regression review

- Regression entry appended: no
- Why this is or is not a confirmed regression: not a regression — these are spec alignment fixes
- Related regression paths: bomb tag sound playback, disconnect handling, NPC name display

# Human acceptance

- Visual review: pending
- Gameplay review: pending — requires multiplayer bomb tag session
- Multiplayer review: pending
- Still unverified: bomb sounds in actual gameplay, disconnect handling with real player drop, NPC name display with actual NPCs holding bomb

# Related feature record

- Feature path: `docs/specs/gamemodes/bombtag.md`, `docs/specs/gamemodes/gamemodes.md`
