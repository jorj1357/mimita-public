// 2026-11-15
// Resident window/input capability.
//
// The game DLL has its own static GLFW copy and must never call GLFW against a
// resident window pointer. These calls are answered by the resident host, which
// owns the window and its GLFW state. Plain values and bounded buffers only.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kWindowCapabilityVersion = 1u;

enum class HostCursorMode : std::int32_t {
    Normal = 0,
    Hidden = 1,
    Disabled = 2,
};

struct WindowCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::int32_t (*get_size)(void* hostUserData, std::int32_t* width,
                             std::int32_t* height);
    std::int32_t (*get_framebuffer_size)(void* hostUserData, std::int32_t* width,
                                         std::int32_t* height);
    std::int32_t (*get_window_pos)(void* hostUserData, std::int32_t* x,
                                   std::int32_t* y);
    std::int32_t (*get_cursor_pos)(void* hostUserData, double* x, double* y);
    std::int32_t (*set_cursor_pos)(void* hostUserData, double x, double y);
    std::int32_t (*focus)(void* hostUserData);
    std::int32_t (*get_cursor_mode)(void* hostUserData);
    std::int32_t (*get_focused)(void* hostUserData);
    std::int32_t (*get_iconified)(void* hostUserData);
    std::int32_t (*set_cursor_mode)(void* hostUserData, std::int32_t mode);
    std::int32_t (*mouse_button_down)(void* hostUserData, std::int32_t button);
    std::int32_t (*key_down)(void* hostUserData, std::int32_t key);

    // Copies a NUL-terminated clipboard string. Returns the copied length, or
    // -1 when unavailable. The host never returns a resident pointer.
    std::int32_t (*clipboard_get)(void* hostUserData, char* out,
                                  std::int32_t capacity);
    std::int32_t (*clipboard_set)(void* hostUserData, const char* text);
};

}  // namespace mimita::host
