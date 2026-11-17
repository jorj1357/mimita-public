// 2026-11-13
#include "game/window-bridge-module.h"
#include "game/transport-bridge-module.h"
#include "engine/whole-game-entry.h"

#include <new>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include "debug/structured-log.h"

#include "engine/engine.h"
#include "audio/audio.h"
#include "main-init.h"
#include "engine/engine-tick.h"
#include "network/multiplayer-context.h"
#include "perf/perf-frame.h"
#include "perf/perf-spike.h"
#include "terminal/terminal-state.h"
#include "devtools/terminal.h"

#include <vector>

namespace mimita::game {
namespace {

Engine* gWholeGameEngine = nullptr;
bool gWholeGameReady = false;
bool gFirstTickEmitted = false;
EngineFrameState gWholeGameFrameState;
std::vector<std::string> gStartupCommands;

void executePendingStartupCommands() {
    if (gStartupCommands.empty()) {
        return;
    }
    if (!gpMpContext || !gpMpContext->connected || !gpWorldLoaded ||
        !*gpWorldLoaded) {
        return;
    }
    for (const std::string& command : gStartupCommands) {
        Terminal::instance().execute(command);
    }
    gStartupCommands.clear();
}

}  // namespace

void queueStartupCommand(const std::string& command) {
    if (!command.empty()) {
        gStartupCommands.push_back(command);
    }
}

bool initializeWholeGameEntry(int width, int height, const char* title) {
    if (gWholeGameReady)
        return true;
    gWholeGameEngine = new (std::nothrow) Engine();
    if (!gWholeGameEngine)
        return false;
    gWholeGameEngine->init(width, height, title);
    if (!gWholeGameEngine->renderer) {
        delete gWholeGameEngine;
        gWholeGameEngine = nullptr;
        return false;
    }
    gWholeGameFrameState = EngineFrameState{};
    gameInit(0, nullptr, *gWholeGameEngine);
    if (GLFWwindow* window = gWholeGameEngine->window()) {
        const bool focusedBefore =
            mimita::game::hostWindowFocused();
        if (!focusedBefore) mimita::game::hostFocusWindow();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.window-focus", "input",
            "hot client focused its gameplay window after generation init", 0,
            nlohmann::json{
                {"focused_before", focusedBefore},
                {"focused_after", mimita::game::hostWindowFocused()},
                {"iconified", mimita::game::hostWindowIconified()},
                {"visible", true}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    gFirstTickEmitted = false;
    gWholeGameReady = true;
    return true;
}

bool wholeGameShouldClose() {
    // A freshly activated generation is not initialized yet. Let the host loop
    // call tick once so lazy bootstrap can acquire the resident renderer.
    if (!gWholeGameReady || !gWholeGameEngine)
        return false;
    return !gWholeGameEngine->running();
}

void tickWholeGame(double deltaSeconds) {
    (void)deltaSeconds;
    if (!gWholeGameReady || !gWholeGameEngine)
        return;
    if (!gFirstTickEmitted)
    {
        gFirstTickEmitted = true;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "game.generation.first_tick", "generation",
            "first gameplay tick after generation initialization", 0, {},
            __FILE__, __LINE__, __FUNCTION__);
        mimita::host::TransportSwapStateV1 swapState{};
        const bool swapStateKnown =
            mimita::game::queryTransportSwapState(&swapState);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "network.hotswap.queue_drained", "generation",
            swapStateKnown
                ? "generation attached to the resident transport swap state"
                : "resident transport swap state unavailable",
            0,
            nlohmann::json{{"result", swapStateKnown ? "skipped" : "failed"},
                           {"reason_code", "resident_pump_not_installed"},
                           {"messages", swapStateKnown
                                ? swapState.queuedEvents : 0},
                           {"bytes", 0},
                           {"overflow", false},
                           {"session_count", swapStateKnown
                                ? swapState.sessionCount : 0},
                           {"queued_events", swapStateKnown
                                ? swapState.queuedEvents : 0},
                           {"suspended", swapStateKnown
                                ? (swapState.suspended != 0) : false},
                           {"generation_attached", swapStateKnown
                                ? (swapState.generationAttached != 0) : false}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    engineTickSimulate(*gWholeGameEngine, gWholeGameFrameState);
    { MIMITA_PERF_SCOPE("EngineTick::StartupCommands");
      executePendingStartupCommands(); }
}

void renderWholeGame(void* renderContext) {
    (void)renderContext;
    if (!gWholeGameReady || !gWholeGameEngine)
        return;
    // The host calls tick then render every frame, so the render half completes
    // the pacing/perf frame that tickWholeGame started.
    engineTickRenderFrame(*gWholeGameEngine, gWholeGameFrameState);
}

void shutdownWholeGameEntry() {
    // Stop generation-owned workers before unload so no thread can enter code
    // that the host is about to replace.
    shutdownAudio();
    perfStopResourceWatchdog();
    // The resident host owns the device and Renderer. Do not call
    // Engine::shutdown here: in DLL mode it would shut down the resident device.
    delete gWholeGameEngine;
    gWholeGameEngine = nullptr;
    gWholeGameReady = false;
    gStartupCommands.clear();
}

}  // namespace mimita::game
