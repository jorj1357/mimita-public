// 08 16 2026, 01 35
/* purpose
* Renders gameplay UI overlays such as pause state, player list, and network debug panels.
* Uses active engine, multiplayer, and replay state to draw lightweight HUD overlays.
* Applies compact VIP appearance to multiplayer player-list names.
* DOES NOT own network packet parsing, entitlement verification, or menu routing.
* DOES NOT mutate gameplay state except explicit overlay button actions.
* DOES NOT load full VIP presets or website badge image assets.
*/

#include "engine/engine-tick-ui.h"
#include "engine/engine.h"
#include "terminal/terminal-state.h"
#include <cstdio>
#include <cctype>
#include <algorithm>
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
#include "vip/vip-name-render.h"
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
#include "notifications/notifications.h"
#include "gui/hud/reward-popup.h"
#include "duel/duel-queue.h"
#include "network/community-match-client.h"
#include "input/mouse-lock.h"
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
#include "game/gamemode-manager.h"
#include "game/game-state.h"
#include "duel/duel-ui.h"
#include "network/multiplayer-context.h"
#include "gui/menus/online-menu.h"

#include "debug/debug-log.h"
#include "config/player-settings.h"
#include "npc/npc-combat.h"
#include "network/server.h"
#include "network/server-gamemode.h"

extern DuelManager gDuelManager;
extern GamemodeManager gGamemodeManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;
extern bool gReplayCinematicMode;extern bool gRoomCodeShow;

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
        (!gReplayExportRenderMode || ReplayExportUI::showDuelDebug()))
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
            gGamemodeManager.stop();
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
            gGamemodeManager.stop();
            npcSystem.destroyAll();
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            gameState = GAME_MENU;
        } else if (action == DuelMenuAction::SaveReplay) {
            if (!gDuelManager.finalKillReplayPath.empty()) {
                std::string jsonPath = gDuelManager.finalKillReplayPath;
                if (startReplayExport(jsonPath, gExportConfig.exportWidth, gExportConfig.exportHeight)) {
                    DevOverlay::instance().showNotification("Exporting replay in background...", 3.0f);
                } else {
                    DevOverlay::instance().showNotification("Failed to start export.", 5.0f);
                }
            } else {
                DevOverlay::instance().showNotification("Replay not ready yet. Wait for replay to load.", 5.0f);
            }
        }
    } else if (gGamemodeManager.isMatchEnd()) {
        // Community match end — render from replicated state
    } else {
        // Local/offline duel only. Network duels use DuelQueue HUD (renderDuelMatchHud).
        if (gDuelManager.enabled())
            gDuelManager.renderHud();
        // Community match HUD: rendered from server-authoritative replicated state
        if (gGamemodeManager.enabled())
            gGamemodeManager.renderHud();
    }

    if (mpContext.active && mpContext.showPlayerList &&
        (!gReplayExportRenderMode || ReplayExportUI::showPlayerList()))
    {
        GuiLayout& tabLayout =
            GuiLayoutManager::instance().getLayout("config/gui/tab-leaderboard.json");
        const GuiElement* panelElement = tabLayout.get("panel");
        const GuiElement* titleElement = tabLayout.get("title");
        const GuiElement* headerElement = tabLayout.get("header");
        const GuiElement* rowElement = tabLayout.get("row");
        const GuiElement* localRowElement = tabLayout.get("localRow");
        const GuiElement* npcRowElement = tabLayout.get("npcRow");
        const GuiElement* deadRowElement = tabLayout.get("deadRow");
        const GuiElement* rowStartEl = tabLayout.get("rowStartOffset");
        auto colOffset = [&](const char* id, float fallback) -> float {
            const GuiElement* el = tabLayout.get(id);
            return el ? uiScaleX(el->x) : fallback;
        };
        const float colId    = colOffset("idCol", 10.0f);
        const float colTeam  = colOffset("teamCol", 60.0f);
        const float colState = colOffset("stateCol", 120.0f);
        const float colName  = colOffset("nameCol", 190.0f);
        const float colPing  = colOffset("pingCol", 390.0f);

        static int gPlayerListFrame = 0;
        ++gPlayerListFrame;
        const float listPhase = (float)gPlayerListFrame / 8.0f;
        float listW = (panelElement && panelElement->w > 0.0f)
            ? uiScaleX(panelElement->w) : 420.0f;
        float listX = panelElement ? uiScaleX(panelElement->x)
                                   : uiScreenW() * 0.5f - listW * 0.5f;
        float listY = panelElement ? uiScaleY(panelElement->y) : uiScreenH() * 0.25f;
        const float rowStart = rowStartEl ? uiScaleY(rowStartEl->y) : 8.0f;
        float lineH = rowElement ? uiScaleY(rowElement->h) : 24.0f;
        float headerH = headerElement ? uiScaleY(headerElement->h) + 6.0f : 30.0f;

        size_t totalActors = mpContext.playerRegistry.size() + mpContext.remoteNpcs.size();
        float autoH = headerH + (totalActors + 1) * lineH + 10.0f;
        float listH = (panelElement && panelElement->h > 0.0f)
            ? uiScaleY(panelElement->h) : autoH;

        uiDrawRect({listX, listY, listW, listH},
                   panelElement ? panelElement->getBackgroundColorVec() : glm::vec4(0.0f, 0.0f, 0.0f, 0.85f),
                   "player-list-bg");
        uiDrawRectOutline({listX, listY, listW, listH},
                           panelElement ? panelElement->getOutlineColorVec() : glm::vec4(0.5f, 0.6f, 0.8f, 1.0f),
                           "player-list-border");

        float y = listY + rowStart;
        uiDrawText(titleElement && !titleElement->text.empty() ? titleElement->text.c_str() : "PLAYERS",
                   titleElement ? uiScaleX(titleElement->x) : listX + colId,
                   titleElement ? uiScaleY(titleElement->y) : y,
                   titleElement && titleElement->fontSize > 0.0f ? titleElement->fontSize : 0.36f,
                   titleElement ? titleElement->getTextColorVec() : glm::vec4(0.8f, 0.9f, 1.0f, 1.0f));
        y += headerH;
        uiDrawText(headerElement && !headerElement->text.empty()
                       ? headerElement->text.c_str()
                       : "ID   TEAM   STATE   NAME                       PING",
                   headerElement ? uiScaleX(headerElement->x) : listX + colId,
                   headerElement ? uiScaleY(headerElement->y) : y,
                   headerElement && headerElement->fontSize > 0.0f ? headerElement->fontSize : 0.28f,
                   headerElement ? headerElement->getTextColorVec() : glm::vec4(0.65f, 0.75f, 0.9f, 1.0f));
        y += lineH;

        // Team tag + state from the authoritative replicated roster.
        const MimitaNet::CommunityMatchClient& tabMatch =
            MimitaNet::CommunityMatchClient::instance();
        const bool hasTeams = tabMatch.active() && !tabMatch.teamName(0).empty();
        const std::string tagFormat =
            tabLayout.get("teamTagFormat") && !tabLayout.get("teamTagFormat")->text.empty()
                ? tabLayout.get("teamTagFormat")->text : "{short}";
        auto localize = [](std::string s) {
            for (auto& c : s) c = (char)std::toupper((unsigned char)c);
            return s;
        };
        auto teamTag = [&](uint32_t actorId) -> std::string {
            if (!hasTeams) return "";
            const uint8_t t = tabMatch.teamForActor(actorId);
            if (t == 0xFF) return "-";
            std::string name = tabMatch.teamName((int)t);
            std::string shortTag = name.substr(0, std::min<size_t>(4, name.size()));
            shortTag = localize(shortTag);
            std::string out = tagFormat;
            size_t at = out.find("{short}");
            if (at != std::string::npos) out.replace(at, 7, shortTag);
            return out;
        };
        auto stateText = [&](uint32_t actorId) -> std::string {
            if (!tabMatch.active()) return "";
            const uint8_t st = tabMatch.actorState(actorId);
            auto elementText = [&](const char* id, const char* fallback) {
                const GuiElement* el = tabLayout.get(id);
                return (el && !el->text.empty()) ? el->text : std::string(fallback);
            };
            switch (st) {
                case (uint8_t)MimitaNet::ActorState::Alive:      return elementText("stateAliveText", "ALIVE");
                case (uint8_t)MimitaNet::ActorState::Dead:       return elementText("stateDeadText", "DEAD");
                case (uint8_t)MimitaNet::ActorState::Respawning: return elementText("stateDeadText", "DEAD");
                case (uint8_t)MimitaNet::ActorState::Spectating: return elementText("stateSpectatorText", "SPECT");
                default: return "";
            }
        };
        auto isActorDead = [&](uint32_t actorId) -> bool {
            if (!tabMatch.active()) return false;
            const uint8_t st = tabMatch.actorState(actorId);
            return st == (uint8_t)MimitaNet::ActorState::Dead ||
                   st == (uint8_t)MimitaNet::ActorState::Respawning ||
                   st == (uint8_t)MimitaNet::ActorState::Spectating;
        };

        // Draw one row at the shared column offsets.
        auto drawColumns = [&](float rowY, const std::string& idText,
                               const std::string& team, const std::string& state,
                               const std::string& name, const std::string& ping,
                               float scale, const glm::vec4& color) {
            uiDrawText(idText.c_str(), listX + colId, rowY, scale, color);
            uiDrawText(team.c_str(), listX + colTeam, rowY, scale, color);
            uiDrawText(state.c_str(), listX + colState, rowY, scale, color);
            uiDrawText(name.c_str(), listX + colName, rowY, scale, color);
            uiDrawText(ping.c_str(), listX + colPing, rowY, scale, color);
        };

        if (mpContext.localPlayerId)
        {
            const float localScale = localRowElement && localRowElement->fontSize > 0.0f
                ? localRowElement->fontSize : 0.32f;
            const bool dead = isActorDead(mpContext.localPlayerId);
            const glm::vec4 localColor = (dead && deadRowElement)
                ? deadRowElement->getTextColorVec()
                : (localRowElement ? localRowElement->getTextColorVec()
                                   : glm::vec4(0.3f, 1.0f, 0.4f, 1.0f));
            char idBuf[32];
            snprintf(idBuf, sizeof(idBuf), "%u", mpContext.localPlayerId);
            char pingBuf[48];
            snprintf(pingBuf, sizeof(pingBuf), "%dms you", mpContext.localPingMs);
            const std::string localName = player.username.empty() ? "you" : player.username;
            drawColumns(y, idBuf, teamTag(mpContext.localPlayerId), stateText(mpContext.localPlayerId),
                        localName, pingBuf, localScale, localColor);
            y += lineH;
        }

        for (const auto& kv : mpContext.playerRegistry)
        {
            if (kv.first == mpContext.localPlayerId)
                continue;
            const float rowScale = rowElement && rowElement->fontSize > 0.0f
                ? rowElement->fontSize : 0.32f;
            const bool dead = isActorDead(kv.first);
            const glm::vec4 rowColor = (dead && deadRowElement)
                ? deadRowElement->getTextColorVec()
                : (rowElement ? rowElement->getTextColorVec() : glm::vec4(0.9f, 0.95f, 1.0f, 1.0f));
            char idBuf[32];
            snprintf(idBuf, sizeof(idBuf), "%u", kv.first);
            char pingBuf[32];
            snprintf(pingBuf, sizeof(pingBuf), "%dms", kv.second.pingMs);
            drawColumns(y, idBuf, teamTag(kv.first), stateText(kv.first),
                        kv.second.name, pingBuf, rowScale, rowColor);
            y += lineH;
        }

        for (const auto& kv : mpContext.remoteNpcs)
        {
            const Player& npc = kv.second;
            const float npcScale = npcRowElement && npcRowElement->fontSize > 0.0f
                ? npcRowElement->fontSize : 0.32f;
            const bool dead = isActorDead(kv.first);
            const glm::vec4 npcColor = (dead && deadRowElement)
                ? deadRowElement->getTextColorVec()
                : (npcRowElement ? npcRowElement->getTextColorVec() : glm::vec4(1.0f, 0.7f, 0.3f, 1.0f));
            char idBuf[32];
            snprintf(idBuf, sizeof(idBuf), "%u", kv.first);
            const std::string npcName = npc.username.empty()
                ? ("NPC-" + std::to_string(kv.first)) : npc.username;
            drawColumns(y, idBuf, teamTag(kv.first), stateText(kv.first),
                        npcName, "NPC", npcScale, npcColor);
            y += lineH;
        }
    }

    if (mpContext.active && mpContext.showDebugOverlay)
    {
        float dbgX = uiScreenW() - 360.0f;
        float dbgY = 20.0f;
        float lineH = 18.0f;
        float dbgW = 340.0f;
        // 17 fixed lines (incl. conditional server-pos-error) + remote rows
        float dbgH = (17.0f + (float)mpContext.remotePlayers.size()) * lineH + 10.0f;

        uiDrawRect({dbgX, dbgY, dbgW, dbgH}, {0.0f, 0.0f, 0.0f, 0.8f}, "net-debug-bg");

        float y = dbgY + 6.0f;
        char buf[256];
        const uint64_t nowDbg = MimitaNet::nowMs();

        // STATUS reflects real packet freshness (never "Connected via ICE" lies).
        const glm::vec4 statusColor =
            mpContext.connected && mpContext.connectionState != MimitaNet::ConnectionState::WeakConnection
                ? glm::vec4(0.3f, 1.0f, 0.4f, 1.0f)
                : glm::vec4(1.0f, 0.30f, 0.25f, 1.0f);
        snprintf(buf, sizeof(buf), "STATUS: %s",
                 MimitaNet::mpConnectionHealthText(mpContext).c_str());
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, statusColor);
        y += lineH;

        snprintf(buf, sizeof(buf), "CONN STATE: %s",
                 MimitaNet::connectionStateName(mpContext.connectionState));
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

        {
            std::string serverLabel = mpContext.serverAddress;
            if (!mpContext.roomCode.empty())
                serverLabel += " room=" + mpContext.roomCode;
            snprintf(buf, sizeof(buf), "SERVER: %s", serverLabel.c_str());
            uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.75f, 0.85f, 1.0f}); y += lineH;
        }

        snprintf(buf, sizeof(buf), "LOCAL PLAYER ID: %u", mpContext.localPlayerId);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.3f, 1.0f, 0.4f, 1.0f}); y += lineH;

        snprintf(buf, sizeof(buf), "PING: %dms", mpContext.localPingMs);
        uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.75f, 0.85f, 1.0f, 1.0f});
        y += lineH;

        {
            const bool transportUp = mpContext.transport && mpContext.transport->connected();
            const char* ice = MimitaNet::mpIceConnectActive() ? "busy" : (transportUp ? "up" : "down");
            snprintf(buf, sizeof(buf), "TRANSPORT: %s ICE=%s",
                     mpContext.transport ? "active" : "raw-udp", ice);
            uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.75f, 0.85f, 1.0f, 1.0f}); y += lineH;
        }

        {
            const uint64_t rxAge = mpContext.lastHeardServerMs && nowDbg >= mpContext.lastHeardServerMs
                ? nowDbg - mpContext.lastHeardServerMs : 0;
            const uint64_t txAge = mpContext.lastPacketSentMs && nowDbg >= mpContext.lastPacketSentMs
                ? nowDbg - mpContext.lastPacketSentMs : 0;
            snprintf(buf, sizeof(buf), "LAST PKT RX AGE: %llums  TX AGE: %llums",
                     (unsigned long long)rxAge, (unsigned long long)txAge);
            uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.7f, 0.8f, 1.0f, 1.0f}); y += lineH;
        }

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

        snprintf(buf, sizeof(buf), "TICK CLIENT %u / SERVER %llu",
                 mpContext.tick, (unsigned long long)mpContext.lastSnapshotTick);
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

        if (mpContext.connectionState == MimitaNet::ConnectionState::Reconnecting)
        {
            const double elapsed = mpContext.disconnectStartedMs
                ? (double)(nowDbg - mpContext.disconnectStartedMs) / 1000.0 : 0.0;
            const double bailIn = mpContext.reconnectGraceDeadlineMs
                ? (double)(mpContext.reconnectGraceDeadlineMs - nowDbg) / 1000.0 : 0.0;
            snprintf(buf, sizeof(buf), "RECONNECT #%d  %.2fs elapsed  /  bail in %.0fs",
                     mpContext.reconnectAttempts, elapsed,
                     bailIn < 0.0 ? 0.0 : bailIn);
            uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {1.0f, 0.5f, 0.4f, 1.0f});
            y += lineH;
        }
        else
        {
            snprintf(buf, sizeof(buf), "RECONNECT: n/a");
            uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.55f, 0.6f, 0.7f, 1.0f});
            y += lineH;
        }

        snprintf(buf, sizeof(buf), "ENTITIES: %zu (PLAYERS %zu / NPCS %zu)",
                 mpContext.remotePlayers.size() + mpContext.remoteNpcs.size() +
                     (mpContext.localPlayerId ? 1u : 0u),
                 mpContext.remotePlayers.size() + (mpContext.localPlayerId ? 1u : 0u),
                 mpContext.remoteNpcs.size());
        uiDrawText(buf, dbgX + 8.0f, y, 0.28f, {0.9f, 0.95f, 1.0f, 1.0f}); y += lineH;

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
            const auto interpIt = mpContext.remotePlayerInterpolation.find(kv.first);
            if (interpIt != mpContext.remotePlayerInterpolation.end())
            {
                const MimitaNet::EntityInterpolationState& s = interpIt->second;
                snprintf(buf, sizeof(buf), "  %s id=%u buf=%zu delay=%.0fms jit=%.0fms d=%.2fm",
                         rname, kv.first, s.buffer.size(),
                         s.adaptiveDelaySeconds * 1000.0, s.estimatedArrivalJitterMs,
                         s.hasTarget ? glm::length(rp.pos - s.target.position) : 0.0f);
            }
            else
            {
                snprintf(buf, sizeof(buf), "  %s id=%u pos=(%.1f,%.1f,%.1f)",
                         rname, kv.first, rp.pos.x, rp.pos.y, rp.pos.z);
            }
            uiDrawText(buf, dbgX + 8.0f, y, 0.26f, {0.6f, 0.85f, 1.0f, 1.0f});
            y += lineH;
        }
    }

    if (!gReplayExportRenderMode || ReplayExportUI::showReplayBrowser())
        gReplayBrowser.draw();

    if (replayPlaybackActive &&
        (!gReplayExportRenderMode || ReplayExportUI::showReplayTimeline())) {
        if (const ReplaySceneFrame* rFrame = gReplayPlayer.currentSceneFrame()) {
            gReplayTimeline.draw(gReplayPlayer.currentTick(), gReplayPlayer.totalTicks());
        }
    }

    if (isReplayExportActive() &&
        (!gReplayExportRenderMode || ReplayExportUI::showExportProgress()))
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

    if (!gReplayExportRenderMode) {
        {
            static bool exportPopupShown = false;
            const ReplayExportJob& job = getReplayExportJob();
            if (job.state == ReplayExportJob::Done && !job.clipExport && !exportPopupShown) {
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
    NotificationSystem::instance().render(true);
    RewardPopupSystem::instance().render();
    // Mouse lock indicator removed — belongs in ESC/Help menu.
    if (gFramePacer.showFPS() && (!gReplayExportRenderMode || ReplayExportUI::showFps()))
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
    if (PostFX::instance().debugEnabled && (!gReplayExportRenderMode || ReplayExportUI::showPostFxDebug()))
    {
        const char* txt = PostFX::instance().debugText();
        if (txt && txt[0])
            uiDrawText(txt, uiScreenW() - 380.0f, 12.0f, 0.28f,
                       {1.0f, 0.8f, 0.2f, 1.0f});
    }
    if (ShadowConfig::instance().data().debugDrawShadowFrustum && (!gReplayExportRenderMode || ReplayExportUI::showShadowDebug()))
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

    if (!gReplayExportRenderMode || ReplayExportUI::showPerfOverlay())
        Perf::renderOverlay();
    if (!gReplayExportRenderMode || ReplayExportUI::showPerfOverlay())
        uiRenderFrameDebugOverlay(engine.window(), "PLAYING", worldPassRan);

    if (isHealthbarDebugEnabled())
        drawHealthbarDebugOverlay(camera);

    // ── Room code HUD ───────────────────────────────────────────
    if (gRoomCodeShow && !gReplayCinematicMode)
    {
        const std::string& code = mpContext.currentRoomCode;
        if (!code.empty())
        {
            bool isLocal = code.find("LOCAL-") == 0;
            char buf[96];
            if (isLocal)
                snprintf(buf, sizeof(buf), "Room: %s  (local)", code.c_str());
            else
                snprintf(buf, sizeof(buf), "Room: %s", code.c_str());
            float cx = uiScreenW() * 0.5f;
            float codeScale = 0.50f;
            float codeW = uiMeasureText(buf, codeScale);
            glm::vec4 codeColor = isLocal
                ? glm::vec4(0.6f, 1.0f, 0.6f, 1.0f)
                : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            uiDrawText(buf, cx - codeW * 0.5f, 18.0f, codeScale, codeColor);
            // Room code instruction lines removed — belongs in ESC/Help menu.
        }
    }

    // ── Duels queue + PvP match HUD ────────────────────────────────
    if (!gReplayExportRenderMode && gameState == GAME_PLAYING)
    {
        // ── FFA/TDM intermission + countdown HUD ──────────────────
        {
            const MimitaNet::CommunityMatchClient& match =
                MimitaNet::CommunityMatchClient::instance();

            if (match.active())
            {
                GuiLayout& matchLayout =
                    GuiLayoutManager::instance().getGamemodeLayout(match.mode());
                auto drawCentered = [&](const char* id, const std::string& text) {
                    const GuiElement* el = matchLayout.get(id);
                    if (!el || !el->visible) return;
                    const float scale = el->fontSize > 0.0f ? el->fontSize : 0.4f;
                    const float w = uiMeasureText(text.c_str(), scale);
                    uiDrawText(text.c_str(), uiScreenW() * 0.5f - w * 0.5f,
                               uiScaleY(el->y), scale, el->getTextColorVec());
                };
                auto textTemplate = [&](const char* id, const std::vector<std::pair<std::string, std::string>>& values) {
                    const GuiElement* el = matchLayout.get(id);
                    std::string text = el ? el->text : "";
                    for (const auto& value : values) {
                        size_t at = 0;
                        while ((at = text.find(value.first, at)) != std::string::npos) {
                            text.replace(at, value.first.size(), value.second);
                            at += value.second.size();
                        }
                    }
                    return text;
                };
                std::string modeName = match.mode();
                std::replace(modeName.begin(), modeName.end(), '_', ' ');
                for (char& c : modeName)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                drawCentered("modeTitle", textTemplate("modeTitle", {
                    {"{mode_name}", modeName},
                    {"{disaster_name}", match.disasterName()},
                    {"{disaster_description}", match.disasterDescription()}}));

                // ── Disaster banner (generic; any mode with a declared
                // disaster can define a "disasterText" layout element). It is
                // driven entirely by replicated state, never a per-disaster
                // renderer.
                if (match.disasterConfigured() && matchLayout.get("disasterText")) {
                    drawCentered("disasterText", textTemplate("disasterText", {
                        {"{disaster_name}", match.disasterName()},
                        {"{disaster_description}", match.disasterDescription()}}));
                }

                // Score presentation is data-driven: a mode displays this
                // element only when its JSON layout defines it.
                // Team names come from the active gamemode JSON in fixed order;
                // fall back to legacy RED/BLUE when the mode has no ordered teams.
                const bool namedTeams = !match.teamName(0).empty();
                const std::string leftName = namedTeams ? match.teamName(0) : "RED";
                const std::string rightName = namedTeams ? match.teamName(1) : "BLUE";
                const int leftScore = namedTeams ? match.roundWins(0) : match.redScore();
                const int rightScore = namedTeams ? match.roundWins(1) : match.blueScore();
                if (matchLayout.get("scoreText")) {
                    drawCentered("scoreText", textTemplate("scoreText", {
                        {"{red_name}", leftName}, {"{red_score}", std::to_string(leftScore)},
                        {"{blue_name}", rightName}, {"{blue_score}", std::to_string(rightScore)},
                        {"{goal}", std::to_string(match.goal())}}));
                }

                // Intermission text is supplied by the active JSON layout.
                if (match.phase() == MimitaNet::DUEL_PHASE_INTERMISSION) {
                    drawCentered("intermissionText", textTemplate("intermissionText", {
                        {"{seconds}", std::to_string((int)std::ceil(std::max(0.0f, match.phaseTimer())))}}));
                }

                if (match.waveBannerVisible()) {
                    drawCentered("waveText", textTemplate("waveText", {
                        {"{number}", std::to_string(match.waveNumber())},
                        {"{wave}", std::to_string(match.waveNumber())}}));
                }

                if (match.mode() == "npc_waves" && match.waveLivesRemaining() > 0 &&
                    match.phase() != MimitaNet::DUEL_PHASE_RESULTS) {
                    drawCentered("livesText", textTemplate("livesText", {
                        {"{lives}", std::to_string(match.waveLivesRemaining())}}));
                }

                if (match.mode() == "npc_waves" && match.matchOver() &&
                    match.waveLivesRemaining() <= 0) {
                    drawCentered("gameOverText", textTemplate("gameOverText", {
                        {"{highest_wave}", std::to_string(match.waveHighest())}}));
                }

                if (!match.waveBannerVisible() &&
                    (match.phase() == MimitaNet::DUEL_PHASE_COUNTDOWN ||
                     match.goVisible())) {
                    const GuiElement* countdownElement = matchLayout.get("countdownText");
                    const uint32_t ticksLeft = match.matchStartTick() > match.serverTick()
                        ? match.matchStartTick() - match.serverTick() : 0;
                    // Clamp to 1: the extrapolated client tick may reach the
                    // start before the GO packet arrives, and the countdown
                    // should never flash "0" — it is replaced by GO.
                    const int number = std::max(1, (int)std::ceil((float)ticksLeft / 60.0f));
                    // goVisible() keeps GO! up for the server-sent window even
                    // if the ACTIVE packet arrived first.
                    const std::string countdownText = match.goVisible()
                        ? (countdownElement && !countdownElement->goText.empty() ? countdownElement->goText : "GO!!!")
                        : textTemplate("countdownText", {{"{countdown}", std::to_string(number)}});
                    drawCentered("countdownText", countdownText);
                }

                if (match.phase() == MimitaNet::DUEL_PHASE_ACTIVE && match.timeLimitSeconds() > 0) {
                    const uint32_t elapsed = match.serverTick() > match.matchStartTick()
                        ? match.serverTick() - match.matchStartTick() : 0;
                    const int left = std::max(0, match.timeLimitSeconds() - (int)(elapsed / 60));
                    drawCentered("matchTime", textTemplate("matchTime", {
                        {"{minutes}", std::to_string(left / 60)},
                        {"{seconds}", std::string(left % 60 < 10 ? "0" : "") + std::to_string(left % 60)}}));
                }

                // ── Round-based modes: active round timer + result strings ──
                if (namedTeams) {
                    if (match.phase() == MimitaNet::DUEL_PHASE_ACTIVE &&
                        match.roundSeconds() > 0.0f) {
                        const int left = std::max(0, (int)std::ceil(match.roundTimerLeft()));
                        drawCentered("matchTime", textTemplate("matchTime", {
                            {"{minutes}", std::to_string(left / 60)},
                            {"{seconds}", std::string(left % 60 < 10 ? "0" : "") + std::to_string(left % 60)}}));
                    }
                    if (matchLayout.get("roundText")) {
                        drawCentered("roundText", textTemplate("roundText", {
                            {"{round}", std::to_string(match.roundNumber())}}));
                    }
                    if (matchLayout.get("roundOverText") &&
                        match.phase() == MimitaNet::DUEL_PHASE_RESULTS) {
                        const std::string winner = match.winnerTeam() >= 0
                            ? match.teamName(match.winnerTeam()) : "";
                        const std::string roundText = match.matchOver()
                            ? (winner + " win the match")
                            : (winner + " win the round");
                        drawCentered("roundOverText", roundText);
                        drawCentered("scoreText", textTemplate("scoreText", {
                            {"{red_name}", leftName}, {"{red_score}", std::to_string(match.roundWins(0))},
                            {"{blue_name}", rightName}, {"{blue_score}", std::to_string(match.roundWins(1))},
                            {"{goal}", std::to_string(match.goal())}}));
                    }

                    // ── Objective prompt (bomb) ─────────────────────
                    // Only shown when a mode objective exists and is in play.
                    const auto& obj = match.objective();
                    if (obj.active &&
                        (match.phase() == MimitaNet::DUEL_PHASE_ACTIVE ||
                         match.phase() == MimitaNet::DUEL_PHASE_GO)) {
                        const glm::vec3 camPos = camera.pos;
                        const float dist = glm::length(camPos - obj.position);
                        // 0=Inactive 1=Carried 2=Dropped 3=Planted 4=Defused 5=Exploded
                        if (obj.progressKind == 1 && matchLayout.get("objectivePrompt")) {
                            drawCentered("objectivePrompt", std::string("Planting Bomb..."));
                        } else if (obj.progressKind == 2 && matchLayout.get("objectivePrompt")) {
                            drawCentered("objectivePrompt", std::string("Defusing Bomb..."));
                        } else if (obj.state == 3 && matchLayout.get("objectiveStatus")) {
                            // Planted: show time until explosion.
                            const int left = std::max(0, (int)std::ceil(obj.timerLeft));
                            drawCentered("objectiveStatus", std::string("BOMB PLANTED ") +
                                std::to_string(left) + "s");
                        } else if (obj.state == 2 && matchLayout.get("objectivePrompt") && dist <= 3.0f) {
                            drawCentered("objectivePrompt", std::string("Pick up Bomb"));
                        } else if (obj.state == 2 && matchLayout.get("objectiveStatus")) {
                            drawCentered("objectiveStatus", std::string("BOMB DROPPED"));
                        }
                    }
                }
            }
        }

        renderDuelQueueHud(engine.window(), dt);
        renderDuelMatchHud(engine.window(), dt);
        renderDuelTracer(camera);
    }
}
