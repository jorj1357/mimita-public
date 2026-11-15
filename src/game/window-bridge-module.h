// 2026-11-15
// Game-DLL side of the resident window/input capability. GUI/editor/chat code
// calls this instead of passing a resident GLFWwindow into the DLL-local GLFW.

#pragma once

#include <string>

#include "host/abi/game-host-abi.h"

namespace mimita::game {

void initializeHostWindow(const mimita::host::HostServicesV1* services);

bool hostWindowSize(int& width, int& height);
bool hostFramebufferSize(int& width, int& height);
bool hostWindowPosition(int& x, int& y);
bool hostCursorPos(double& x, double& y);
bool hostSetCursorPos(double x, double y);
bool hostFocusWindow();
int hostCursorMode();
bool hostWindowFocused();
bool hostWindowIconified();
bool hostSetCursorMode(int mode);
bool hostMouseDown(int button);
bool hostKeyDown(int key);
std::string hostClipboardGet();
bool hostClipboardSet(const std::string& text);

}  // namespace mimita::game
