// 2026-11-13
#include "engine/whole-game-entry.h"

#include <new>

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include "debug/structured-log.h"

#include "engine/engine.h"
#include "main-init.h"
#include "engine/engine-tick.h"
#include "network/multiplayer-context.h"
#include "perf/perf-frame.h"
#include "terminal/terminal-state.h"
#include "devtools/terminal.h"

#include <vector>

namespace mimita::game {
namespace {

Engine* gWholeGameEngine = nullptr;
bool gWholeGameReady = false;
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
            glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
        if (!focusedBefore) glfwFocusWindow(window);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::General, StructuredLevel::Important,
            "input.window-focus", "input",
            "hot client focused its gameplay window after generation init", 0,
            nlohmann::json{
                {"focused_before", focusedBefore},
                {"focused_after", glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0},
                {"iconified", glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0},
                {"visible", glfwGetWindowAttrib(window, GLFW_VISIBLE) != 0}},
            __FILE__, __LINE__, __FUNCTION__);
    }
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
    engineTickSimulate(*gWholeGameEngine, gWholeGameFrameState);
    executePendingStartupCommands();
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
    // Stop the module-owned watchdog before unload so its thread cannot keep
    // reading this generation's globals after the DLL is replaced.
    perfStopResourceWatchdog();
    // The resident host owns the device and Renderer. Do not call
    // Engine::shutdown here: in DLL mode it would shut down the resident device.
    delete gWholeGameEngine;
    gWholeGameEngine = nullptr;
    gWholeGameReady = false;
    gStartupCommands.clear();
}

}  // namespace mimita::game
