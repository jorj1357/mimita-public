// 2026-11-12
// Versioned render command-stream capability.
//
// The game generation submits bounded draw commands. The resident host owns the
// device, GL/backend state, shader/texture/buffer handles, and the final flush.
// The game never calls OpenGL or holds a device resource handle directly.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kRenderCapabilityVersion = 1u;
inline constexpr std::uint32_t kRenderMaxCommandsPerSubmit = 4096u;

enum class RenderOp : std::uint32_t {
    Clear = 1,
    Viewport = 2,
    BindTexture = 3,
    BindShader = 4,
    SetState = 5,
    DrawArrays = 6,
    DrawElements = 7,
};

struct RenderCommandV1 {
    std::uint32_t op;
    std::uint32_t flags;
    std::uint64_t handle0;
    std::uint64_t handle1;
    float floats[8];
    std::int32_t ints[4];
};

struct RenderCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    HostResult (*submit)(void* hostUserData, const RenderCommandV1* commands,
                         std::uint32_t count);
    HostResult (*flush)(void* hostUserData);
    std::uint32_t (*pending)(void* hostUserData);
};

}  // namespace mimita::host
