// 2026-11-15
// Temporary hot-DLL input fallback.
// The game DLL currently links its own static GLFW copy, so GLFW polling sees
// neither the resident window nor its key/mouse state. This adapter reads the
// already-created Win32 window directly until CAP_INPUT is promoted.
#pragma once

struct GLFWwindow;

namespace HostInputFallback {

void setGameWindow(GLFWwindow* window);
void* gameWindowHandle();
bool gameWindowFocused();
bool focusGameWindow();
bool mouseDown(int glfwButton);
bool cursorPos(GLFWwindow* window, double* x, double* y);
bool keyDown(int glfwKey);

}  // namespace HostInputFallback
