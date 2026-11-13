#pragma once

#include <chrono>
#include <string>

struct Engine;

// Per-frame state shared between the simulation and render halves of one
// engine frame. The host tick owns the simulation half; the render half runs
// after it in the same frame.
struct EngineFrameState {
    float dt = 0.0f;
    bool worldPassRan = false;
    std::chrono::steady_clock::time_point frameStart;
    float frameMs = 0.0f;
};

void engineTick(Engine& engine);
void engineTickSimulate(Engine& engine, EngineFrameState& state);
void engineTickRenderFrame(Engine& engine, EngineFrameState& state);
void engineSetRoomCodeFilePath(const std::string& path);
