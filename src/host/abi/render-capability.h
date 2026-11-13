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
    BindVertexArray = 8,
    BindBuffer = 9,
    UseProgram = 10,
    BindFramebuffer = 11,
    DepthTest = 12,
    Blend = 13,
    Cull = 14,
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

    // Bounded upload/attribute/uniform operations used while the draw path is
    // being migrated. Buffers/data pointers are valid only for the call.
    HostResult (*buffer_data)(void* hostUserData, std::uint32_t target,
                              std::uint64_t buffer, const void* data,
                              std::uint32_t size, std::uint32_t usage);
    HostResult (*tex_image_2d)(void* hostUserData, std::uint64_t texture,
                               std::int32_t level, std::int32_t internalFormat,
                               std::int32_t width, std::int32_t height,
                               std::int32_t border, std::uint32_t format,
                               std::uint32_t type, const void* data,
                               std::uint32_t size);
    HostResult (*tex_parameteri)(void* hostUserData, std::uint32_t target,
                                 std::uint32_t pname, std::int32_t value);
    HostResult (*vertex_attrib_pointer)(void* hostUserData, std::uint32_t index,
                                        std::int32_t size, std::uint32_t type,
                                        std::uint32_t normalized,
                                        std::int32_t stride,
                                        std::uint32_t offset);
    HostResult (*enable_vertex_attrib)(void* hostUserData, std::uint32_t index,
                                       std::uint32_t enabled);
    HostResult (*uniform1i)(void* hostUserData, std::int32_t location,
                            std::int32_t value);
    HostResult (*uniform1f)(void* hostUserData, std::int32_t location, float value);
    HostResult (*uniform3f)(void* hostUserData, std::int32_t location, float x,
                            float y, float z);
    HostResult (*uniform4f)(void* hostUserData, std::int32_t location, float x,
                            float y, float z, float w);
    HostResult (*uniform_matrix4fv)(void* hostUserData, std::int32_t location,
                                    std::uint32_t count, const float* values);
};

}  // namespace mimita::host
