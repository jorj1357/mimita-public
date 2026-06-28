#include "engine/engine-tick-ui.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <GLFW/glfw3.h>
#include "camera.h"
#include "entities/player.h"
#include "world/world.h"
#include "npc/npc.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/death-system.h"
#include "effects/effect-part.h"
#include "replay/replay.h"
#include "replay/replay-export.h"
#include "replay/replay-export-ui.h"
#include "replay/replay-factory.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "gui/hud/chat-bubble.h"
#include "gui/gui-editor.h"
#include "ui/hitmarker.h"
#include "crosshair/crosshair-render.h"
#include "crosshair/crosshair-config.h"
#include "devtools/dev-config.h"
#include "devtools/dev-npc-selection.h"
#include "devtools/dev-overlay.h"
#include "perf/perf.h"
#include "video/frame-pacer.h"
#include "shadow/shadow-config.h"
#include "shadow/shadow-render.h"
#include "render/post-fx.h"
#include "render/lighting-config.h"
#include "audio/music-manager.h"
#include "game/duel.h"
#include "game/bomb-tag.h"
#include "game/game-state.h"
#include "network/multiplayer-context.h"

#include "debug/debug-log.h"
#include "config/player-settings.h"
#include "npc/npc-combat.h"
#include "network/server.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;

void engineTickUIReplayHUD(Engine& engine, float dt)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    auto& mpContext = MP_CONTEXT;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayPlayer = REPLAY_PLAYER;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();

    GuiLayout& hudLayout = GuiLayoutManager::instance().getLayout("config/gui/hud.json");
    auto hudText = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = hudLayout.get(id);
        if (!el) return;
        float scale = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        glm::vec4 color = el->getTextColorVec();
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), scale, color);
    };

    drawHitmarker(dt);
    if (mpContext.active && !mpContext.connected)
    {
        const float boxW = 360.0f;
        const float boxX = (uiScreenW() - boxW) * 0.5f;
        hudText("connectionText", mpContext.connectionStatus);
    }
    if (gReplayRecorder.isRecording() && !gReplayExportRenderMode) {
        const float overlayX = uiScreenW() - 230.0f;
        uiDrawRect({overlayX - 18.0f, 20.0f, 12.0f, 12.0f},
                   {1.0f, 0.05f, 0.05f, 1.0f}, "replay-record-dot");
        uiDrawText("[REPLAY REC]", overlayX, 30.0f, 0.34f,
                   {1.0f, 0.12f, 0.12f, 1.0f});
        char replayTickText[64];
        snprintf(replayTickText, sizeof(replayTickText), "tick: %u",
                 gReplayRecorder.currentTick());
        uiDrawText(replayTickText, overlayX, 58.0f, 0.30f,
                   {1.0f, 0.12f, 0.12f, 1.0f});
    }
    if (replayPlaybackActive && !gReplayCinematicMode) {
        const float rOverlayX = uiScreenW() - 280.0f;
        const float rOverlayY = 20.0f;
        const auto* rFrame = gReplayPlayer.currentSceneFrame();
        const uint32_t totalTicks = gReplayPlayer.totalTicks();
        const float currentTime = gReplayPlayer.currentTick() / 60.0f;

        if (!gReplayExportRenderMode) {
        const float totalTime = (float)totalTicks / 60.0f;
        const char* camMode = gReplayPlayer.cameraController().modeName();
        const bool paused = gReplayPlayer.isPaused();

        const float labelX = rOverlayX + 12.0f;
        const float valueX = rOverlayX + 100.0f;
        const float lineH = 20.0f;
        const float bgW = 250.0f;
        float bgH = lineH * 5.0f + 16.0f;

        uiDrawRect({rOverlayX, rOverlayY, bgW, bgH},
                   {0.0f, 0.0f, 0.0f, 0.70f}, "replay-hud-bg");

        float y = rOverlayY + 8.0f;
        uiDrawText("Viewing:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
        if (rFrame && !rFrame->actors.empty())
            uiDrawText(rFrame->actors[0].name.c_str(), valueX, y, 0.28f, {1.0f, 1.0f, 1.0f, 1.0f});
        y += lineH;

        uiDrawText("Camera:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
        {
            char camBuf[64];
            snprintf(camBuf, sizeof(camBuf), "%s%s", camMode,
                     paused ? " [PAUSED]" : "");
            uiDrawText(camBuf, valueX, y, 0.28f,
                       paused ? glm::vec4{1.0f, 0.9f, 0.3f, 1.0f}
                              : glm::vec4{0.7f, 0.85f, 1.0f, 1.0f});
        }
        y += lineH;

        uiDrawText("Tick:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
        {
            char tickBuf[64];
            snprintf(tickBuf, sizeof(tickBuf), "%u / %u",
                     (unsigned)gReplayPlayer.currentTick(), (unsigned)totalTicks);
            uiDrawText(tickBuf, valueX, y, 0.28f, {0.9f, 0.9f, 0.3f, 1.0f});
        }
        y += lineH;

        uiDrawText("Time:", labelX, y, 0.28f, {0.6f, 0.6f, 0.6f, 1.0f});
        {
            auto formatTime = [](float seconds) -> std::string {
                int totalSec = (int)seconds;
                int mins = totalSec / 60;
                int secs = totalSec % 60;
                char buf[16];
                snprintf(buf, sizeof(buf), "%02d:%02d", mins, secs);
                return std::string(buf);
            };
            std::string curStr = formatTime(currentTime);
            std::string totStr = formatTime(totalTime);
            char timeBuf[64];
            snprintf(timeBuf, sizeof(timeBuf), "%s / %s", curStr.c_str(), totStr.c_str());
            uiDrawText(timeBuf, valueX, y, 0.28f, {0.9f, 0.9f, 0.3f, 1.0f});
        }
        y += lineH;

        if (totalTicks > 0) {
            const float barX = rOverlayX + 8.0f;
            const float barY = y + 4.0f;
            const float barW = bgW - 16.0f;
            const float barH = 6.0f;
            uiDrawRect({barX, barY, barW, barH}, {0.3f, 0.3f, 0.3f, 0.7f}, "seek-bg");
            float progress = std::clamp((float)gReplayPlayer.currentTick() / (float)std::max(totalTicks, 1u), 0.0f, 1.0f);
            uiDrawRect({barX, barY, barW * progress, barH}, {0.9f, 0.9f, 0.3f, 0.9f}, "seek-fill");
        }
        }

        // Game HUD (engineTickUIGameHUD) handles crosshair, ammo, health,
        // and other gameplay UI during replay. This replay HUD only adds
        // replay-specific overlays (timeline, controls help, healthbars).
        if (const ReplaySceneFrame* hbFrame = gReplayPlayer.currentSceneFrame()) {
            for (const ReplayActorState& actorState : hbFrame->actors) {
                auto mit = replayActorModels.find(actorState.id);
                if (mit == replayActorModels.end() || !mit->second || mit->second->dead) {
                    continue;
                }
                drawPlayerHealthbar(*mit->second, camera, "replay-hp");
            }
        }

        if (!gReplayExportRenderMode) {
        const float helpX = uiScreenW() - 220.0f;
        const float helpY = uiScreenH() - 140.0f;
        const float helpW = 200.0f;
        const float helpH = 110.0f;
        uiDrawRect({helpX, helpY, helpW, helpH},
                   {0.0f, 0.0f, 0.0f, 0.65f}, "replay-help-bg");
        float hy = helpY + 6.0f;
        uiDrawText("REPLAY CONTROLS", helpX + 8.0f, hy, 0.28f,
                   {0.9f, 0.9f, 0.3f, 1.0f}); hy += 18.0f;
        uiDrawText("SPACE    Pause/Resume", helpX + 8.0f, hy, 0.24f,
                   {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
        uiDrawText("<-       Back 5s", helpX + 8.0f, hy, 0.24f,
                   {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
        uiDrawText("->       Forward 5s", helpX + 8.0f, hy, 0.24f,
                   {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
        uiDrawText("L        Cinematic", helpX + 8.0f, hy, 0.24f,
                   {0.8f, 0.8f, 1.0f, 1.0f}); hy += 16.0f;
        uiDrawText("freecam  Free Camera", helpX + 8.0f, hy, 0.24f,
                   {0.8f, 0.8f, 1.0f, 1.0f});
        }
    }
}
