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
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/hud/player-nameplates.h"
#include "gui/hud/chat-bubble.h"
#include "gui/gui-editor.h"
#include "competitive/competitive-match.h"
#include "competitive/competitive-ui.h"
#include "gui/gui-main.h"
#include "game/game-state.h"
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
#include "gui/menus/online-menu.h"

#include "debug/debug-log.h"
#include "config/player-settings.h"
#include "npc/npc-combat.h"
#include "network/server.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;
extern bool gRoomCodeShow;

void engineTickUIOverlays(Engine& engine, float dt, bool worldPassRan)
{
    Player& player = THE_PLAYER;
    Camera& camera = THE_CAMERA;
    World& world = THE_WORLD;
    NpcSystem& npcSystem = THE_NPC_SYSTEM;
    WeaponSystem& weapons = THE_WEAPONS;
    bool& worldLoaded = WORLD_LOADED;
    GameState& gameState = GAME_STATE;
    bool& editorMode = EDITOR_MODE;
    std::string& activeGameMode = ACTIVE_GAME_MODE;
    std::string& activeMapPath = ACTIVE_MAP_PATH;
    bool& freecamEnabled = FREECAM_ENABLED;
    auto& replayActorModels = REPLAY_ACTOR_MODELS;
    auto& mpContext = MP_CONTEXT;
    auto& gReplayRecorder = REPLAY_RECORDER;
    auto& gReplayPlayer = REPLAY_PLAYER;
    auto& gReplayBrowser = REPLAY_BROWSER;
    auto& gReplayTimeline = REPLAY_TIMELINE;

    const bool replayPlaybackActive = gReplayPlayer.isPlaying();
    GuiLayout& hudLayout = GuiLayoutManager::instance().getLayout("config/gui/hud.json");

    if (gDuelManager.phase() == DuelPhase::MatchEnd &&
        (!gReplayExportRenderMode || ReplayExportUI::showDuelDebug))
    {
        const char* stateName = "None";
        switch (gDuelManager.endState()) {
        case DuelEndState::None:          stateName = "None"; break;
        case DuelEndState::VictoryScreen: stateName = "VictoryScreen"; break;
        case DuelEndState::Countdown:     stateName = "Countdown"; break;
        case DuelEndState::FinalKillReplay: stateName = "FinalKillReplay"; break;
        case DuelEndState::ReplayMenu:    stateName = "ReplayMenu"; break;
        }
        float y = 20.0f;
        char buf[128];
        snprintf(buf, sizeof(buf), "DUEL STATE: %s", stateName);
        uiDrawText(buf, 24, y, 0.35f, {0.3f, 1.0f, 0.3f, 1.0f}); y += 20.0f;
        snprintf(buf, sizeof(buf), "ReplayReady: %d", (int)gDuelManager.isReplayReady());
        uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
        snprintf(buf, sizeof(buf), "ReplayLoaded: %s", gReplayPlayer.totalTicks() > 0 ? "YES" : "NO");
        uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
        snprintf(buf, sizeof(buf), "ReplayPlaying: %d", (int)gReplayPlayer.isPlaying());
        uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
        snprintf(buf, sizeof(buf), "CurrentReplayTick: %u/%u", gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
        uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
        const ReplayExportJob& job = getReplayExportJob();
        const char* exportState = "Idle";
        switch (job.state) {
        case ReplayExportJob::Idle:      exportState = "Idle"; break;
        case ReplayExportJob::Capturing: exportState = "Capturing"; break;
        case ReplayExportJob::Encoding:  exportState = "Encoding"; break;
        case ReplayExportJob::Done:      exportState = "Done"; break;
        case ReplayExportJob::Failed:    exportState = "Failed"; break;
        }
        snprintf(buf, sizeof(buf), "ExportInProgress: %s", exportState);
        uiDrawText(buf, 24, y, 0.35f, {1.0f, 1.0f, 1.0f, 1.0f}); y += 18.0f;
        snprintf(buf, sizeof(buf), "ReplayPath: %s", gDuelManager.finalKillReplayPath.c_str());
        uiDrawText(buf, 24, y, 0.30f, {0.8f, 0.8f, 0.8f, 1.0f});
    }
    if (gDuelManager.endState() == DuelEndState::FinalKillReplay) {
        float elapsed = gReplayPlayer.totalTicks() > 0
            ? (float)gReplayPlayer.currentTick() / 60.0f
            : 0.0f;
        float factor = 1.0f;
        if (elapsed < 2.0f) {
            factor = 0.15f;
        } else if (elapsed < 3.5f) {
            float p = (elapsed - 2.0f) / 1.5f;
            factor = 0.15f + p * 0.85f;
        }
        gReplayPlayer.setTimescale(factor);
    }

    if (gDuelManager.phase() == DuelPhase::MatchEnd) {
        // Intercept competitive match end immediately to show competitive result screen
        if (isCompetitiveMatchActive())
        {
            gameState = GAME_MENU;
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            gGuiMenuState = GUI_MENU_COMPETITIVE_RESULT;
            gDuelManager.stopDuel();
            gBombTagManager.stop();
            npcSystem.destroyAll();
            gReplayPlayer.stopPlayback();
            if (gReplayRecorder.isRecording())
                gReplayRecorder.stopRecording();
        }

        DuelMenuAction action = gDuelManager.renderMatchOverScreen(engine.window());
        if (action == DuelMenuAction::PlayAgain) {
            gDuelManager.restartDuel(player, npcSystem, world);
        } else if (action == DuelMenuAction::ExitToMenu) {
            gReplayPlayer.stopPlayback();
            if (gReplayRecorder.isRecording())
                gReplayRecorder.stopRecording();
            gDuelManager.stopDuel();
            gBombTagManager.stop();
            npcSystem.destroyAll();
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            gameState = GAME_MENU;
        } else if (action == DuelMenuAction::SaveReplay) {
            if (!gDuelManager.finalKillReplayPath.empty()) {
                std::string jsonPath = gDuelManager.finalKillReplayPath;
                int rw = engine.renderer ? engine.renderer->width : 1280;
                int rh = engine.renderer ? engine.renderer->height : 720;
                if (startReplayExport(jsonPath, rw, rh)) {
                    DevOverlay::instance().showNotification("Exporting replay...", 2.0f);
                }
            } else {
                DevOverlay::instance().showNotification("Replay not ready yet. Wait for replay to load.", 5.0f);
            }
        }
    } else if (gBombTagManager.phase() == BombTagPhase::MatchEnd) {
        BombTagMenuAction btAction = gBombTagManager.renderMatchOverScreen(engine.window());
        if (btAction == BombTagMenuAction::PlayAgain) {
            BombTagConfig cfg;
            cfg.numNpcs = 3;
            cfg.npcDifficulty = 5.0f;
            cfg.lives = 0;
            cfg.timeLimitSeconds = 180;
            cfg.enabled = true;
            cfg.mapPath = activeMapPath;
            npcSystem.destroyAll();
            gBombTagManager.setCamera(camera);
            gBombTagManager.start(cfg, player, npcSystem, world);
        } else if (btAction == BombTagMenuAction::ExitToMenu) {
            gReplayPlayer.stopPlayback();
            if (gReplayRecorder.isRecording())
                gReplayRecorder.stopRecording();
            gBombTagManager.stop();
            gDuelManager.stopDuel();
            npcSystem.destroyAll();
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            gameState = GAME_MENU;
        }
    } else {
        if (gDuelManager.enabled())
            gDuelManager.renderHud();
        if (gBombTagManager.enabled())
            gBombTagManager.renderHud();
    }

    if (mpContext.active && mpContext.showPlayerList)
    {
        float listX = uiScreenW() * 0.5f - 160.0f;
        float listY = uiScreenH() * 0.25f;
        float listW = 320.0f;
        float lineH = 24.0f;
        float headerH = 30.0f;

        size_t totalPlayers = mpContext.playerRegistry.size();
        float listH = headerH + (totalPlayers + 1) * lineH + 10.0f;

        uiDrawRect({listX, listY, listW, listH}, {0.0f, 0.0f, 0.0f, 0.85f}, "player-list-bg");
        uiDrawRectOutline({listX, listY, listW, listH}, {0.5f, 0.6f, 0.8f, 1.0f}, "player-list-border");

        float y = listY + 8.0f;
        uiDrawText("PLAYERS", listX + 10.0f, y, 0.36f, {0.8f, 0.9f, 1.0f, 1.0f});
        y += headerH;
        uiDrawText("ID   NAME                         PING",
                   listX + 10.0f, y, 0.28f, {0.65f, 0.75f, 0.9f, 1.0f});
        y += lineH;

        if (mpContext.localPlayerId)
        {
            const char* localName = player.username.empty() ? "you" : player.username.c_str();
            char localLine[128];
            snprintf(localLine, sizeof(localLine), "%u   %s   %dms (you)",
                     mpContext.localPlayerId, localName, mpContext.localPingMs);
            uiDrawText(localLine, listX + 10.0f, y, 0.32f, {0.3f, 1.0f, 0.4f, 1.0f});
            y += lineH;
        }

        for (const auto& kv : mpContext.playerRegistry)
        {
            if (kv.first == mpContext.localPlayerId)
                continue;
            const char* pname = kv.second.name.c_str();
            char remoteLine[128];
            snprintf(remoteLine, sizeof(remoteLine), "%u  %s  %dms",
                     kv.first, pname, kv.second.pingMs);
            uiDrawText(remoteLine, listX + 10.0f, y, 0.32f, {0.9f, 0.95f, 1.0f, 1.0f});
            y += lineH;
        }
    }

    if (mpContext.active && mpContext.showDebugOverlay)
    {
        float dbgX = uiScreenW() - 360.0f;
        float dbgY = 20.0f;
        float lineH = 18.0f;
        float dbgW = 340.0f;
        float dbgH = (13.0f + (float)mpContext.remotePlayers.size()) * lineH + 10.0f;

        uiDrawRect({dbgX, dbgY, dbgW, dbgH}, {0.0f, 0.0f, 0.0f, 0.8f}, "net-debug-bg");

        float y = dbgY + 6.0f;
        char buf[256];

        snprintf(buf, sizeof(buf), "STATUS: %s", mpContext.connectionStatus.c_str());
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                   mpContext.connected ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f)
                                       : glm::vec4(1.0f, 0.55f, 0.2f, 1.0f));
        y += lineH;

        snprintf(buf, sizeof(buf), "LOCAL PLAYER ID: %u", mpContext.localPlayerId);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.3f, 1.0f, 0.4f, 1.0f}); y += lineH;

        snprintf(buf, sizeof(buf), "PING: %dms", mpContext.localPingMs);
        uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.75f, 0.85f, 1.0f, 1.0f});
        y += lineH;

        snprintf(buf, sizeof(buf), "ENTITIES: %zu (PLAYERS %zu / NPCS %zu)",
                 mpContext.remotePlayers.size() + mpContext.remoteNpcs.size() +
                     (mpContext.localPlayerId ? 1u : 0u),
                 mpContext.remotePlayers.size() + (mpContext.localPlayerId ? 1u : 0u),
                 mpContext.remoteNpcs.size());
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

        snprintf(buf, sizeof(buf), "TICK CLIENT %u / SERVER %llu",
                 mpContext.tick, (unsigned long long)mpContext.lastSnapshotTick);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

        const uint64_t snapshotAge = mpContext.lastSnapshotReceivedMs
            ? MimitaNet::nowMs() - mpContext.lastSnapshotReceivedMs
            : 0;
        snprintf(buf, sizeof(buf), "SNAPSHOT AGE: %llums",
                 (unsigned long long)snapshotAge);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

        const uint64_t snapshotTotal =
            mpContext.snapshotsReceived + mpContext.snapshotsMissed;
        const float lossPercent = snapshotTotal
            ? 100.0f * (float)mpContext.snapshotsMissed / (float)snapshotTotal
            : 0.0f;
        snprintf(buf, sizeof(buf), "SNAPSHOT LOSS: %.1f%% (%llu missed)",
                 lossPercent, (unsigned long long)mpContext.snapshotsMissed);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

        snprintf(buf, sizeof(buf), "PACKETS TX/RX: %llu / %llu",
                 (unsigned long long)mpContext.packetsSent,
                 (unsigned long long)mpContext.packetsReceived);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;

        snprintf(buf, sizeof(buf), "SERVER: %s", mpContext.serverAddress.c_str());
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.75f, 0.85f, 1.0f}); y += lineH;

        snprintf(buf, sizeof(buf), "LOCAL POS: %.1f %.1f %.1f HP=%d",
                 player.pos.x, player.pos.y, player.pos.z,
                 mpContext.localServerHealth);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                   {0.35f, 1.0f, 0.45f, 1.0f});
        y += lineH;

        if (mpContext.hasLocalServerPosition)
        {
            snprintf(buf, sizeof(buf), "SERVER POS ERROR: %.2fm",
                     glm::length(player.pos - mpContext.localServerPosition));
            uiDrawText(buf, dbgX + 8.0f, y, 0.28f,
                       {1.0f, 0.35f, 0.25f, 1.0f});
        }
        y += lineH;

        for (const auto& kv : mpContext.remotePlayers)
        {
            const Player& rp = kv.second;
            auto nameIt = mpContext.playerRegistry.find(kv.first);
            const char* rname = (nameIt != mpContext.playerRegistry.end()) ? nameIt->second.name.c_str() : "?";
            snprintf(buf, sizeof(buf), "  %s id=%u pos=(%.1f,%.1f,%.1f)",
                     rname, kv.first, rp.pos.x, rp.pos.y, rp.pos.z);
            uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.6f, 0.85f, 1.0f, 1.0f});
            y += lineH;
        }
    }

    if (!gReplayExportRenderMode) {
        gReplayBrowser.draw();

        if (replayPlaybackActive) {
            if (const ReplaySceneFrame* rFrame = gReplayPlayer.currentSceneFrame()) {
                gReplayTimeline.draw(gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
            }
        }

        if (isReplayExportActive())
        {
            float ex = uiScreenW() * 0.5f - 200.0f;
            float ey = uiScreenH() * 0.7f;
            float ew = 400.0f;
            float eh = 80.0f;
            uiDrawRect({ex, ey, ew, eh}, {0.0f, 0.0f, 0.0f, 0.8f}, "export-bg");
            std::string status = getReplayExportStatusText();
            uiDrawText(status.c_str(), ex + 10.0f, ey + 8.0f, 0.32f, {0.3f, 1.0f, 0.5f, 1.0f});
            float p = getReplayExportProgress();
            uiDrawRect({ex + 10.0f, ey + eh - 16.0f, (ew - 20.0f) * p, 10.0f},
                       {0.3f, 1.0f, 0.3f, 1.0f}, "export-progress");
        }
        {
            static bool exportPopupShown = false;
            const ReplayExportJob& job = getReplayExportJob();
            if (job.state == ReplayExportJob::Done && !exportPopupShown) {
                exportPopupShown = true;
                std::string result = getReplayExportStatusText();
                DevOverlay::instance().showNotification(result, 8.0f);
            }
            if (job.state == ReplayExportJob::Failed && !exportPopupShown) {
                exportPopupShown = true;
                std::string result = getReplayExportStatusText();
                DevOverlay::instance().showNotification(result, 8.0f);
            }
            if (job.state == ReplayExportJob::Idle)
                exportPopupShown = false;
        }
    }
    MusicManager::instance().drawAllOverlay();
    if (gFramePacer.showFPS() && (!gReplayExportRenderMode || ReplayExportUI::showFps))
    {
        const GuiElement* fpsEl = hudLayout.get("fpsText");
        float fx = fpsEl ? fpsEl->x : 12.0f;
        float fy = fpsEl ? fpsEl->y : 12.0f;
        float fScale = fpsEl && fpsEl->fontSize > 0.0f ? fpsEl->fontSize : 0.36f;
        glm::vec4 fCol = fpsEl ? fpsEl->getTextColorVec() : glm::vec4{0.3f, 1.0f, 0.5f, 1.0f};
        uiDrawText(gFramePacer.fpsText(), uiScaleX(fx), uiScaleY(fy), fScale, fCol);
        if (gFramePacer.frameDebug())
        {
            const GuiElement* dbgEl = hudLayout.get("fpsDebugText");
            float dy = dbgEl ? dbgEl->y : (fy + 26.0f);
            float dScale = dbgEl && dbgEl->fontSize > 0.0f ? dbgEl->fontSize : 0.30f;
            glm::vec4 dCol = dbgEl ? dbgEl->getTextColorVec() : glm::vec4{0.5f, 0.8f, 1.0f, 1.0f};
            uiDrawText(gFramePacer.debugText(), uiScaleX(fx), uiScaleY(dy), dScale, dCol);
        }
    }
    if (PostFX::instance().debugEnabled && (!gReplayExportRenderMode || ReplayExportUI::showPostFxDebug))
    {
        const char* txt = PostFX::instance().debugText();
        if (txt && txt[0])
            uiDrawText(txt, uiScreenW() - 380.0f, 12.0f, 0.28f,
                       {1.0f, 0.8f, 0.2f, 1.0f});
    }
    if (ShadowConfig::instance().data().debugDrawShadowFrustum && (!gReplayExportRenderMode || ReplayExportUI::showShadowDebug))
    {
        const auto& sd = ShadowConfig::instance().data();
        char buf[512];
        glm::vec3 dir = LightingConfig::instance().lightDir();
        snprintf(buf, sizeof(buf),
            "SHADOWS\nenabled: %s\nmapSize: %d\ndistance: %.0f\nbias: %.4f\ndarkness: %.2f\nsoftness: %.1f\n\nsunDir:\n%.2f\n%.2f\n%.2f",
            sd.enabled ? "yes" : "no",
            sd.shadowMapSize,
            sd.shadowDistance,
            sd.shadowBias,
            sd.shadowDarkness,
            sd.shadowSoftness,
            dir.x, dir.y, dir.z);
        uiDrawText(buf, uiScreenW() - 280.0f, 120.0f, 0.26f,
                   {1.0f, 0.9f, 0.4f, 1.0f});
    }

    if (!gReplayExportRenderMode || ReplayExportUI::showPerfOverlay)
        Perf::renderOverlay();
    if (!gReplayExportRenderMode || ReplayExportUI::showPerfOverlay)
        uiRenderFrameDebugOverlay(engine.window(), "PLAYING", worldPassRan);

    if (isHealthbarDebugEnabled())
        drawHealthbarDebugOverlay(camera);

    // ── Room code HUD ───────────────────────────────────────────
    if (gRoomCodeShow && !gReplayExportRenderMode && !gReplayCinematicMode)
    {
        const std::string& code = mpContext.currentRoomCode;
        if (mpContext.active && !code.empty())
        {
            char buf[80];
            snprintf(buf, sizeof(buf), "Room code: %s", code.c_str());
            float cx = uiScreenW() * 0.5f;
            // 7 16 2026 todo make this hot reloadable, all things in gui should be hot
            // reloadable, but do later 
            float codeScale = 0.50f;
            float codeW = uiMeasureText(buf, codeScale);
            uiDrawText(buf, cx - codeW * 0.5f, 18.0f, codeScale, {1.0f, 1.0f, 1.0f, 1.0f});
            float instrScale = 0.32f;
            const char* line1 = "Press ` to open console";
            const char* line2 = "Type roomcodeshow 0 to hide the code Vro :heart:";
            float line1W = uiMeasureText(line1, instrScale);
            float line2W = uiMeasureText(line2, instrScale);
            float instrY = 46.0f;
            float lineGap = 16.0f;
            uiDrawText(line1, cx - line1W * 0.5f, instrY, instrScale, {0.75f, 0.85f, 1.0f, 1.0f});
            uiDrawText(line2, cx - line2W * 0.5f, instrY + lineGap, instrScale, {0.75f, 0.85f, 1.0f, 1.0f});
            printf("[ROOMCODE HUD] active=%d connected=%d roomCode=%s\n",
                   (int)mpContext.active, (int)mpContext.connected, code.c_str());
        }
    }
}
