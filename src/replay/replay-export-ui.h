// 08 19 2026, 09 50
/* purpose
* Defines the compile-time visibility policy for replay export UI categories.
* Keeps cinematic/debug exclusions separate from normal gameplay HUD elements.
* Lets existing HUD owners decide whether optional overlays appear in exports.
* Does NOT render UI, bind framebuffers, or capture video pixels.
* Does NOT own replay job state, export resolution, or encoder selection.
* Does NOT change normal gameplay or replay playback visibility.
*/
#pragma once

// ============================================================
// REPLAY EXPORT UI FILTER
//
// Exported videos should look like normal gameplay footage.
// Set any of these to true to include them in exported videos.
// ============================================================

namespace ReplayExportUI {

// Death/respawn UI
static constexpr bool showDeathScreen   = true;
static constexpr bool showRespawnUI     = true;

// Replay-specific UI
static constexpr bool showReplayControls = true;
static constexpr bool showReplayTimeline = true;
static constexpr bool showReplayBrowser  = true;

// Export UI
static constexpr bool showExportProgress = true;

// Debug overlays
static constexpr bool showFps           = true;
static constexpr bool showPostFxDebug   = false;
static constexpr bool showShadowDebug   = false;
static constexpr bool showPerfOverlay   = false;
static constexpr bool showDevOverlay    = false;
static constexpr bool showDebugVis      = false;
static constexpr bool showDuelDebug     = false;
static constexpr bool showNpcDebug      = false;

// Development/informational HUD
static constexpr bool showSpeedDisplay  = true;
static constexpr bool showModeText      = true;
static constexpr bool showPlayerList    = true;
static constexpr bool showNetDebug      = false;

}
