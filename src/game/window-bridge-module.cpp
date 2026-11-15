// 2026-11-15
// Game-DLL side of the resident window/input capability.

#include "game/window-bridge-module.h"

#include <array>
#include <cstdint>
#include <cstring>

#include "host/abi/window-capability.h"
#include "input/input-router.h"
#include "input/mouse-lock.h"

namespace mimita::game {
namespace {
const mimita::host::WindowCapabilityV1* gWindow = nullptr;
void* gHostData = nullptr;
}  // namespace

void initializeHostWindow(const mimita::host::HostServicesV1* services) {
    gWindow = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) return;
    auto* capability = static_cast<const mimita::host::WindowCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Window),
            mimita::host::kWindowCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kWindowCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::WindowCapabilityV1)) {
        gWindow = capability;
    }
}

bool hostWindowSize(int& width, int& height) {
    std::int32_t w = 0, h = 0;
    if (!gWindow || !gWindow->get_size ||
        !gWindow->get_size(gHostData, &w, &h)) return false;
    width = w;
    height = h;
    return true;
}

bool hostFramebufferSize(int& width, int& height) {
    std::int32_t w = 0, h = 0;
    if (!gWindow || !gWindow->get_framebuffer_size ||
        !gWindow->get_framebuffer_size(gHostData, &w, &h)) return false;
    width = w;
    height = h;
    return true;
}

bool hostWindowPosition(int& x, int& y) {
    std::int32_t px = 0, py = 0;
    if (!gWindow || !gWindow->get_window_pos ||
        !gWindow->get_window_pos(gHostData, &px, &py)) return false;
    x = px;
    y = py;
    return true;
}

bool hostCursorPos(double& x, double& y) {
    double cx = 0.0, cy = 0.0;
    if (gWindow && gWindow->get_cursor_pos &&
        gWindow->get_cursor_pos(gHostData, &cx, &cy)) {
        x = cx;
        y = cy;
        return true;
    }
    return InputRouter::cursorPos(nullptr, &x, &y);
}

bool hostSetCursorPos(double x, double y) {
    return gWindow && gWindow->set_cursor_pos &&
           gWindow->set_cursor_pos(gHostData, x, y) != 0;
}

bool hostFocusWindow() {
    return gWindow && gWindow->focus && gWindow->focus(gHostData) != 0;
}

int hostCursorMode() {
    if (!gWindow || !gWindow->get_cursor_mode) return -1;
    return gWindow->get_cursor_mode(gHostData);
}

bool hostWindowFocused() {
    return gWindow && gWindow->get_focused &&
           gWindow->get_focused(gHostData) != 0;
}

bool hostWindowIconified() {
    return gWindow && gWindow->get_iconified &&
           gWindow->get_iconified(gHostData) != 0;
}

bool hostSetCursorMode(int mode) {
    return gWindow && gWindow->set_cursor_mode &&
           gWindow->set_cursor_mode(gHostData, mode) != 0;
}

bool hostMouseDown(int button) {
    if (gWindow && gWindow->mouse_button_down)
        return gWindow->mouse_button_down(gHostData, button) != 0;
    return InputRouter::mouseDown(button);
}

bool hostKeyDown(int key) {
    if (gWindow && gWindow->key_down)
        return gWindow->key_down(gHostData, key) != 0;
    return InputRouter::keyDown(key);
}

std::string hostClipboardGet() {
    std::array<char, 4096> buffer{};
    if (!gWindow || !gWindow->clipboard_get) return {};
    const std::int32_t copied =
        gWindow->clipboard_get(gHostData, buffer.data(),
                               static_cast<std::int32_t>(buffer.size()));
    if (copied <= 0) return {};
    return std::string(buffer.data(), static_cast<std::size_t>(copied));
}

bool hostClipboardSet(const std::string& text) {
    return gWindow && gWindow->clipboard_set &&
           gWindow->clipboard_set(gHostData, text.c_str()) != 0;
}

}  // namespace mimita::game
