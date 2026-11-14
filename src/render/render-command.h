// 2026-11-13
/* purpose
* Contract for game-owned render submission.
* Game code calls these helpers instead of OpenGL. The resident host owns the
* device and backend state; a game generation only describes what to bind,
* upload, draw, and which shader program it owns.
*
* Transitional split:
*   - EXE build (`!MIMITA_GAME_DLL`): render-command-legacy.cpp maps the
*     semantic enums to raw OpenGL so the resident path behaves as before.
*   - game.dll build (`MIMITA_GAME_DLL`): a bridge forwards to CAP_RENDER. That
*     bridge is added only after the host implements the matching capability
*     operations; until then the DLL must not call these helpers.
*
* Shaders are hot: the game owns program handles, sources, reload policy, and
* the uniform-location cache. The host only provides the primitive operations.
* This header is the authoritative missing-operation list for CAP_RENDER.
* Does NOT own the device, own a shader registry, or change VSync.
*/
#pragma once

#include <cstdint>

#include "render/render-types.h"

namespace mimita::game::render {

// ---- bind / state ----
void bindVertexArray(VertexArray vertexArray);
void bindBuffer(BufferTarget target, Buffer buffer);
void bindTexture(TextureTarget target, Texture texture);
void useProgram(Program program);
void bindFramebuffer(FramebufferTarget target, Framebuffer framebuffer);
void activeTexture(std::uint32_t unit);

void enable(Capability capability);
void disable(Capability capability);
void depthMask(bool enabled);
void depthFunc(DepthFunc func);
void cullFace(CullFaceMode mode);
void blendFunc(BlendFactor source, BlendFactor destination);
void blendFuncSeparate(BlendFactor sourceRgb, BlendFactor destinationRgb,
                       BlendFactor sourceAlpha, BlendFactor destinationAlpha);
void polygonMode(PolygonMode mode);
void scissor(std::int32_t x, std::int32_t y, std::int32_t width,
             std::int32_t height);
void lineWidth(float width);
void clearColor(float red, float green, float blue, float alpha);
void viewport(std::int32_t x, std::int32_t y, std::int32_t width,
              std::int32_t height);
void clear(bool color, bool depth, bool stencil);

// Host-owned state snapshot. Replaces the game's manual GL query/save/restore.
void pushState();
void popState();

// ---- upload / attributes ----
// bufferData/bufferSubData bind the buffer first, matching the host capability
// and OpenGL's bound-buffer semantics.
void bufferData(BufferTarget target, Buffer buffer, const void* data,
                std::uint32_t size, BufferUsage usage);
void bufferSubData(BufferTarget target, Buffer buffer, std::uint32_t offset,
                   const void* data, std::uint32_t size);
void texImage2D(TextureTarget target, std::int32_t level, TextureFormat format,
                std::int32_t width, std::int32_t height, std::int32_t border,
                PixelFormat pixelFormat, PixelType pixelType, const void* data);
void texParameteri(TextureTarget target, TextureParameter parameter,
                   TextureValue value);
void texParameteri(TextureTarget target, TextureParameter parameter,
                   std::int32_t value);
void texParameterfv(TextureTarget target, TextureParameter parameter,
                    const float* values);
void pixelStorei(PixelStoreParameter parameter, std::int32_t value);
void generateMipmap(TextureTarget target);
void vertexAttribPointer(std::uint32_t index, std::int32_t size,
                         VertexType type, bool normalized, std::int32_t stride,
                         std::uint32_t offset);
void enableVertexAttribArray(std::uint32_t index);
void disableVertexAttribArray(std::uint32_t index);

// ---- draw ----
void drawArrays(Primitive mode, std::int32_t first, std::int32_t count);
void drawArraysInstanced(Primitive mode, std::int32_t first, std::int32_t count,
                         std::int32_t instanceCount);
void vertexAttribDivisor(std::uint32_t index, std::uint32_t divisor);
// The element buffer is expected to be bound with bindBuffer(ElementArray, ...).
void drawElements(Primitive mode, std::int32_t count, IndexType type,
                  const void* indices);

// ---- uniforms (by cached location) ----
void uniform1i(Uniform location, std::int32_t value);
void uniform1f(Uniform location, float value);
void uniform2f(Uniform location, float x, float y);
void uniform3f(Uniform location, float x, float y, float z);
void uniform4f(Uniform location, float x, float y, float z, float w);
void uniform1fv(Uniform location, std::int32_t count, const float* values);
void uniform2fv(Uniform location, std::int32_t count, const float* values);
void uniform3fv(Uniform location, std::int32_t count, const float* values);
// Matrices are always uploaded without transpose, matching CAP_RENDER.
void uniformMatrix4fv(Uniform location, std::int32_t count, const float* values);

// ---- primitive shader service (game owns the program) ----
Shader createShader(ShaderStage stage);
void shaderSource(Shader shader, const char* source);
void compileShader(Shader shader);
Program createProgram();
void attachShader(Program program, Shader shader);
void linkProgram(Program program);
Uniform getUniformLocation(Program program, const char* name);
void getUniformiv(Program program, Uniform location, std::int32_t* values,
                  std::int32_t count);
void getProgramiv(Program program, ShaderParameter parameter,
                  std::int32_t* value);
void getShaderiv(Shader shader, ShaderParameter parameter, std::int32_t* value);
void getProgramInfoLog(Program program, std::int32_t capacity,
                       std::int32_t* length, char* log);
void getShaderInfoLog(Shader shader, std::int32_t capacity,
                      std::int32_t* length, char* log);
std::uint32_t getError();

// ---- render targets ----
void framebufferTexture2D(TextureTarget target, FramebufferAttachment attachment,
                          Texture texture);
void bindRenderbuffer(std::uint32_t renderbuffer);
void renderbufferStorage(RenderbufferFormat format, std::int32_t width,
                         std::int32_t height);
void framebufferRenderbuffer(FramebufferAttachment attachment,
                             std::uint32_t renderbuffer);
void drawBuffer(FramebufferBuffer buffer);
void readBuffer(FramebufferBuffer buffer);
bool checkFramebufferStatus();

}  // namespace mimita::game::render
