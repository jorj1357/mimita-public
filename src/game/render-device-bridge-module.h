// 2026-11-13
#pragma once

#include "host/abi/game-host-abi.h"

struct Renderer;

namespace mimita::game {
void initializeHostRenderDevice(const mimita::host::HostServicesV1* services);
Renderer* acquireHostRenderer(int width, int height, const char* title);
void setHostCursorVisible(bool visible);
}  // namespace mimita::game
