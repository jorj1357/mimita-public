// 2026-11-12
// Game-DLL render phone for CAP_RENDER.
//
// The generation submits bounded draw commands and primitive upload / shader /
// state operations; the resident host owns the device and flushes the stream.
// Shaders are hot: this file owns no program registry or location cache.

#include "host/abi/game-host-abi.h"
#include "game/render-bridge-module.h"
#include "host/abi/render-capability.h"
#include "render/render-command.h"

namespace mimita::game {

namespace {
const mimita::host::RenderCapabilityV1* gRender = nullptr;
void* gHostData = nullptr;

using mimita::host::HostResult;
using mimita::host::RenderCommandV1;
using mimita::host::RenderOp;

void submitOp(RenderOp op, std::uint32_t flags = 0, std::uint64_t handle0 = 0,
              std::uint64_t handle1 = 0, const float* floats = nullptr,
              const std::int32_t* ints = nullptr) {
    if (!gRender || !gRender->submit)
        return;
    RenderCommandV1 command{};
    command.op = static_cast<std::uint32_t>(op);
    command.flags = flags;
    command.handle0 = handle0;
    command.handle1 = handle1;
    if (floats) {
        for (int i = 0; i < 8; ++i)
            command.floats[i] = floats[i];
    }
    if (ints) {
        for (int i = 0; i < 4; ++i)
            command.ints[i] = ints[i];
    }
    gRender->submit(gHostData, &command, 1);
}
}  // namespace

void initializeHostRender(const mimita::host::HostServicesV1* services) {
    gRender = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::RenderCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Render),
            mimita::host::kRenderCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kRenderCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::RenderCapabilityV1)) {
        gRender = capability;
    }
}

mimita::host::HostResult submitRenderCommands(
    const mimita::host::RenderCommandV1* commands, std::uint32_t count) {
    if (!gRender || !gRender->submit || !commands || count == 0) {
        return mimita::host::HostResult::Unsupported;
    }
    return gRender->submit(gHostData, commands, count);
}

mimita::host::HostResult flushRender() {
    if (!gRender || !gRender->flush) {
        return mimita::host::HostResult::Unsupported;
    }
    return gRender->flush(gHostData);
}

std::uint32_t pendingRenderCommands() {
    return gRender && gRender->pending ? gRender->pending(gHostData) : 0;
}

namespace render {

void bindVertexArray(VertexArray vertexArray) {
    submitOp(RenderOp::BindVertexArray, 0, vertexArray);
}

void bindBuffer(BufferTarget target, Buffer buffer) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(target), 0, 0, 0};
    submitOp(RenderOp::BindBuffer, 0, buffer, 0, nullptr, ints);
}

void bindTexture(TextureTarget target, Texture texture) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(target), 0, 0, 0};
    submitOp(RenderOp::BindTextureTarget, 0, texture, 0, nullptr, ints);
}

void useProgram(Program program) {
    submitOp(RenderOp::UseProgram, 0, program);
}

void bindFramebuffer(FramebufferTarget target, Framebuffer framebuffer) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(target), 0, 0, 0};
    submitOp(RenderOp::BindFramebuffer, 0, framebuffer, 0, nullptr, ints);
}

void activeTexture(std::uint32_t unit) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(unit), 0, 0, 0};
    submitOp(RenderOp::ActiveTexture, 0, 0, 0, nullptr, ints);
}

void enable(Capability capability) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(capability), 0, 0, 0};
    submitOp(RenderOp::EnableCapability, 0, 0, 0, nullptr, ints);
}

void disable(Capability capability) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(capability), 0, 0, 0};
    submitOp(RenderOp::DisableCapability, 0, 0, 0, nullptr, ints);
}

void depthMask(bool enabled) {
    submitOp(RenderOp::DepthMask, enabled ? 1u : 0u);
}

void depthFunc(DepthFunc func) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(func), 0, 0, 0};
    submitOp(RenderOp::DepthFunc, 0, 0, 0, nullptr, ints);
}

void cullFace(CullFaceMode mode) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(mode), 0, 0, 0};
    submitOp(RenderOp::CullFaceMode, 0, 0, 0, nullptr, ints);
}

void blendFunc(BlendFactor source, BlendFactor destination) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(source),
                                  static_cast<std::int32_t>(destination), 0, 0};
    submitOp(RenderOp::BlendFunc, 0, 0, 0, nullptr, ints);
}

void blendFuncSeparate(BlendFactor sourceRgb, BlendFactor destinationRgb,
                       BlendFactor sourceAlpha, BlendFactor destinationAlpha) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(sourceRgb),
                                  static_cast<std::int32_t>(destinationRgb),
                                  static_cast<std::int32_t>(sourceAlpha),
                                  static_cast<std::int32_t>(destinationAlpha)};
    submitOp(RenderOp::BlendFuncSeparate, 0, 0, 0, nullptr, ints);
}

void polygonMode(PolygonMode mode) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(mode), 0, 0, 0};
    submitOp(RenderOp::PolygonMode, 0, 0, 0, nullptr, ints);
}

void scissor(std::int32_t x, std::int32_t y, std::int32_t width,
             std::int32_t height) {
    const std::int32_t ints[4] = {x, y, width, height};
    submitOp(RenderOp::Scissor, 0, 0, 0, nullptr, ints);
}

void lineWidth(float width) {
    const float floats[8] = {width, 0, 0, 0, 0, 0, 0, 0};
    submitOp(RenderOp::LineWidth, 0, 0, 0, floats);
}

void clearColor(float red, float green, float blue, float alpha) {
    const float floats[8] = {red, green, blue, alpha, 0, 0, 0, 0};
    submitOp(RenderOp::ClearColor, 0, 0, 0, floats);
}

void viewport(std::int32_t x, std::int32_t y, std::int32_t width,
              std::int32_t height) {
    const std::int32_t ints[4] = {x, y, width, height};
    submitOp(RenderOp::Viewport, 0, 0, 0, nullptr, ints);
}

void clear(bool color, bool depth, bool stencil) {
    std::uint32_t flags = 0;
    if (color) flags |= 1u;
    if (depth) flags |= 2u;
    if (stencil) flags |= 4u;
    submitOp(RenderOp::Clear, flags);
}

void pushState() {
    submitOp(RenderOp::PushState);
}

void popState() {
    submitOp(RenderOp::PopState);
}

void bufferData(BufferTarget target, Buffer buffer, const void* data,
                std::uint32_t size, BufferUsage usage) {
    if (gRender && gRender->buffer_data)
        gRender->buffer_data(gHostData, target, buffer, data, size, usage);
}

void bufferSubData(BufferTarget target, Buffer buffer, std::uint32_t offset,
                   const void* data, std::uint32_t size) {
    if (gRender && gRender->buffer_sub_data)
        gRender->buffer_sub_data(gHostData, target, buffer, offset, data, size);
}

void texImage2D(TextureTarget target, std::int32_t level, TextureFormat format,
                std::int32_t width, std::int32_t height, std::int32_t border,
                PixelFormat pixelFormat, PixelType pixelType, const void* data) {
    if (gRender && gRender->tex_image_2d)
        gRender->tex_image_2d(gHostData, target, level, format, width, height,
                              border, pixelFormat, pixelType, data, 0);
}

void texParameteri(TextureTarget target, TextureParameter parameter,
                   TextureValue value) {
    texParameteri(target, parameter, static_cast<std::int32_t>(value));
}

void texParameteri(TextureTarget target, TextureParameter parameter,
                   std::int32_t value) {
    if (gRender && gRender->tex_parameteri)
        gRender->tex_parameteri(gHostData, target, parameter, value);
}

void vertexAttribPointer(std::uint32_t index, std::int32_t size,
                         VertexType type, bool normalized, std::int32_t stride,
                         std::uint32_t offset) {
    if (gRender && gRender->vertex_attrib_pointer)
        gRender->vertex_attrib_pointer(gHostData, index, size, type,
                                       normalized ? 1u : 0u, stride, offset);
}

void enableVertexAttribArray(std::uint32_t index) {
    if (gRender && gRender->enable_vertex_attrib)
        gRender->enable_vertex_attrib(gHostData, index, 1);
}

void disableVertexAttribArray(std::uint32_t index) {
    if (gRender && gRender->enable_vertex_attrib)
        gRender->enable_vertex_attrib(gHostData, index, 0);
}

void drawArrays(Primitive mode, std::int32_t first, std::int32_t count) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(mode), first, count, 0};
    submitOp(RenderOp::DrawArrays, 0, 0, 0, nullptr, ints);
}

void drawArraysInstanced(Primitive mode, std::int32_t first,
                         std::int32_t count, std::int32_t instanceCount) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(mode), first, count,
                                  instanceCount};
    submitOp(RenderOp::DrawArraysInstanced, 0, 0, 0, nullptr, ints);
}

void vertexAttribDivisor(std::uint32_t index, std::uint32_t divisor) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(index),
                                  static_cast<std::int32_t>(divisor), 0, 0};
    submitOp(RenderOp::VertexAttribDivisor, 0, 0, 0, nullptr, ints);
}

void texParameterfv(TextureTarget target, TextureParameter parameter,
                    const float* values) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(target),
                                  static_cast<std::int32_t>(parameter), 0, 0};
    float floats[8] = {};
    if (values) {
        for (int i = 0; i < 4; ++i)
            floats[i] = values[i];
    }
    submitOp(RenderOp::TexParameterfv, 0, 0, 0, floats, ints);
}

void drawBuffer(FramebufferBuffer buffer) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(buffer), 0, 0, 0};
    submitOp(RenderOp::DrawBuffer, 0, 0, 0, nullptr, ints);
}

void readBuffer(FramebufferBuffer buffer) {
    const std::int32_t ints[4] = {static_cast<std::int32_t>(buffer), 0, 0, 0};
    submitOp(RenderOp::ReadBuffer, 0, 0, 0, nullptr, ints);
}

void drawElements(Primitive mode, std::int32_t count, IndexType type,
                  const void* indices) {
    const std::int32_t ints[4] = {
        static_cast<std::int32_t>(mode), static_cast<std::int32_t>(type), count,
        static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(indices))};
    submitOp(RenderOp::DrawElements, 0, 0, 0, nullptr, ints);
}

void uniform1i(Uniform location, std::int32_t value) {
    if (gRender && gRender->uniform1i)
        gRender->uniform1i(gHostData, location, value);
}
void uniform1f(Uniform location, float value) {
    if (gRender && gRender->uniform1f)
        gRender->uniform1f(gHostData, location, value);
}
void uniform2f(Uniform location, float x, float y) {
    if (gRender && gRender->uniform2f)
        gRender->uniform2f(gHostData, location, x, y);
}
void uniform3f(Uniform location, float x, float y, float z) {
    if (gRender && gRender->uniform3f)
        gRender->uniform3f(gHostData, location, x, y, z);
}
void uniform4f(Uniform location, float x, float y, float z, float w) {
    if (gRender && gRender->uniform4f)
        gRender->uniform4f(gHostData, location, x, y, z, w);
}
void uniform1fv(Uniform location, std::int32_t count, const float* values) {
    if (gRender && gRender->uniform1fv)
        gRender->uniform1fv(gHostData, location, count, values);
}
void uniform2fv(Uniform location, std::int32_t count, const float* values) {
    if (gRender && gRender->uniform2fv)
        gRender->uniform2fv(gHostData, location, count, values);
}
void uniform3fv(Uniform location, std::int32_t count, const float* values) {
    if (gRender && gRender->uniform3fv)
        gRender->uniform3fv(gHostData, location, count, values);
}
void uniformMatrix4fv(Uniform location, std::int32_t count, const float* values) {
    if (gRender && gRender->uniform_matrix4fv)
        gRender->uniform_matrix4fv(gHostData, location,
                                   static_cast<std::uint32_t>(count), values);
}

Shader createShader(ShaderStage stage) {
    return gRender && gRender->create_shader
               ? gRender->create_shader(gHostData, stage)
               : 0;
}

void shaderSource(Shader shader, const char* source) {
    if (gRender && gRender->shader_source)
        gRender->shader_source(gHostData, shader, source);
}

void compileShader(Shader shader) {
    if (gRender && gRender->compile_shader)
        gRender->compile_shader(gHostData, shader);
}

Program createProgram() {
    return gRender && gRender->create_program ? gRender->create_program(gHostData)
                                              : 0;
}

void attachShader(Program program, Shader shader) {
    if (gRender && gRender->attach_shader)
        gRender->attach_shader(gHostData, program, shader);
}

void linkProgram(Program program) {
    if (gRender && gRender->link_program)
        gRender->link_program(gHostData, program);
}

Uniform getUniformLocation(Program program, const char* name) {
    return gRender && gRender->get_uniform_location
               ? gRender->get_uniform_location(gHostData, program, name)
               : -1;
}

void getUniformiv(Program program, Uniform location, std::int32_t* values,
                  std::int32_t count) {
    if (gRender && gRender->get_uniformiv)
        gRender->get_uniformiv(gHostData, program, location, values, count);
}

void getProgramiv(Program program, ShaderParameter parameter,
                  std::int32_t* value) {
    if (gRender && gRender->get_programiv)
        gRender->get_programiv(gHostData, program, parameter, value);
}

void getShaderiv(Shader shader, ShaderParameter parameter, std::int32_t* value) {
    if (gRender && gRender->get_shaderiv)
        gRender->get_shaderiv(gHostData, shader, parameter, value);
}

void getProgramInfoLog(Program program, std::int32_t capacity,
                       std::int32_t* length, char* log) {
    if (gRender && gRender->get_program_info_log)
        gRender->get_program_info_log(gHostData, program, capacity, length, log);
}

void getShaderInfoLog(Shader shader, std::int32_t capacity,
                      std::int32_t* length, char* log) {
    if (gRender && gRender->get_shader_info_log)
        gRender->get_shader_info_log(gHostData, shader, capacity, length, log);
}

std::uint32_t getError() {
    return gRender && gRender->get_error ? gRender->get_error(gHostData) : 0;
}

void framebufferTexture2D(TextureTarget target, FramebufferAttachment attachment,
                          Texture texture) {
    if (gRender && gRender->framebuffer_texture_2d)
        gRender->framebuffer_texture_2d(gHostData, target, attachment, texture);
}

void bindRenderbuffer(std::uint32_t renderbuffer) {
    if (gRender && gRender->bind_renderbuffer)
        gRender->bind_renderbuffer(gHostData, renderbuffer);
}

void renderbufferStorage(RenderbufferFormat format, std::int32_t width,
                         std::int32_t height) {
    if (gRender && gRender->renderbuffer_storage)
        gRender->renderbuffer_storage(gHostData, format, width, height);
}

void framebufferRenderbuffer(FramebufferAttachment attachment,
                             std::uint32_t renderbuffer) {
    if (gRender && gRender->framebuffer_renderbuffer)
        gRender->framebuffer_renderbuffer(gHostData, attachment, renderbuffer);
}

bool checkFramebufferStatus() {
    return gRender && gRender->check_framebuffer_status
               ? gRender->check_framebuffer_status(gHostData) != 0
               : false;
}

}  // namespace render

}  // namespace mimita::game
