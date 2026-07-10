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
#include "replay/replay-editor.h"
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

#include "devtools/terminal.h"
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
            // Keyframe markers on timeline when editor is loaded
            if (gReplayEditor.isLoaded()) {
                float markerH = barH + 4.0f;
                float markerY = barY - 2.0f;
                for (int ki = 0; ki < gReplayEditor.cameraKeyframeCount(); ++ki) {
                    float kfX = barX + (float)gReplayEditor.cameraKeyframe(ki).tick / (float)totalTicks * barW;
                    uiDrawRect({kfX - 1.0f, markerY, 3.0f, markerH}, {0.2f, 0.8f, 0.2f, 0.9f}, "kf-marker-cam");
                }
                for (int ki = 0; ki < gReplayEditor.cameraModeKeyframeCount(); ++ki) {
                    float kfX = barX + (float)gReplayEditor.cameraModeKeyframe(ki).tick / (float)totalTicks * barW;
                    uiDrawRect({kfX - 1.0f, markerY, 3.0f, markerH}, {0.2f, 0.5f, 1.0f, 0.9f}, "kf-marker-mode");
                }
                for (int ki = 0; ki < gReplayEditor.timeKeyframeCount(); ++ki) {
                    float kfX = barX + (float)gReplayEditor.timeKeyframe(ki).tick / (float)totalTicks * barW;
                    uiDrawRect({kfX - 1.0f, markerY, 3.0f, markerH}, {1.0f, 0.8f, 0.2f, 0.9f}, "kf-marker-time");
                }
            }
        }
        }

        // Game HUD (engineTickUIGameHUD) handles crosshair, ammo, health,
        // and other gameplay UI during replay. This replay HUD only adds
        // replay-specific overlays (timeline, controls help, healthbars).
        if (const ReplaySceneFrame* hbFrame = gReplayPlayer.currentSceneFrame()) {
            for (const ReplayActorState& actorState : hbFrame->actors) {
                if (actorState.dead || actorState.health <= 0)
                    continue;
                auto mit = replayActorModels.find(actorState.id);
                if (mit == replayActorModels.end() || !mit->second)
                    continue;
                drawPlayerHealthbar(*mit->second, camera, "replay-hp", "replay_frame");
            }
        }

        if (!gReplayExportRenderMode) {
        const bool editorActive = gReplayEditor.isLoaded();
        const float helpX = uiScreenW() - 230.0f;
        const float helpW = 210.0f;
        int lineCount = editorActive ? 9 : 5;
        const float helpH = (float)(lineCount * 16 + 12);
        const float helpY = uiScreenH() - helpH - 10.0f;
        uiDrawRect({helpX, helpY, helpW, helpH},
                   {0.0f, 0.0f, 0.0f, 0.65f}, "replay-help-bg");
        float hy = helpY + 6.0f;
        uiDrawText(editorActive ? "EDITOR CONTROLS" : "REPLAY CONTROLS",
                   helpX + 8.0f, hy, 0.28f,
                   {0.9f, 0.9f, 0.3f, 1.0f}); hy += 16.0f;
        if (editorActive) {
            uiDrawText("SPACE    Play/Pause", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("F        Toggle Freecam", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("K        Keyframe", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("<-/->    Seek 1s", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("Sh+Up/Dn KF Nav", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("Ctrl+Z   Undo", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("WASD+QE  Fly", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("rplehelp for more", helpX + 8.0f, hy, 0.24f,
                       {0.5f, 0.5f, 0.7f, 1.0f});
        } else {
            uiDrawText("SPACE    Pause/Resume", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("<-       Back 5s", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("->       Forward 5s", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("L        Cinematic", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f}); hy += 15.0f;
            uiDrawText("F3       Save replay", helpX + 8.0f, hy, 0.24f,
                       {0.8f, 0.8f, 1.0f, 1.0f});
        }
        }
    }

    // ── Replay Editor Keyframe Popup ──────────────────────────
    if (gReplayEditor.keyframePromptStage > 0) {
        GLFWwindow* win = engine.window();
        float fbW = uiScreenW();
        float fbH = uiScreenH();
        float pw = 400.0f;
        float ph = 280.0f;

        // Full-screen dim overlay
        uiDrawRect({0.0f, 0.0f, fbW, fbH}, {0.0f, 0.0f, 0.0f, 0.55f}, "kf-dim");

        // Centered popup box
        float px = (fbW - pw) * 0.5f;
        float py = (fbH - ph) * 0.5f;
        uiDrawRect({px, py, pw, ph}, {0.12f, 0.12f, 0.15f, 1.0f}, "kf-bg");
        uiDrawRectOutline({px, py, pw, ph}, {0.3f, 0.3f, 0.4f, 1.0f}, "kf-border");

        float textX = px + 20.0f;
        float ty = py + 16.0f;

        if (gReplayEditor.keyframePromptStage == 1) {
            // Title
            uiDrawText("Create Keyframe", textX, ty, 0.40f, {1.0f, 1.0f, 0.8f, 1.0f});
            ty += 34.0f;

            float bw = pw - 40.0f;
            float bh = 32.0f;
            float by = ty;

            // Button 1: Camera Position
            UIButtonState b1 = uiButton(win, "1  Camera Position",
                {textX, by, bw, bh}, {0.2f, 0.25f, 0.35f, 1.0f}, "kf-btn-1");
            if (b1.clicked) {
                glm::vec3 pos = gReplayEditor.freecam
                    ? gReplayEditor.freecamPos : THE_CAMERA.pos;
                glm::quat rot = gReplayEditor.freecam
                    ? gReplayEditor.freecamRot
                    : glm::quatLookAt(glm::normalize(THE_CAMERA.front), glm::vec3(0,0,1));
                gReplayEditor.addCameraKeyframe(
                    gReplayEditor.keyframePromptTick, pos, rot,
                    gReplayEditor.freecamRoll, gReplayEditor.freecamFov,
                    gReplayEditor.defaultInterp);
                Terminal::instance().addLog(
                    "[RPLE] Camera position keyframe at tick " +
                    std::to_string(gReplayEditor.keyframePromptTick));
                gReplayEditor.keyframePromptStage = 0;
            }
            by += bh + 8.0f;

            // Button 2: Camera Mode
            UIButtonState b2 = uiButton(win, "2  Camera Mode",
                {textX, by, bw, bh}, {0.2f, 0.25f, 0.35f, 1.0f}, "kf-btn-2");
            if (b2.clicked) {
                gReplayEditor.keyframePromptStage = 2;
            }
            by += bh + 8.0f;

            // Button 3: Playback Speed
            UIButtonState b3 = uiButton(win, "3  Playback Speed",
                {textX, by, bw, bh}, {0.2f, 0.25f, 0.35f, 1.0f}, "kf-btn-3");
            if (b3.clicked) {
                gReplayEditor.keyframePromptStage = 3;
                gReplayEditor.pbspeedInputBuf[0] = '\0';
                gReplayEditor.pbspeedInputLen = 0;
            }

            uiDrawText("ESC to cancel", textX, py + ph - 22.0f, 0.26f, {0.5f, 0.5f, 0.6f, 1.0f});

        } else if (gReplayEditor.keyframePromptStage == 2) {
            // Camera mode sub-prompt
            uiDrawText("Camera Mode", textX, ty, 0.40f, {1.0f, 1.0f, 0.8f, 1.0f});
            ty += 34.0f;

            float bw = pw - 40.0f;
            float bh = 32.0f;
            float by = ty;

            UIButtonState b1 = uiButton(win, "1  Third Person",
                {textX, by, bw, bh}, {0.2f, 0.25f, 0.35f, 1.0f}, "kf-mode-1");
            if (b1.clicked) {
                gReplayEditor.addCameraModeKeyframe(
                    gReplayEditor.keyframePromptTick, ReplayEditorCamMode::ThirdPerson);
                gReplayEditor.keyframePromptStage = 0;
            }
            by += bh + 8.0f;

            UIButtonState b2 = uiButton(win, "2  Freecam",
                {textX, by, bw, bh}, {0.2f, 0.25f, 0.35f, 1.0f}, "kf-mode-2");
            if (b2.clicked) {
                gReplayEditor.addCameraModeKeyframe(
                    gReplayEditor.keyframePromptTick, ReplayEditorCamMode::Freecam);
                gReplayEditor.keyframePromptStage = 0;
            }
            by += bh + 8.0f;

            UIButtonState b3 = uiButton(win, "3  First Person",
                {textX, by, bw, bh}, {0.2f, 0.25f, 0.35f, 1.0f}, "kf-mode-3");
            if (b3.clicked) {
                gReplayEditor.addCameraModeKeyframe(
                    gReplayEditor.keyframePromptTick, ReplayEditorCamMode::FirstPerson);
                gReplayEditor.keyframePromptStage = 0;
            }

            uiDrawText("ESC to cancel", textX, py + ph - 22.0f, 0.26f, {0.5f, 0.5f, 0.6f, 1.0f});

        } else if (gReplayEditor.keyframePromptStage == 3) {
            // Playback speed value input
            uiDrawText("Playback Speed Keyframe", textX, ty, 0.40f, {1.0f, 1.0f, 0.8f, 1.0f});
            ty += 34.0f;

            uiDrawText("Enter playback speed multiplier (0.01-100):",
                textX, ty, 0.28f, {0.8f, 0.8f, 1.0f, 1.0f});
            ty += 26.0f;

            const char* examples = "Examples:  0.1  0.25  0.5  1  2  5  10";
            uiDrawText(examples, textX, ty, 0.24f, {0.6f, 0.6f, 0.8f, 1.0f});
            ty += 26.0f;

            // Input box
            float inputW = pw - 40.0f;
            float inputH = 32.0f;
            float inputX = textX;
            float inputY = ty;
            uiDrawRect({inputX, inputY, inputW, inputH}, {0.08f, 0.08f, 0.1f, 1.0f}, "kf-speed-input-bg");
            uiDrawRectOutline({inputX, inputY, inputW, inputH}, {0.4f, 0.4f, 0.6f, 1.0f}, "kf-speed-input-border");

            std::string displayText = gReplayEditor.pbspeedInputLen > 0
                ? std::string(gReplayEditor.pbspeedInputBuf) + "x"
                : "_";
            uiDrawText(displayText.c_str(), inputX + 10.0f, inputY + 4.0f, 0.32f, {1.0f, 1.0f, 1.0f, 1.0f});
            ty += inputH + 12.0f;

            uiDrawText("Enter to confirm, ESC to cancel",
                textX, ty, 0.26f, {0.5f, 0.5f, 0.6f, 1.0f});
            ty += 20.0f;

            // Current value preview
            if (gReplayEditor.pbspeedInputLen > 0) {
                float previewSpeed = 1.0f;
                try {
                    previewSpeed = std::clamp(std::stof(gReplayEditor.pbspeedInputBuf), 0.01f, 100.0f);
                } catch (...) {}
                char preview[64];
                std::snprintf(preview, sizeof(preview), "Preview: %.2fx playback speed", previewSpeed);
                uiDrawText(preview, textX, ty, 0.28f, {0.6f, 1.0f, 0.6f, 1.0f});
            }
        }
    }
}
