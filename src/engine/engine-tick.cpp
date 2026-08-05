#include "engine/engine-tick.h"
#include "engine/engine-tick-setup.h"
#include "engine/engine-tick-audio.h"
#include "engine/engine-tick-state.h"
#include "engine/engine-tick-replay.h"
#include "engine/engine-tick-net.h"
#include "engine/engine-tick-camera.h"
#include "engine/engine-tick-combat.h"
#include "engine/engine-tick-render.h"
#include "engine/engine-tick-ui.h"

#include <chrono>
#include <cstdio>
#include <shellapi.h>
#include <windows.h>
#include "engine/engine.h"
#include "gui/gui-main.h"
#include "gui/ui-system.h"
#include "gui/hud/chat-window.h"
#include "gui/menus/online-menu.h"
#include "notifications/notifications.h"
#include "input/mouse-lock.h"
#include "devtools/dev-overlay.h"
#include "devtools/terminal.h"
#include "debug/debug-diag.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "perf/perf.h"
#include "perf/perf-spike.h"
#include "perf/perf-frame.h"
#include "video/frame-pacer.h"
#include "replay/replay-editor.h"
#include "replay/replay-export.h"
#include "replay/replay-export-ui.h"
#include "game/duel.h"
#include "game/bomb-tag.h"
#include "game/game-cli.h"
#include "game/game-state.h"
#include "replay/replay.h"
#include "network/multiplayer-context.h"
#include "network/server.h"
#include "gui/gui-main.h"
#include "terminal/terminal-state.h"

extern DuelManager gDuelManager;
extern BombTagManager gBombTagManager;
extern FramePacer gFramePacer;
extern bool gReplayExportRenderMode;

// Heartbeat: logs at most once per 0.5s, used to identify where the frame loop stops.
#define HEARTBEAT(MSG) \
    Debug::logThrottled(Debug::Category::General, "heartbeat", 0.5f, "[HB] " MSG "\n")

// Spike detection: warns if a stage takes too long.
#define CHECK_SPIKE(NAME, MS, THRESHOLD) \
    do { \
        if ((MS) > (THRESHOLD)) { \
            const char* __colors[] = {"", "\033[33m", "\033[38;5;214m", "\033[31m"}; \
            int __idx = (MS) > 20.0f ? 3 : ((MS) > 10.0f ? 2 : ((MS) > 5.0f ? 1 : 0)); \
            Debug::warn(Debug::Category::General, \
                "\033[33m[SPIKE]\033[0m %s %.1fms (threshold=%dms)\n", \
                (NAME), (MS), (THRESHOLD)); \
        } \
    } while(0)

void engineTick(Engine& engine)
{
    MIMITA_PERF_SCOPE("EngineTick");
    // Step UI tick clock (60 Hz fixed step, independent of render FPS)
    gChatUiTickClock.tick();
    NotificationSystem::instance().advanceTicks();
    if (GAME_STATE == GAME_PLAYING)
        NotificationSystem::instance().updateTips();
    auto tFrameStart = std::chrono::steady_clock::now();
    HEARTBEAT("FRAME START");

    float dt;
    bool worldPassRan;
    { MIMITA_PERF_SCOPE("Setup"); engineTickSetup(engine, dt, worldPassRan); }
    { MIMITA_PERF_SCOPE("Audio"); engineTickAudio(dt); }
    // Always tick listen server if hosting (receives packets, sends snapshots)
    {
        MimitaNet::ListenServerState* ls = getListenServerState();
        if (ls && ls->active)
            MimitaNet::tickListenServer(*ls, dt);
    }
    { MIMITA_PERF_SCOPE("State"); engineTickState(engine, dt); }
    HEARTBEAT("after state");

    const bool replayExportForceRender =
        (getReplayExportJob().state == ReplayExportJob::Capturing ||
         getReplayExportJob().state == ReplayExportJob::Encoding) && WORLD_LOADED;

    if (GAME_STATE == GAME_PLAYING || GAME_STATE == GAME_LOADING_MAP || replayExportForceRender)
    {
        { MIMITA_PERF_SCOPE("Replay"); engineTickReplay(engine, dt); } HEARTBEAT("after replay");
        { MIMITA_PERF_SCOPE("Networking"); engineTickNet(engine, dt); } HEARTBEAT("after net");
        if (GAME_STATE == GAME_PLAYING) {
            { MIMITA_PERF_SCOPE("Camera"); engineTickCamera(engine, dt); } HEARTBEAT("after camera");
            { MIMITA_PERF_SCOPE("Combat"); engineTickCombat(engine, dt); } HEARTBEAT("after combat");
            { MIMITA_PERF_SCOPE("Rendering"); engineTickRender(engine, dt, worldPassRan); } HEARTBEAT("after render");
            { MIMITA_PERF_SCOPE("UI"); engineTickUI(engine, dt, worldPassRan); } HEARTBEAT("after ui");
        }
        { MIMITA_PERF_SCOPE("DevOverlay");
        if (!gReplayExportRenderMode || ReplayExportUI::showDevOverlay)
            DevOverlay::instance().render();
        }
    }

    { MIMITA_PERF_SCOPE("Menus");
    uiUpdateMedia(dt);

    if (GAME_STATE == GAME_MENU && !isReplayExportActive())
    {
        guiMain(engine.window(), GAME_STATE);
    }
    }

    static bool escapePrev = false;
    bool escapeDown = glfwGetKey(engine.window(), GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escapeDown && !escapePrev)
    {
        // If keyframe prompt is active, let the camera handler cancel it
        if (gReplayEditor.keyframePromptStage > 0) {
            // handled in engine-tick-camera.cpp
        } else if (Terminal::instance().isOpen()) {
            Terminal::instance().toggle();
            bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
            glfwSetInputMode(engine.window(), GLFW_CURSOR,
                GAME_STATE == GAME_PLAYING && !duelMatchOver && MouseLock::locked()
                    ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        } else if (REPLAY_PLAYER.isPlaying()) {
            printf("[MAINMENU] cleaning replay\n");
            REPLAY_PLAYER.stopPlayback();
            REPLAY_ACTOR_MODELS.clear();
            REPLAY_WEAPON_MODELS.clear();
            REPLAY_CHAT_STATES.clear();
            printf("[MAINMENU] switching to replay menu\n");
            gGuiMenuState = GUI_MENU_REPLAY;
            GAME_STATE = GAME_MENU;
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else if (GAME_STATE == GAME_MENU && gGuiMenuState == GUI_MENU_REPLAY) {
            printf("[MAINMENU] switching to main menu\n");
            gGuiMenuState = GUI_MENU_MAIN;
        } else {
            if (gDuelManager.phase() != DuelPhase::Off) {
                Debug::log(Debug::Category::Duel, "[DUEL] escape pressed during duel (phase=%d) — cleaning up", (int)gDuelManager.phase());
                REPLAY_PLAYER.stopPlayback();
                if (REPLAY_RECORDER.isRecording())
                    REPLAY_RECORDER.stopRecording();
                gDuelManager.stopDuel();
                THE_NPC_SYSTEM.destroyAll();
            }
            // mpShutdown cancels any in-flight async ICE connect first, then
            // tears down an active session. Safe to call even when not active.
            MimitaNet::mpShutdown(MP_CONTEXT);
            onlineMenuSetServerCode("");
            onlineMenuSetServerRunning(false);
            Debug::log(Debug::Category::Duel, "[DUEL] escape: transitioning to main menu");
            GAME_STATE = GAME_MENU;
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    escapePrev = escapeDown;

    // L key toggles gameplay mouse lock so the cursor can click notifications.
    static bool mouseLockKeyPrev = false;
    bool mouseLockKeyDown = glfwGetKey(engine.window(), GLFW_KEY_L) == GLFW_PRESS;
    if (GAME_STATE == GAME_PLAYING && !Terminal::instance().isOpen() && !isChatOpen() &&
        !REPLAY_PLAYER.isPlaying() && mouseLockKeyDown && !mouseLockKeyPrev) {
        MouseLock::toggle(engine.window());
        Debug::log(Debug::Category::Gui, "[MOUSELOCK] toggled via L: %s\n",
                   MouseLock::locked() ? "locked" : "unlocked");
    }
    mouseLockKeyPrev = mouseLockKeyDown;

    static bool f10Prev = false;
    bool f10Down = glfwGetKey(engine.window(), GLFW_KEY_F10) == GLFW_PRESS;
    if (!Terminal::instance().isOpen() && f10Down && !f10Prev) {
        ShellExecuteA(NULL, "open", "replays", NULL, NULL, SW_SHOWNORMAL);
        Debug::log(Debug::Category::General, "[MAIN] opened replays folder");
    }
    f10Prev = f10Down;

    static bool f12Prev = false;
    bool f12Down = glfwGetKey(engine.window(), GLFW_KEY_F12) == GLFW_PRESS;
    if (f12Down && !f12Prev && !Terminal::instance().isOpen()) {
        Debug::log(Debug::Category::General, "[MAINMENU] F12 pressed — forcing main menu");
        forceMainMenu();
        DevOverlay::instance().showNotification("Returned to Main Menu (F12)", 3.0f);
        Terminal::instance().addLog("[MAINMENU] triggered via F12 key");
    }
    f12Prev = f12Down;

    if (GAME_STATE == GAME_MENU && gDuelManager.phase() != DuelPhase::Off) {
        Debug::log(Debug::Category::Duel, "[DUEL FAILSAFE] gameState=MENU but duel phase=%d — forcing cleanup", (int)gDuelManager.phase());
        forceMainMenu();
    }

    if (isReplayExportActive()) {
        updateReplayExport();
    } else if (isReplayBatchExportActive()) {
        updateReplayBatchExport();
    }

    HEARTBEAT("before terminal");
    { MIMITA_PERF_SCOPE("Terminal"); Terminal::instance().render(); }
    { Perf::ScopedTimer _t("Diag"); diagRenderStage(8); }

    HEARTBEAT("end frame");
    { MIMITA_PERF_SCOPE("Swap"); engine.endFrame(); }
    { Perf::ScopedTimer _t("Diag"); diagRenderStage(9); }
    { Perf::ScopedTimer _t("Diag"); diagRenderFrameEnd(); }
    Perf::endFrame();

    { MIMITA_PERF_SCOPE("Sleep"); gFramePacer.endFrame(); }

    // ── Structured logger config hot-reload + tick ────
    StructuredLogger::instance().pollConfig();
    StructuredLogger::instance().tick();

    // ── Frame timing breakdown ───────────────────────
    {
        auto tNow = std::chrono::steady_clock::now();
        float frameMs = std::chrono::duration<float, std::milli>(tNow - tFrameStart).count();
        CHECK_SPIKE("FRAME TOTAL", frameMs, 20);

        static auto sLastReport = std::chrono::steady_clock::now();
        float sinceReport = std::chrono::duration<float>(tNow - sLastReport).count();
        if (sinceReport >= 1.0f) {
            sLastReport = tNow;

            // Enriched frame timing with entity counts
            int lastIdx = gFrameHistoryCount > 0
                ? (gFrameHistoryIndex - 1 + FRAME_HISTORY_CAPACITY) % FRAME_HISTORY_CAPACITY
                : -1;
            int npcs = (lastIdx >= 0) ? gFrameHistory[lastIdx].npcCount : 0;
            int efx  = (lastIdx >= 0) ? gFrameHistory[lastIdx].effectCount : 0;
            int aud  = (lastIdx >= 0) ? gFrameHistory[lastIdx].audioCount : 0;

            Debug::log(Debug::Category::General,
                "[FRAME TIMING] %.1fms  fps=%.0f  dt=%.3f  npcs=%d  effects=%d  audio=%d\n",
                frameMs, 1000.0f / (frameMs > 0.1f ? frameMs : 1.0f), dt,
                npcs, efx, aud);
        }
    }
}
