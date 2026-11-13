// 2026-11-13
/* purpose
* Contract for game-owned render submission.
* Game code calls these helpers instead of OpenGL. The resident host owns the
* device and GL state; a game generation only describes what to bind, upload,
* and draw.
*
* Transitional split:
*   - EXE build (`!MIMITA_GAME_DLL`): render-command-legacy.cpp forwards to raw
*     OpenGL so the resident path behaves exactly as before.
*   - game.dll build (`MIMITA_GAME_DLL`): a bridge forwards to CAP_RENDER. That
*     bridge is added only after the host implements the matching capability
*     operations; until then the DLL must not call these helpers.
*
* This header is the authoritative missing-operation list for CAP_RENDER.
* Does NOT own the device, compile shaders resident-side, or change VSync.
*/
#pragma once

#include <cstdint>

namespace mimita::game::render {

// ---- bind / state ----
void bindVertexArray(std::uint32_t vertexArray);
void bindBuffer(std::uint32_t target, std::uint32_t buffer);
void bindTexture(std::uint32_t target, std::uint32_t texture);
void useProgram(std::uint32_t program);
void bindFramebuffer(std::uint32_t target, std::uint32_t framebuffer);
void activeTexture(std::uint32_t unit);

void enable(std::uint32_t capability);
void disable(std::uint32_t capability);
void depthMask(bool enabled);
void depthFunc(std::uint32_t func);
void cullFace(std::uint32_t mode);
void blendFunc(std::uint32_t source, std::uint32_t destination);
void blendFuncSeparate(std::uint32_t sourceRgb, std::uint32_t destinationRgb,
                       std::uint32_t sourceAlpha, std::uint32_t destinationAlpha);
void pixelStorei(std::uint32_t pname, std::int32_t value);
void polygonMode(std::uint32_t face, std::uint32_t mode);
void scissor(std::int32_t x, std::int32_t y, std::int32_t width,
             std::int32_t height);
void clearColor(float red, float green, float blue, float alpha);
void viewport(std::int32_t x, std::int32_t y, std::int32_t width,
              std::int32_t height);
void clear(std::uint32_t mask);

// ---- upload / attributes ----
// bufferData/bufferSubData bind the buffer first, matching the host capability
// and OpenGL's bound-buffer semantics.
void bufferData(std::uint32_t target, std::uint32_t buffer, const void* data,
                std::uint32_t size, std::uint32_t usage);
void bufferSubData(std::uint32_t target, std::uint32_t buffer,
                   std::uint32_t offset, const void* data, std::uint32_t size);
void texImage2D(std::uint32_t target, std::int32_t level,
                std::int32_t internalFormat, std::int32_t width,
                std::int32_t height, std::int32_t border, std::uint32_t format,
                std::uint32_t type, const void* data);
void texParameteri(std::uint32_t target, std::uint32_t pname, std::int32_t value);
void vertexAttribPointer(std::uint32_t index, std::int32_t size,
                         std::uint32_t type, bool normalized, std::int32_t stride,
                         std::uint32_t offset);
void enableVertexAttribArray(std::uint32_t index);
void disableVertexAttribArray(std::uint32_t index);

// ---- draw ----
void drawArrays(std::uint32_t mode, std::int32_t first, std::int32_t count);
// The element buffer is expected to be bound with bindBuffer(GL_ELEMENT_ARRAY_BUFFER, ...).
void drawElements(std::uint32_t mode, std::int32_t count, std::uint32_t type,
                  const void* indices);

// ---- uniforms ----
void uniform1i(std::int32_t location, std::int32_t value);
void uniform1f(std::int32_t location, float value);
void uniform2f(std::int32_t location, float x, float y);
void uniform3f(std::int32_t location, float x, float y, float z);
void uniform4f(std::int32_t location, float x, float y, float z, float w);
void uniform1fv(std::int32_t location, std::int32_t count, const float* values);
void uniform2fv(std::int32_t location, std::int32_t count, const float* values);
void uniform3fv(std::int32_t location, std::int32_t count, const float* values);
// Matrices are always uploaded without transpose, matching CAP_RENDER.
void uniformMatrix4fv(std::int32_t location, std::int32_t count,
                      const float* values);

// ---- resident shader service ----
// The resident renderer owns shader compilation/linking and the uniform-location
// cache. Game code names the program and asks for locations by name.
std::uint32_t createShader(std::uint32_t type);
void shaderSource(std::uint32_t shader, const char* source);
void compileShader(std::uint32_t shader);
std::uint32_t createProgram();
void attachShader(std::uint32_t program, std::uint32_t shader);
void linkProgram(std::uint32_t program);
std::int32_t getUniformLocation(std::uint32_t program, const char* name);
void getProgramiv(std::uint32_t program, std::uint32_t pname, std::int32_t* value);
void getShaderiv(std::uint32_t shader, std::uint32_t pname, std::int32_t* value);
void getProgramInfoLog(std::uint32_t program, std::int32_t capacity,
                       std::int32_t* length, char* log);
void getShaderInfoLog(std::uint32_t shader, std::int32_t capacity,
                      std::int32_t* length, char* log);
std::uint32_t getError();

// ---- queries ----
void getIntegerv(std::uint32_t pname, std::int32_t* values, std::int32_t count);
bool isEnabled(std::uint32_t capability);
bool getBoolean(std::uint32_t pname);

}  // namespace mimita::game::render
