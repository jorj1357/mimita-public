// 09 14 2026
/* purpose
* Shared hot HUD/UI state. One generic `MatchHudState` dynamic component carries
* the data a hot ui.frame system needs to compose a match HUD (timer, scores,
* phase). The mode that owns match state writes it; the HUD reads it. Hot-only
* header: not a GameAPI context field.
* Does NOT link into the EXE.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

static constexpr std::uint64_t HOT_MATCH_HUD_COMPONENT = gameHash("MatchHudState");

// Hot/shared mode-HUD ownership claim. When a hot composition fully covers the
// active mode's HUD, the cold client HUD composition yields (one owner). Partial
// coverage keeps the cold fallback.
static constexpr std::uint64_t HOT_MODE_HUD_CLAIM_COMPONENT =
    gameHash("ModeHudClaim");
struct HotModeHudClaimV1 {
    std::uint32_t owned;
    std::uint32_t reserved;
};

// Hot/shared UI-screen ownership claim. When a hot composition owns a screen's
// UI, the cold legacy composition for that screen yields (one owner).
static constexpr std::uint64_t HOT_UI_CLAIM_COMPONENT = gameHash("HotUiClaim");
struct HotUiClaimV1 {
    std::uint64_t screenId;
    std::uint32_t owned;
    std::uint32_t reserved;
};

// Generation-migratable hot UI navigation state. Stored as a dynamic component
// so it belongs to the running session, not the code DLL: a hot generation swap
// must NOT reset the current screen. Hash ids only (no cold screen enum).
static constexpr std::uint64_t HOT_UI_NAV_COMPONENT =
    gameHash("HotUiNavigationState");
struct HotUiNavigationStateV1 {
    std::uint64_t screenId;
    std::uint64_t previousScreenId;
    std::uint64_t modalId;
    std::uint64_t focusId;
    std::uint32_t flags;
    std::uint32_t reserved;
};

// Generic menu-shell data (profile/version/connection), projected from typed
// cold sources so hot menu policy composes chrome without reading cold GUI or
// account objects. Reusable by any hot screen that shows account/status chrome.
static constexpr std::uint64_t HOT_MENU_SHELL_COMPONENT = gameHash("MenuShellState");
struct HotMenuShellStateV1 {
    char username[32];
    char version[24];
    char avatar[24];
    std::uint32_t flags;       // bit0 = signed in, bit1 = account available
    std::uint32_t connection;  // generic connection state id (0 = none)
    std::int32_t mmr;
    std::int32_t wins;
    std::int32_t losses;
    std::int32_t kills;
    std::int32_t deaths;
    std::uint32_t tier;        // VIP/supporter tier (presentation fact)
    std::uint32_t reserved;
};

// Generic pending UI action for COLD secure/screen transitions. Hot UI routes a
// logical action id here; the cold menu layer (which runs every menu frame)
// consumes it and performs the actual auth/screen work (tokens/passwords stay
// cold). Generation-safe: an id, never a callback pointer.
static constexpr std::uint64_t HOT_UI_PENDING_ACTION_COMPONENT =
    gameHash("HotUiPendingAction");
struct HotUiPendingActionV1 {
    std::uint64_t actionId;
    std::uint32_t seq;   // increments per request; cold clears after handling
    std::uint32_t reserved;
};

struct HotMatchHudStateV1 {
    float timerSeconds;
    std::int32_t scoreA;
    std::int32_t scoreB;
    std::uint32_t phase;   // 0 none, 1 countdown, 2 active, 3 intermission, 4 results
    char phaseText[32];
    char labelA[24];
    char labelB[24];
};
