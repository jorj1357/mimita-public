// 09 06 2026, 18 00
/* purpose
* Record the VIP NameStyle migration: admin color fix, 5 game surfaces migrated to VIP rendering, style_revision added
* Covers chat window, match leaderboard, duel scoreboard, account panel, HUD playerName
* Covers database migration, API propagation, and game-side epoch tracking
* DOES NOT fix pre-existing bomb-tag and server-duel build errors
*/

# VIP NameStyle Migration — 09 06 2026

## Branch
`8292026stash`

## Time
~17:52–18:00 EST

## Pre-existing Changes
40 files were already modified before this session (bomb-tag refactor, hit effects, server-duel, etc.). Build fails on pre-existing bomb-tag and server-duel errors unrelated to VIP work.

## Changes Made

### Step 1: Admin Color Fix
**File:** `src/website/api-client.cpp:158`
**Old:** `if (role == "admin") return "#191919";`
**New:** `if (role == "admin") return "#000000";`
**Reason:** Game fallback color for admin was `#191919` (dark gray) but website authoritative `STAFF_ROLE_COLORS.admin` is `#000000` (pure black). Now they match.

### Step 2a: Chat Window VIP Styling
**File:** `src/gui/hud/chat-window.cpp`
**Changes:** Refactored text wrapping and rendering to split player messages into name prefix + message text. First line renders sender name with `vipDrawStyledName()` using `entry.senderVipAppearance` and `entry.senderVipStyleDetail`. Subsequent wrapped lines render in plain white. Server messages remain gold. Added `namePrefixWidths` vector for tracking name pixel width per message.

### Step 2b: Match FFA Leaderboard VIP Styling
**Files:** `src/gui/hud/match-leaderboard.h`, `src/gui/hud/match-leaderboard.cpp`, `src/network/community-match-client.cpp`, `src/duel/duel-queue.cpp`
**Changes:** Added `VipAppearance` and `VipStyleDetail` fields to `MatchLeaderboardEntry`. Both `community-match-client.cpp` and `duel-queue.cpp` now populate VIP data from `AuthSystem` (local player) and `MP_CONTEXT.remotePlayers` (remote players) when building the leaders list. Rendering uses `vipDrawStyledName()` for names with `: score` appended in rank color.

### Step 2c: Duel Scoreboard VIP Styling
**Files:** `src/duel/duel-ui.cpp`, `src/duel/duel-queue.h`
**Changes:** Added `enemyPlayerId()` public accessor to `DuelQueue`. Replaced plain-text scoreboard with separate VIP-styled renders for player name, score text, and opponent name. Local player VIP from `AuthSystem`, opponent VIP from `MP_CONTEXT.remotePlayers`.

### Step 2d: Account Panel VIP Styling
**File:** `src/gui/menus/account-panel.cpp`
**Changes:** Replaced `drawGuiElement` username rendering with `vipDrawStyledNameCentered()` using `auth.user().vipAppearance` and `auth.user().vipStyleDetail`.

### Step 2e: HUD playerName VIP Styling
**File:** `src/engine/engine-tick-ui-game-hud.cpp`
**Changes:** Replaced `hudText("playerName", ...)` lambda call with `vipDrawStyledNameCentered()` using `player.vipAppearance` and `player.vipStyleDetail`. Falls back to plain `uiDrawText` for replay actors (which lack VIP data).

### Step 3: Style Revision System
**Files:** `website/server/db.js`, `website/server/vip-entitlements.js`, `website/server/vip-routes.js`, `website/server/admin.js`, `src/website/api-client.cpp`, `src/network/server-packets.cpp`

**Database:** Added `style_revision INT NOT NULL DEFAULT 1` column to `vip_name_styles` table via CREATE TABLE and ALTER TABLE migration.

**API:** `computeVipState()` now returns `style_revision` in the VIP state object. The style query selects `style_json, style_revision`.

**Style save:** Both `PATCH /style` and `POST /style/reset` endpoints increment `style_revision` on every save using `ON CONFLICT DO UPDATE SET style_revision = vip_name_styles.style_revision + 1`.

**Admin style:** `POST /admin/vip/style` also increments revision.

**Game parsing:** `parseVipStyleDetailJson()` reads `vip.style_revision` and maps it to `VipStyleDetail.styleEpoch`.

**Server join:** `server-packets.cpp` now uses `vipResult.vipStyleDetail.styleEpoch` instead of hardcoded `1` for the initial `vipStyleEpoch`.

### Test Fix
**File:** `website/server/vip-payments.test.js:207`
**Old:** `text.startsWith("SELECT style_json FROM vip_name_styles")`
**New:** `text.startsWith("SELECT style_json") && text.includes("FROM vip_name_styles")`
**Reason:** Query changed to include `style_revision` column.

## Test Results

| Test Suite | Pass | Fail | Notes |
|-----------|------|------|-------|
| vip-entitlements.test.js | 15 | 0 | All pass |
| vip-payments.test.js | 18 | 2 | 2 pre-existing failures (subscription checkout, syncActiveSubscriptions) |

## Build Result
**FAILED** — pre-existing errors in `bomb-tag-state.cpp`, `bomb-tag.cpp`, `engine-tick-replay.cpp`, `engine-tick-combat.cpp`, `engine-tick-state.cpp`, `server-duel.cpp`. None caused by this session's VIP changes. `engine-tick-ui-game-hud.cpp` (my most complex change) compiled successfully.

## Files Modified (VIP-related only)
1. `src/website/api-client.cpp` — admin color fix + style_revision parsing
2. `src/gui/hud/chat-window.cpp` — VIP chat names
3. `src/gui/hud/match-leaderboard.h` — VIP data in MatchLeaderboardEntry
4. `src/gui/hud/match-leaderboard.cpp` — VIP leaderboard rendering
5. `src/gui/menus/account-panel.cpp` — VIP account panel name
6. `src/engine/engine-tick-ui-game-hud.cpp` — VIP HUD playerName
7. `src/duel/duel-ui.cpp` — VIP duel scoreboard
8. `src/duel/duel-queue.h` — enemyPlayerId accessor
9. `src/duel/duel-queue.cpp` — VIP data for duel leaders
10. `src/network/community-match-client.cpp` — VIP data for match leaders
11. `src/network/server-packets.cpp` — use styleEpoch from website
12. `website/server/db.js` — style_revision column
13. `website/server/vip-entitlements.js` — style_revision in response
14. `website/server/vip-routes.js` — increment style_revision
15. `website/server/admin.js` — increment style_revision
16. `website/server/vip-payments.test.js` — fix test dispatcher query match
