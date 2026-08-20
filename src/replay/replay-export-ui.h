// 08 19 2026, 09 50
/* purpose
* Defines the JSON-driven visibility policy for replay export UI categories.
* Keeps cinematic/debug exclusions separate from normal gameplay HUD elements.
* Lets existing HUD owners decide whether optional overlays appear in exports.
* Does NOT render UI, bind framebuffers, or capture video pixels.
* Does NOT own replay job state, export resolution, or encoder selection.
* Does NOT change normal gameplay or replay playback visibility.
*/
#pragma once

#include "replay/replay-export.h"

// ============================================================
// REPLAY EXPORT UI FILTER
//
// Exported videos should look like normal gameplay footage. Values are loaded
// from config/replayexport.json and affect export renders only.
// ============================================================

namespace ReplayExportUI {

// Death/respawn UI
inline bool showDeathScreen() { return gExportConfig.ui.deathScreen; }
inline bool showRespawnUI() { return gExportConfig.ui.deathScreen; }

// Chat is split because the 2D history and world-space bubbles are separate.
inline bool showChat() { return gExportConfig.ui.chat; }
inline bool showChatBubbles() { return gExportConfig.ui.chatBubbles; }

// Replay-specific UI
inline bool showReplayInfo() { return gExportConfig.ui.replayInfo; }
inline bool showReplayControls() { return gExportConfig.ui.replayControls; }
inline bool showReplayTimeline() { return gExportConfig.ui.replayTimeline; }
inline bool showReplayBrowser() { return gExportConfig.ui.replayBrowser; }

// Export UI
inline bool showExportProgress() { return gExportConfig.ui.exportProgress; }

// Debug overlays
inline bool showFps() { return gExportConfig.ui.fps; }
inline bool showPostFxDebug() { return gExportConfig.ui.postFxDebug; }
inline bool showShadowDebug() { return gExportConfig.ui.shadowDebug; }
inline bool showPerfOverlay() { return gExportConfig.ui.performanceOverlay; }
inline bool showDevOverlay() { return gExportConfig.ui.devOverlay; }
inline bool showDebugVis() { return gExportConfig.ui.debugVisuals; }
inline bool showDuelDebug() { return gExportConfig.ui.duelDebug; }
inline bool showNpcDebug() { return gExportConfig.ui.npcDebug; }

// Development/informational HUD
inline bool showSpeedDisplay() { return gExportConfig.ui.speedDisplay; }
inline bool showModeText() { return gExportConfig.ui.modeText; }
inline bool showPlayerList() { return gExportConfig.ui.playerList; }
inline bool showNetDebug() { return false; }

}
