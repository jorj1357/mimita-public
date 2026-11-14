// 2026-11-13
#include "engine/whole-game-entry.h"

#include <new>

#include "engine/engine.h"
#include "main-init.h"
#include "engine/engine-tick.h"

namespace mimita::game {
namespace {

Engine* gWholeGameEngine = nullptr;
bool gWholeGameReady = false;
EngineFrameState gWholeGameFrameState;

}  // namespace

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
    // The resident host owns the device and Renderer. Do not call
    // Engine::shutdown here: in DLL mode it would shut down the resident device.
    delete gWholeGameEngine;
    gWholeGameEngine = nullptr;
    gWholeGameReady = false;
}

}  // namespace mimita::game
