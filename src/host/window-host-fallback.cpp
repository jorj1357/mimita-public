// 2026-11-15
// EXE-side fallback for the game-DLL window capability.
//
// The hot game DLL answers these calls through WindowCapabilityV1. The resident
// EXE uses the same API internally, so it needs a local implementation that
// reads the resident InputRouter window instead of a DLL-local GLFW copy.

#include <GLFW/glfw3.h>

#include <cstring>
#include <string>

#include "game/render-device-bridge-module.h"
#include "game/window-bridge-module.h"
#include "input/input-router.h"

namespace mimita::game {
namespace {
GLFWwindow* residentWindow() {
    return InputRouter::window();
}
}  // namespace

bool hostWindowSize(int& width, int& height) {
    GLFWwindow* window = residentWindow();
    if (!window) return false;
    glfwGetWindowSize(window, &width, &height);
    return true;
}

bool hostFramebufferSize(int& width, int& height) {
    GLFWwindow* window = residentWindow();
    if (!window) return false;
    glfwGetFramebufferSize(window, &width, &height);
    return true;
}

bool hostWindowPosition(int& x, int& y) {
    GLFWwindow* window = residentWindow();
    if (!window) return false;
    glfwGetWindowPos(window, &x, &y);
    return true;
}

bool hostCursorPos(double& x, double& y) {
    GLFWwindow* window = residentWindow();
    if (!window) return false;
    glfwGetCursorPos(window, &x, &y);
    return true;
}

bool hostSetCursorPos(double x, double y) {
    GLFWwindow* window = residentWindow();
    if (!window) return false;
    glfwSetCursorPos(window, x, y);
    return true;
}

bool hostFocusWindow() {
    GLFWwindow* window = residentWindow();
    if (!window) return false;
    glfwFocusWindow(window);
    return true;
}

int hostCursorMode() {
    GLFWwindow* window = residentWindow();
    if (!window) return -1;
    const int mode = glfwGetInputMode(window, GLFW_CURSOR);
    if (mode == GLFW_CURSOR_DISABLED) return 2;
    if (mode == GLFW_CURSOR_HIDDEN) return 1;
    return 0;
}

bool hostWindowFocused() {
    GLFWwindow* window = residentWindow();
    return window && glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0;
}

bool hostWindowIconified() {
    GLFWwindow* window = residentWindow();
    return window && glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
}

bool hostSetCursorMode(int mode) {
    GLFWwindow* window = residentWindow();
    if (!window || mode < 0 || mode > 2) return false;
    const int glfwMode = mode == 2 ? GLFW_CURSOR_DISABLED
        : mode == 1 ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(window, GLFW_CURSOR, glfwMode);
    return glfwGetInputMode(window, GLFW_CURSOR) == glfwMode;
}

bool hostMouseDown(int button) {
    GLFWwindow* window = residentWindow();
    return window && glfwGetMouseButton(window, button) == GLFW_PRESS;
}

bool hostKeyDown(int key) {
    GLFWwindow* window = residentWindow();
    return window && glfwGetKey(window, key) == GLFW_PRESS;
}

std::string hostClipboardGet() {
    GLFWwindow* window = residentWindow();
    const char* text = window ? glfwGetClipboardString(window) : nullptr;
    return text ? std::string(text) : std::string();
}

bool hostClipboardSet(const std::string& text) {
    GLFWwindow* window = residentWindow();
    if (!window) return false;
    glfwSetClipboardString(window, text.c_str());
    return true;
}

void setHostCursorVisible(bool visible) {
    hostSetCursorMode(visible ? 0 : 2);
}

}  // namespace mimita::game
