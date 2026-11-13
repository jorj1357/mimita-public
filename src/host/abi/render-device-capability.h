// 2026-11-13
// Resident render-device lifecycle capability.
//
// The device, window, GL context, and Renderer instance stay resident. A game
// generation receives a stable Renderer pointer and forwards lifecycle calls;
// it never constructs or destroys the device.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kRenderDeviceCapabilityVersion = 1u;

struct RenderDeviceCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    void* (*acquire)(void* hostUserData, std::int32_t width, std::int32_t height,
                     const char* title);
    float (*begin_frame)(void* hostUserData);
    void (*end_frame)(void* hostUserData);
    std::int32_t (*should_close)(void* hostUserData);
    void (*shutdown)(void* hostUserData);
    std::int32_t (*apply_video_mode)(void* hostUserData, std::int32_t width,
                                     std::int32_t height, std::int32_t mode);
    void (*set_vsync)(void* hostUserData, std::int32_t enabled);
    bool (*reload_world_shader_if_changed)(void* hostUserData);
    void (*set_cursor_visible)(void* hostUserData, std::int32_t visible);
};

}  // namespace mimita::host
