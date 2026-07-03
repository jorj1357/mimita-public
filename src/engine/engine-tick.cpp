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
#include "devtools/dev-overlay.h"
#include "devtools/terminal.h"
#include "debug/debug-diag.h"
#include "debug/debug-log.h"
#include "perf/perf.h"
#include "video/frame-pacer.h"
#include "replay/replay-export.h"
#include "replay/replay-export-ui.h"
#include "game/duel.h"
#include "game/bomb-tag.h"
#include "game/game-cli.h"
#include "game/game-state.h"
#include "replay/replay.h"
#include "network/multiplayer-context.h"
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
    Perf::ScopedTimer _frame("FrameOverhead");
    auto tFrameStart = std::chrono::steady_clock::now();
    HEARTBEAT("FRAME START");

    float dt;
    bool worldPassRan;
    {
        Perf::ScopedTimer _t("Setup");
        engineTickSetup(engine, dt, worldPassRan);
    }
    {
        Perf::ScopedTimer _t("Audio");
        engineTickAudio(dt);
    }
    {
        Perf::ScopedTimer _t("State");
        engineTickState(engine, dt);
    }
    HEARTBEAT("after state");

    const bool replayExportForceRender =
        (getReplayExportJob().state == ReplayExportJob::Capturing ||
         getReplayExportJob().state == ReplayExportJob::Encoding) && WORLD_LOADED;

    if (GAME_STATE == GAME_PLAYING || replayExportForceRender)
    {
        { Perf::ScopedTimer _t("Replay"); engineTickReplay(engine, dt); } HEARTBEAT("after replay");
        { Perf::ScopedTimer _t("Networking"); engineTickNet(engine, dt); } HEARTBEAT("after net");
        { Perf::ScopedTimer _t("Camera"); engineTickCamera(engine, dt); } HEARTBEAT("after camera");
        { Perf::ScopedTimer _t("Combat"); engineTickCombat(engine, dt); } HEARTBEAT("after combat");
        { Perf::ScopedTimer _t("Rendering"); engineTickRender(engine, dt, worldPassRan); } HEARTBEAT("after render");
        { Perf::ScopedTimer _t("UI"); engineTickUI(engine, dt, worldPassRan); } HEARTBEAT("after ui");

        { Perf::ScopedTimer _t("DevOverlay");
        if (!gReplayExportRenderMode || ReplayExportUI::showDevOverlay)
            DevOverlay::instance().render();
        }
    }

    { Perf::ScopedTimer _t("Menus");
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
        if (Terminal::instance().isOpen()) {
            Terminal::instance().toggle();
            bool duelMatchOver = gDuelManager.phase() == DuelPhase::MatchEnd;
            glfwSetInputMode(engine.window(), GLFW_CURSOR,
                GAME_STATE == GAME_PLAYING && !duelMatchOver ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
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
            if (MP_CONTEXT.active) {
                MimitaNet::mpShutdown(MP_CONTEXT);
            }
            Debug::log(Debug::Category::Duel, "[DUEL] escape: transitioning to main menu");
            GAME_STATE = GAME_MENU;
            glfwSetInputMode(engine.window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    escapePrev = escapeDown;

    static bool f10Prev = false;
    bool f10Down = glfwGetKey(engine.window(), GLFW_KEY_F10) == GLFW_PRESS;
    if (f10Down && !f10Prev) {
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
    { Perf::ScopedTimer _t("Terminal"); Terminal::instance().render(); }
    { Perf::ScopedTimer _t("Diag"); diagRenderStage(8); }

    HEARTBEAT("end frame");
    { Perf::ScopedTimer _t("Swap"); engine.endFrame(); }
    { Perf::ScopedTimer _t("Diag"); diagRenderStage(9); }
    { Perf::ScopedTimer _t("Diag"); diagRenderFrameEnd(); }
    Perf::endFrame();
    { Perf::ScopedTimer _t("Sleep"); gFramePacer.endFrame(); }

    // ── Frame timing breakdown ───────────────────────
    {
        auto tNow = std::chrono::steady_clock::now();
        float frameMs = std::chrono::duration<float, std::milli>(tNow - tFrameStart).count();
        CHECK_SPIKE("FRAME TOTAL", frameMs, 20);

        static auto sLastReport = std::chrono::steady_clock::now();
        float sinceReport = std::chrono::duration<float>(tNow - sLastReport).count();
        if (sinceReport >= 1.0f) {
            sLastReport = tNow;

            Debug::logThrottled(Debug::Category::General, "frame-timing", 1.0f,
                "[FRAME TIMING] %.1fms  fps=%.0f  dt=%.3f\n",
                frameMs, 1000.0f / (frameMs > 0.1f ? frameMs : 1.0f), dt);
        }
    }
}
