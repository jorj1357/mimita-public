// 2026-11-12
// Versioned render command-stream capability.
//
// The game generation submits bounded draw commands and primitive upload /
// shader / state operations. The resident host owns the device, backend state,
// and the final flush. The game never calls OpenGL or holds a device handle
// directly.
//
// Shaders are hot: the game owns program handles, sources, reload policy, and
// the uniform-location cache. The host only performs the primitive operations.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"
#include "host/abi/render-types.h"

namespace mimita::host {

inline constexpr std::uint32_t kRenderCapabilityVersion = 2u;
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
    // Semantic-target bind; ints[0] is mimita::render::TextureTarget.
    BindTextureTarget = 15,
    ActiveTexture = 16,
    EnableCapability = 17,
    DisableCapability = 18,
    DepthMask = 19,
    DepthFunc = 20,
    CullFaceMode = 21,
    BlendFunc = 22,
    BlendFuncSeparate = 23,
    PolygonMode = 24,
    Scissor = 25,
    LineWidth = 26,
    ClearColor = 27,
    PushState = 28,
    PopState = 29,
    DrawArraysInstanced = 30,
    VertexAttribDivisor = 31,
    TexParameterfv = 32,
    DrawBuffer = 33,
    ReadBuffer = 34,
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

    // Bounded upload/attribute operations. Data pointers are valid only for the
    // call; the host must not retain them.
    HostResult (*buffer_data)(void* hostUserData, mimita::render::BufferTarget target,
                              std::uint64_t buffer, const void* data,
                              std::uint32_t size, mimita::render::BufferUsage usage);
    HostResult (*buffer_sub_data)(void* hostUserData,
                                  mimita::render::BufferTarget target,
                                  std::uint64_t buffer, std::uint32_t offset,
                                  const void* data, std::uint32_t size);
    HostResult (*tex_image_2d)(void* hostUserData,
                               mimita::render::TextureTarget target,
                               std::int32_t level,
                               mimita::render::TextureFormat format,
                               std::int32_t width, std::int32_t height,
                               std::int32_t border,
                               mimita::render::PixelFormat pixelFormat,
                               mimita::render::PixelType pixelType,
                               const void* data, std::uint32_t size);
    HostResult (*tex_parameteri)(void* hostUserData,
                                 mimita::render::TextureTarget target,
                                 mimita::render::TextureParameter parameter,
                                 std::int32_t value);
    HostResult (*vertex_attrib_pointer)(void* hostUserData, std::uint32_t index,
                                        std::int32_t size,
                                        mimita::render::VertexType type,
                                        std::uint32_t normalized,
                                        std::int32_t stride,
                                        std::uint32_t offset);
    HostResult (*enable_vertex_attrib)(void* hostUserData, std::uint32_t index,
                                       std::uint32_t enabled);

    // Uniform uploads by cached location.
    HostResult (*uniform1i)(void* hostUserData, std::int32_t location,
                            std::int32_t value);
    HostResult (*uniform1f)(void* hostUserData, std::int32_t location, float value);
    HostResult (*uniform2f)(void* hostUserData, std::int32_t location, float x,
                            float y);
    HostResult (*uniform3f)(void* hostUserData, std::int32_t location, float x,
                            float y, float z);
    HostResult (*uniform4f)(void* hostUserData, std::int32_t location, float x,
                            float y, float z, float w);
    HostResult (*uniform1fv)(void* hostUserData, std::int32_t location,
                             std::int32_t count, const float* values);
    HostResult (*uniform2fv)(void* hostUserData, std::int32_t location,
                             std::int32_t count, const float* values);
    HostResult (*uniform3fv)(void* hostUserData, std::int32_t location,
                             std::int32_t count, const float* values);
    HostResult (*uniform_matrix4fv)(void* hostUserData, std::int32_t location,
                                    std::uint32_t count, const float* values);

    // Primitive shader operations. The game owns the program and the cache.
    std::uint32_t (*create_shader)(void* hostUserData,
                                   mimita::render::ShaderStage stage);
    HostResult (*shader_source)(void* hostUserData, std::uint32_t shader,
                                const char* source);
    HostResult (*compile_shader)(void* hostUserData, std::uint32_t shader);
    std::uint32_t (*create_program)(void* hostUserData);
    HostResult (*attach_shader)(void* hostUserData, std::uint32_t program,
                                std::uint32_t shader);
    HostResult (*link_program)(void* hostUserData, std::uint32_t program);
    std::int32_t (*get_uniform_location)(void* hostUserData,
                                         std::uint32_t program, const char* name);
    HostResult (*get_uniformiv)(void* hostUserData, std::uint32_t program,
                                std::int32_t location, std::int32_t* values,
                                std::int32_t count);
    HostResult (*get_programiv)(void* hostUserData, std::uint32_t program,
                                mimita::render::ShaderParameter parameter,
                                std::int32_t* value);
    HostResult (*get_shaderiv)(void* hostUserData, std::uint32_t shader,
                               mimita::render::ShaderParameter parameter,
                               std::int32_t* value);
    HostResult (*get_program_info_log)(void* hostUserData, std::uint32_t program,
                                       std::int32_t capacity,
                                       std::int32_t* length, char* log);
    HostResult (*get_shader_info_log)(void* hostUserData, std::uint32_t shader,
                                      std::int32_t capacity,
                                      std::int32_t* length, char* log);
    std::uint32_t (*get_error)(void* hostUserData);

    // Render targets.
    HostResult (*framebuffer_texture_2d)(
        void* hostUserData, mimita::render::TextureTarget target,
        mimita::render::FramebufferAttachment attachment, std::uint64_t texture);
    HostResult (*bind_renderbuffer)(void* hostUserData, std::uint32_t renderbuffer);
    HostResult (*renderbuffer_storage)(void* hostUserData,
                                       mimita::render::RenderbufferFormat format,
                                       std::int32_t width, std::int32_t height);
    HostResult (*framebuffer_renderbuffer)(
        void* hostUserData, mimita::render::FramebufferAttachment attachment,
        std::uint32_t renderbuffer);
    std::int32_t (*check_framebuffer_status)(void* hostUserData);
};

}  // namespace mimita::host
