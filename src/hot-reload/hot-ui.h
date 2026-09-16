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

// Generic pause-menu state bridged from the cold modal: whether it is open and
// which logical view (pause.main / pause.settings / ...) is active. Hot policy
// composes the view; cold keeps the modal/focus mechanism. Not pause-specific
// rendering ABI.
// Generic server-listing facts (presentation entity per discovered server). Hot
// server-browser policy enumerates these; discovery/sockets/ping/connect stay
// cold. listingId is an opaque stable id; code is the cold connect key.
static constexpr std::uint64_t HOT_SERVER_LISTING_COMPONENT =
    gameHash("ServerListingState");
static constexpr std::uint32_t HOT_SERVER_LISTING_REACHABLE = 1u;
static constexpr std::uint32_t HOT_SERVER_LISTING_PASSWORD = 2u;
struct HotServerListingV1 {
    std::uint64_t listingId;
    std::int32_t players;
    std::int32_t maxPlayers;
    std::int32_t pingMs;
    std::uint32_t flags;      // HOT_SERVER_LISTING_*
    char code[16];
    char name[40];
    char map[24];
    char mode[16];
};

// Generic hot-owned text-field state (one focused field). The backend never owns
// the text; hot updates this from ui.action TEXT_INPUT/TEXT_SUBMIT and re-emits
// the GAME_UI_TEXT_INPUT widget. Bounded (no char* across generations).
static constexpr std::uint64_t HOT_UI_TEXT_COMPONENT = gameHash("HotUiTextState");
static constexpr std::uint32_t HOT_UI_TEXT_MAX = 32;
struct HotUiTextStateV1 {
    std::uint64_t elementId;   // which field is focused/edited
    char text[HOT_UI_TEXT_MAX];
    std::uint32_t reserved;
};

static constexpr std::uint64_t HOT_PAUSE_STATE_COMPONENT = gameHash("PauseMenuState");
struct HotPauseStateV1 {
    std::uint64_t viewHash;
    std::uint32_t visible;
    std::uint32_t reserved;
};

// Generic "scoreboard should be shown" state. Cold input maps the physical key
// (Tab hold) into this; hot owns what appears. Not scoreboard-specific beyond
// the logical visibility fact.
static constexpr std::uint64_t HOT_SCOREBOARD_VISIBLE_COMPONENT =
    gameHash("ScoreboardVisible");
struct HotScoreboardVisibleV1 {
    std::uint32_t visible;
    std::uint32_t reserved;
};

// Generic per-actor match stats (actor match FACTS, not UI rows). Joined by
// EntityId with ActorIdentityState/ActorTeamState for scoreboard composition.
static constexpr std::uint64_t HOT_ACTOR_STATS_COMPONENT =
    gameHash("ActorMatchStatsState");
struct HotActorMatchStatsV1 {
    std::int32_t score;
    std::int32_t rank;    // 0-based presentation rank hint
    std::uint32_t flags;  // bit0 = local player
    std::uint32_t reserved;
};

// Generic objective presentation state. One reusable component for any
// objective (bomb, capture point, payload, flag, control zone). Hot UI
// interprets stateHash; the kernel/backend never knows the objective kind.
static constexpr std::uint64_t HOT_OBJECTIVE_COMPONENT =
    gameHash("ObjectivePresentationState");
struct HotObjectiveStateV1 {
    std::uint64_t objectiveId;
    std::uint64_t stateHash;   // gameHash("bomb.planted") etc.
    std::uint32_t ownerTeam;
    float progress;            // 0..1 (plant/defuse/capture)
    float timer;               // seconds (detonation/capture)
    std::uint32_t flags;
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
    char value[32];      // optional bounded payload (e.g. server code)
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
