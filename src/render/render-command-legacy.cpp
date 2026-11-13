// 2026-11-13
// EXE-side implementation of the render-command contract.
//
// The resident EXE still owns the real GL context, so this transitional file
// forwards every helper to OpenGL. The game.dll build excludes this file and
// uses the CAP_RENDER bridge instead, once the host implements those ops.

#include "render/render-command.h"

#ifndef MIMITA_GAME_DLL

#include <glad/glad.h>

namespace mimita::game::render {

void bindVertexArray(std::uint32_t vertexArray) {
    glBindVertexArray(static_cast<GLuint>(vertexArray));
}

void bindBuffer(std::uint32_t target, std::uint32_t buffer) {
    glBindBuffer(static_cast<GLenum>(target), static_cast<GLuint>(buffer));
}

void bindTexture(std::uint32_t target, std::uint32_t texture) {
    glBindTexture(static_cast<GLenum>(target), static_cast<GLuint>(texture));
}

void useProgram(std::uint32_t program) {
    glUseProgram(static_cast<GLuint>(program));
}

void bindFramebuffer(std::uint32_t target, std::uint32_t framebuffer) {
    glBindFramebuffer(static_cast<GLenum>(target), static_cast<GLuint>(framebuffer));
}

void activeTexture(std::uint32_t unit) {
    glActiveTexture(static_cast<GLenum>(unit));
}

void enable(std::uint32_t capability) {
    glEnable(static_cast<GLenum>(capability));
}

void disable(std::uint32_t capability) {
    glDisable(static_cast<GLenum>(capability));
}

void depthMask(bool enabled) {
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void depthFunc(std::uint32_t func) {
    glDepthFunc(static_cast<GLenum>(func));
}

void cullFace(std::uint32_t mode) {
    glCullFace(static_cast<GLenum>(mode));
}

void blendFunc(std::uint32_t source, std::uint32_t destination) {
    glBlendFunc(static_cast<GLenum>(source), static_cast<GLenum>(destination));
}

void blendFuncSeparate(std::uint32_t sourceRgb, std::uint32_t destinationRgb,
                       std::uint32_t sourceAlpha, std::uint32_t destinationAlpha) {
    glBlendFuncSeparate(static_cast<GLenum>(sourceRgb),
                        static_cast<GLenum>(destinationRgb),
                        static_cast<GLenum>(sourceAlpha),
                        static_cast<GLenum>(destinationAlpha));
}

void pixelStorei(std::uint32_t pname, std::int32_t value) {
    glPixelStorei(static_cast<GLenum>(pname), static_cast<GLint>(value));
}

void polygonMode(std::uint32_t face, std::uint32_t mode) {
    glPolygonMode(static_cast<GLenum>(face), static_cast<GLenum>(mode));
}

void scissor(std::int32_t x, std::int32_t y, std::int32_t width,
             std::int32_t height) {
    glScissor(static_cast<GLint>(x), static_cast<GLint>(y),
              static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void clearColor(float red, float green, float blue, float alpha) {
    glClearColor(red, green, blue, alpha);
}

void viewport(std::int32_t x, std::int32_t y, std::int32_t width,
              std::int32_t height) {
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void clear(std::uint32_t mask) {
    glClear(static_cast<GLbitfield>(mask));
}

void bufferData(std::uint32_t target, std::uint32_t buffer, const void* data,
                std::uint32_t size, std::uint32_t usage) {
    glBindBuffer(static_cast<GLenum>(target), static_cast<GLuint>(buffer));
    glBufferData(static_cast<GLenum>(target), static_cast<GLsizeiptr>(size), data,
                 static_cast<GLenum>(usage));
}

void bufferSubData(std::uint32_t target, std::uint32_t buffer,
                   std::uint32_t offset, const void* data, std::uint32_t size) {
    glBindBuffer(static_cast<GLenum>(target), static_cast<GLuint>(buffer));
    glBufferSubData(static_cast<GLenum>(target), static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size), data);
}

void texImage2D(std::uint32_t target, std::int32_t level,
                std::int32_t internalFormat, std::int32_t width,
                std::int32_t height, std::int32_t border, std::uint32_t format,
                std::uint32_t type, const void* data) {
    glTexImage2D(static_cast<GLenum>(target), static_cast<GLint>(level),
                 static_cast<GLint>(internalFormat), static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), static_cast<GLint>(border),
                 static_cast<GLenum>(format), static_cast<GLenum>(type), data);
}

void texParameteri(std::uint32_t target, std::uint32_t pname, std::int32_t value) {
    glTexParameteri(static_cast<GLenum>(target), static_cast<GLenum>(pname),
                    static_cast<GLint>(value));
}

void vertexAttribPointer(std::uint32_t index, std::int32_t size,
                         std::uint32_t type, bool normalized, std::int32_t stride,
                         std::uint32_t offset) {
    glVertexAttribPointer(static_cast<GLuint>(index), static_cast<GLint>(size),
                          static_cast<GLenum>(type),
                          normalized ? GL_TRUE : GL_FALSE,
                          static_cast<GLsizei>(stride),
                          reinterpret_cast<const void*>(
                              static_cast<std::uintptr_t>(offset)));
}

void enableVertexAttribArray(std::uint32_t index) {
    glEnableVertexAttribArray(static_cast<GLuint>(index));
}

void disableVertexAttribArray(std::uint32_t index) {
    glDisableVertexAttribArray(static_cast<GLuint>(index));
}

void drawArrays(std::uint32_t mode, std::int32_t first, std::int32_t count) {
    glDrawArrays(static_cast<GLenum>(mode), static_cast<GLint>(first),
                 static_cast<GLsizei>(count));
}

void drawElements(std::uint32_t mode, std::int32_t count, std::uint32_t type,
                  const void* indices) {
    glDrawElements(static_cast<GLenum>(mode), static_cast<GLsizei>(count),
                   static_cast<GLenum>(type), indices);
}

void uniform1i(std::int32_t location, std::int32_t value) {
    glUniform1i(static_cast<GLint>(location), static_cast<GLint>(value));
}

void uniform1f(std::int32_t location, float value) {
    glUniform1f(static_cast<GLint>(location), value);
}

void uniform2f(std::int32_t location, float x, float y) {
    glUniform2f(static_cast<GLint>(location), x, y);
}

void uniform3f(std::int32_t location, float x, float y, float z) {
    glUniform3f(static_cast<GLint>(location), x, y, z);
}

void uniform4f(std::int32_t location, float x, float y, float z, float w) {
    glUniform4f(static_cast<GLint>(location), x, y, z, w);
}

void uniform1fv(std::int32_t location, std::int32_t count, const float* values) {
    glUniform1fv(static_cast<GLint>(location), static_cast<GLsizei>(count), values);
}

void uniform2fv(std::int32_t location, std::int32_t count, const float* values) {
    glUniform2fv(static_cast<GLint>(location), static_cast<GLsizei>(count), values);
}

void uniform3fv(std::int32_t location, std::int32_t count, const float* values) {
    glUniform3fv(static_cast<GLint>(location), static_cast<GLsizei>(count), values);
}

void uniformMatrix4fv(std::int32_t location, std::int32_t count,
                      const float* values) {
    glUniformMatrix4fv(static_cast<GLint>(location), static_cast<GLsizei>(count),
                       GL_FALSE, values);
}

std::uint32_t createShader(std::uint32_t type) {
    return static_cast<std::uint32_t>(glCreateShader(static_cast<GLenum>(type)));
}

void shaderSource(std::uint32_t shader, const char* source) {
    const GLchar* sources[1] = {source};
    glShaderSource(static_cast<GLuint>(shader), 1, sources, nullptr);
}

void compileShader(std::uint32_t shader) {
    glCompileShader(static_cast<GLuint>(shader));
}

std::uint32_t createProgram() {
    return static_cast<std::uint32_t>(glCreateProgram());
}

void attachShader(std::uint32_t program, std::uint32_t shader) {
    glAttachShader(static_cast<GLuint>(program), static_cast<GLuint>(shader));
}

void linkProgram(std::uint32_t program) {
    glLinkProgram(static_cast<GLuint>(program));
}

std::int32_t getUniformLocation(std::uint32_t program, const char* name) {
    return static_cast<std::int32_t>(
        glGetUniformLocation(static_cast<GLuint>(program), name));
}

void getProgramiv(std::uint32_t program, std::uint32_t pname, std::int32_t* value) {
    glGetProgramiv(static_cast<GLuint>(program), static_cast<GLenum>(pname),
                   reinterpret_cast<GLint*>(value));
}

void getShaderiv(std::uint32_t shader, std::uint32_t pname, std::int32_t* value) {
    glGetShaderiv(static_cast<GLuint>(shader), static_cast<GLenum>(pname),
                  reinterpret_cast<GLint*>(value));
}

void getProgramInfoLog(std::uint32_t program, std::int32_t capacity,
                       std::int32_t* length, char* log) {
    glGetProgramInfoLog(static_cast<GLuint>(program), static_cast<GLsizei>(capacity),
                        reinterpret_cast<GLsizei*>(length), log);
}

void getShaderInfoLog(std::uint32_t shader, std::int32_t capacity,
                      std::int32_t* length, char* log) {
    glGetShaderInfoLog(static_cast<GLuint>(shader), static_cast<GLsizei>(capacity),
                       reinterpret_cast<GLsizei*>(length), log);
}

std::uint32_t getError() {
    return static_cast<std::uint32_t>(glGetError());
}

void getIntegerv(std::uint32_t pname, std::int32_t* values, std::int32_t count) {
    (void)count;
    glGetIntegerv(static_cast<GLenum>(pname), reinterpret_cast<GLint*>(values));
}

bool isEnabled(std::uint32_t capability) {
    return glIsEnabled(static_cast<GLenum>(capability)) != GL_FALSE;
}

bool getBoolean(std::uint32_t pname) {
    GLboolean value = GL_FALSE;
    glGetBooleanv(static_cast<GLenum>(pname), &value);
    return value != GL_FALSE;
}

}  // namespace mimita::game::render

#endif  // MIMITA_GAME_DLL
