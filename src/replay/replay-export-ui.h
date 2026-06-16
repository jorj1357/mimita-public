#pragma once

// ============================================================
// REPLAY EXPORT UI FILTER
//
// Exported videos should look like normal gameplay footage.
// Set any of these to true to include them in exported videos.
// ============================================================

namespace ReplayExportUI {

// Death/respawn UI
static constexpr bool showDeathScreen   = false;
static constexpr bool showRespawnUI     = false;

// Replay-specific UI
static constexpr bool showReplayControls = false;
static constexpr bool showReplayTimeline = false;
static constexpr bool showReplayBrowser  = false;

// Export UI
static constexpr bool showExportProgress = false;

// Debug overlays
static constexpr bool showFps           = false;
static constexpr bool showPostFxDebug   = false;
static constexpr bool showShadowDebug   = false;
static constexpr bool showPerfOverlay   = false;
static constexpr bool showDevOverlay    = false;
static constexpr bool showDebugVis      = false;
static constexpr bool showDuelDebug     = false;
static constexpr bool showNpcDebug      = false;

// Development/informational HUD
static constexpr bool showSpeedDisplay  = false;
static constexpr bool showModeText      = false;
static constexpr bool showPlayerList    = false;
static constexpr bool showNetDebug      = false;

}
