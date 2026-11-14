// 2026-11-14
#pragma once

#include "host/abi/game-host-abi.h"

namespace mimita::game {
void handleGameInputEvent(const mimita::host::GameInputEventV1& event);
void handleGameInputKey(int key, int scancode, int action, int mods);
void handleGameInputChar(unsigned int codepoint);
void handleGameInputScroll(double xOffset, double yOffset);
void handleGameInputDrop(int count, const char** paths);
}  // namespace mimita::game
