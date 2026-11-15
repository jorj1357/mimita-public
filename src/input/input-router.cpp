// 2026-11-15
// Hot normalized input owner. No allocations, locks, or filesystem work here.

#include "input/input-router.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <GLFW/glfw3.h>
#include <windows.h>
#include <nlohmann/json.hpp>

#include "camera.h"
#include "config/camera-config.h"
#include "input/host-input-fallback.h"
#include "debug/structured-log.h"

namespace InputRouter {
namespace {

constexpr int kKeyCount = 512;
constexpr int kMouseCount = 3;

std::array<bool, kKeyCount> gKeyDown{};
std::array<bool, kMouseCount> gMouseDown{};
std::array<bool, kMouseCount> gMouseInjected{};
GLFWwindow* gWindow = nullptr;
bool gFocused = false;
bool gHasHostKeys = false;
double gCursorX = 0.0;
double gCursorY = 0.0;
double gLastCursorX = 0.0;
double gLastCursorY = 0.0;
bool gHasCursor = false;
float gPendingLookX = 0.0f;
float gPendingLookY = 0.0f;
std::uint64_t gFrameId = 0;
std::uint64_t gClientTick = 0;
std::uint64_t gServerTick = 0;
double gLastMouseInputMs = -1.0;
Source gLookSource = Source::Unknown;
bool gPointerLocked = false;

double nowMs() {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace

void setWindow(GLFWwindow* window) {
    gWindow = window;
    HostInputFallback::setGameWindow(window);
}

GLFWwindow* window() { return gWindow; }

void beginFrame(std::uint64_t clientTick, std::uint64_t serverTick) {
    ++gFrameId;
    gClientTick = clientTick;
    gServerTick = serverTick;
    gFocused = HostInputFallback::gameWindowFocused();
    for (int button = 0; button < kMouseCount; ++button) {
        if (!gMouseInjected[button])
            gMouseDown[button] = HostInputFallback::mouseDown(button);
        gMouseInjected[button] = false;
    }
    if (!gFocused) {
        gHasCursor = false;
        return;
    }

    double x = 0.0;
    double y = 0.0;
    if (!HostInputFallback::cursorPos(gWindow, &x, &y)) return;
    if (!gHasCursor) {
        gLastCursorX = x;
        gLastCursorY = y;
        gHasCursor = true;
    }
    const double dx = x - gLastCursorX;
    const double dy = y - gLastCursorY;
    gLastCursorX = x;
    gLastCursorY = y;
    gCursorX = x;
    gCursorY = y;

    // Host mouse movement is sampled through HostInputFallback when the
    // resident callback is not producing camera input.
    if (HostInputFallback::gameWindowFocused() &&
        (std::abs(dx) > 0.0 || std::abs(dy) > 0.0)) {
        const double sinceHostMouse = gLastMouseInputMs < 0.0
            ? 1.0e9 : nowMs() - gLastMouseInputMs;
        if (sinceHostMouse > 50.0) {
            gPendingLookX += static_cast<float>(dx);
            gPendingLookY += static_cast<float>(dy);
            gLookSource = Source::Win32;
        }
    }
}

void ingestKey(int key, int, int action, int, Source source) {
    if (key < 0 || key >= kKeyCount) return;
    if (source == Source::Host) gHasHostKeys = true;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        gKeyDown[key] = true;
    } else if (action == GLFW_RELEASE) {
        gKeyDown[key] = false;
    }
}

void ingestMouseButton(int button, int action, Source) {
    if (button < 0 || button >= kMouseCount) return;
    gMouseDown[button] = action == GLFW_PRESS;
    if (action == GLFW_PRESS) gMouseInjected[button] = true;
}

void ingestCursor(double x, double y, Source) {
    if (!gHasCursor) {
        gLastCursorX = x;
        gLastCursorY = y;
        gHasCursor = true;
    }
    gCursorX = x;
    gCursorY = y;
}

void ingestText(unsigned int, Source) {}

void ingestFocus(bool focused, Source source) {
    gFocused = focused;
    if (source == Source::Host) gHasHostKeys = true;
}

void ingestLookDelta(float dx, float dy, Source source) {
    gPendingLookX += dx;
    gPendingLookY += dy;
    gLastMouseInputMs = nowMs();
    gLookSource = source;
}

bool keyDown(int key) {
    return key >= 0 && key < kKeyCount && gKeyDown[key];
}

bool mouseDown(int button) {
    return button >= 0 && button < kMouseCount && gMouseDown[button];
}

bool focused() { return gFocused || HostInputFallback::gameWindowFocused(); }
bool hasHostKeys() { return gHasHostKeys; }
glm::vec2 lookDelta() { return {gPendingLookX, gPendingLookY}; }
bool cursorPos(GLFWwindow*, double* x, double* y) {
    if (!x || !y) return false;
    *x = gCursorX;
    *y = gCursorY;
    return gHasCursor;
}
double cursorX() { return gCursorX; }
double cursorY() { return gCursorY; }
std::uint64_t frameId() { return gFrameId; }
std::uint64_t clientTick() { return gClientTick; }
std::uint64_t serverTick() { return gServerTick; }

void applyPendingLook(Camera& camera) {
    if (gPendingLookX == 0.0f && gPendingLookY == 0.0f) return;
    if (gLookSource != Source::Agent &&
        camera.lastMouseInputTime >= 0.0 &&
        nowMs() - camera.lastMouseInputTime < 50.0) {
        gPendingLookX = 0.0f;
        gPendingLookY = 0.0f;
        return;
    }
    const float sensitivity = CAMERA_SENS;
    camera.yaw -= gPendingLookX * sensitivity;
    camera.pitch += gPendingLookY * sensitivity;
    if (CamConfig::instance().data().orientationMode != "head_world") {
        camera.pitch = glm::clamp(camera.pitch, -89.9f, 89.9f);
    }
    camera.updateVectors();
    static double lastLookLogMs = -1.0;
    if (lastLookLogMs < 0.0 || nowMs() - lastLookLogMs >= 500.0) {
        lastLookLogMs = nowMs();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Camera, StructuredLevel::Important,
            "camera.look-input", "input", "normalized look delta applied", 0,
            nlohmann::json{{"source", static_cast<int>(gLookSource)},
                           {"dx", gPendingLookX},
                           {"dy", gPendingLookY},
                           {"yaw", camera.yaw},
                           {"pitch", camera.pitch},
                           {"third_person", camera.thirdPerson}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    gPendingLookX = 0.0f;
    gPendingLookY = 0.0f;
    gLookSource = Source::Unknown;
}

bool gameWindowFocused() { return HostInputFallback::gameWindowFocused(); }
bool focusGameWindow() { return HostInputFallback::focusGameWindow(); }

void setPointerLock(bool locked) {
    gPointerLocked = locked;
    HWND window = static_cast<HWND>(HostInputFallback::gameWindowHandle());
    if (!window) return;
    if (locked) {
        RECT rect{};
        if (GetClientRect(window, &rect)) {
            POINT topLeft{rect.left, rect.top};
            POINT bottomRight{rect.right, rect.bottom};
            ClientToScreen(window, &topLeft);
            ClientToScreen(window, &bottomRight);
            RECT screenRect{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
            ClipCursor(&screenRect);
        }
    } else {
        ClipCursor(nullptr);
        gHasCursor = false;
    }
}

bool pointerLocked() { return gPointerLocked; }

}  // namespace InputRouter
