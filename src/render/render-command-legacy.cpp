// 2026-11-13
// EXE-side implementation of the render-command contract.
//
// The resident EXE still owns the real GL context, so this transitional file
// maps the backend-neutral enums to OpenGL. The game.dll build excludes this
// file and uses the CAP_RENDER bridge instead, once the host implements those
// operations.

#include "render/render-command.h"

#ifndef MIMITA_GAME_DLL

#include <glad/glad.h>

#include <vector>

namespace mimita::game::render {
namespace {

GLenum toGl(Primitive value) {
    switch (value) {
        case Primitive::Points: return GL_POINTS;
        case Primitive::Lines: return GL_LINES;
        case Primitive::LineStrip: return GL_LINE_STRIP;
        case Primitive::Triangles: return GL_TRIANGLES;
        case Primitive::TriangleStrip: return GL_TRIANGLE_STRIP;
        case Primitive::TriangleFan: return GL_TRIANGLE_FAN;
    }
    return GL_TRIANGLES;
}

GLenum toGl(TextureTarget value) {
    switch (value) {
        case TextureTarget::Texture2D: return GL_TEXTURE_2D;
        case TextureTarget::TextureCube: return GL_TEXTURE_CUBE_MAP;
        case TextureTarget::CubePositiveX: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        case TextureTarget::CubeNegativeX: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        case TextureTarget::CubePositiveY: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        case TextureTarget::CubeNegativeY: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        case TextureTarget::CubePositiveZ: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        case TextureTarget::CubeNegativeZ: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    }
    return GL_TEXTURE_2D;
}

GLenum toGl(BufferTarget value) {
    switch (value) {
        case BufferTarget::Array: return GL_ARRAY_BUFFER;
        case BufferTarget::ElementArray: return GL_ELEMENT_ARRAY_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}

GLenum toGl(BufferUsage value) {
    switch (value) {
        case BufferUsage::Static: return GL_STATIC_DRAW;
        case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
        case BufferUsage::Stream: return GL_STREAM_DRAW;
    }
    return GL_STATIC_DRAW;
}

GLenum toGl(Capability value) {
    switch (value) {
        case Capability::DepthTest: return GL_DEPTH_TEST;
        case Capability::Blend: return GL_BLEND;
        case Capability::CullFace: return GL_CULL_FACE;
        case Capability::ScissorTest: return GL_SCISSOR_TEST;
        case Capability::StencilTest: return GL_STENCIL_TEST;
        case Capability::PolygonOffsetFill: return GL_POLYGON_OFFSET_FILL;
        case Capability::Multisample: return GL_MULTISAMPLE;
    }
    return GL_DEPTH_TEST;
}

GLenum toGl(BlendFactor value) {
    switch (value) {
        case BlendFactor::Zero: return GL_ZERO;
        case BlendFactor::One: return GL_ONE;
        case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha: return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::SrcColor: return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor: return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
    }
    return GL_ONE;
}

GLenum toGl(DepthFunc value) {
    switch (value) {
        case DepthFunc::Never: return GL_NEVER;
        case DepthFunc::Less: return GL_LESS;
        case DepthFunc::Equal: return GL_EQUAL;
        case DepthFunc::Lequal: return GL_LEQUAL;
        case DepthFunc::Greater: return GL_GREATER;
        case DepthFunc::Notequal: return GL_NOTEQUAL;
        case DepthFunc::Gequal: return GL_GEQUAL;
        case DepthFunc::Always: return GL_ALWAYS;
    }
    return GL_LESS;
}

GLenum toGl(CullFaceMode value) {
    switch (value) {
        case CullFaceMode::Front: return GL_FRONT;
        case CullFaceMode::Back: return GL_BACK;
        case CullFaceMode::FrontAndBack: return GL_FRONT_AND_BACK;
    }
    return GL_BACK;
}

GLenum toGl(PolygonMode value) {
    switch (value) {
        case PolygonMode::Point: return GL_POINT;
        case PolygonMode::Line: return GL_LINE;
        case PolygonMode::Fill: return GL_FILL;
    }
    return GL_FILL;
}

GLenum toGl(IndexType value) {
    switch (value) {
        case IndexType::UInt16: return GL_UNSIGNED_SHORT;
        case IndexType::UInt32: return GL_UNSIGNED_INT;
    }
    return GL_UNSIGNED_INT;
}

GLenum toGl(TextureFormat value) {
    switch (value) {
        case TextureFormat::RGBA: return GL_RGBA;
        case TextureFormat::RGB: return GL_RGB;
        case TextureFormat::RGBA8: return GL_RGBA8;
        case TextureFormat::Depth: return GL_DEPTH_COMPONENT;
        case TextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
    }
    return GL_RGBA;
}

GLenum toGl(PixelFormat value) {
    switch (value) {
        case PixelFormat::RGBA: return GL_RGBA;
        case PixelFormat::RGB: return GL_RGB;
        case PixelFormat::RG: return GL_RG;
        case PixelFormat::Red: return GL_RED;
        case PixelFormat::Depth: return GL_DEPTH_COMPONENT;
    }
    return GL_RGBA;
}

GLenum toGl(PixelType value) {
    switch (value) {
        case PixelType::UnsignedByte: return GL_UNSIGNED_BYTE;
        case PixelType::Float: return GL_FLOAT;
    }
    return GL_UNSIGNED_BYTE;
}

GLenum toGl(TextureParameter value) {
    switch (value) {
        case TextureParameter::MinFilter: return GL_TEXTURE_MIN_FILTER;
        case TextureParameter::MagFilter: return GL_TEXTURE_MAG_FILTER;
        case TextureParameter::WrapS: return GL_TEXTURE_WRAP_S;
        case TextureParameter::WrapT: return GL_TEXTURE_WRAP_T;
        case TextureParameter::WrapR: return GL_TEXTURE_WRAP_R;
        case TextureParameter::MaxLevel: return GL_TEXTURE_MAX_LEVEL;
        case TextureParameter::BorderColor: return GL_TEXTURE_BORDER_COLOR;
    }
    return GL_TEXTURE_MIN_FILTER;
}

GLenum toGl(TextureValue value) {
    switch (value) {
        case TextureValue::Nearest: return GL_NEAREST;
        case TextureValue::Linear: return GL_LINEAR;
        case TextureValue::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
        case TextureValue::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case TextureValue::ClampToBorder: return GL_CLAMP_TO_BORDER;
        case TextureValue::Repeat: return GL_REPEAT;
        case TextureValue::MirroredRepeat: return GL_MIRRORED_REPEAT;
    }
    return GL_LINEAR;
}

GLenum toGl(VertexType value) {
    switch (value) {
        case VertexType::Float: return GL_FLOAT;
        case VertexType::UInt32: return GL_UNSIGNED_INT;
    }
    return GL_FLOAT;
}

GLenum toGl(ShaderStage value) {
    switch (value) {
        case ShaderStage::Vertex: return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
    }
    return GL_VERTEX_SHADER;
}

GLenum toGl(ShaderParameter value) {
    switch (value) {
        case ShaderParameter::CompileStatus: return GL_COMPILE_STATUS;
        case ShaderParameter::LinkStatus: return GL_LINK_STATUS;
        case ShaderParameter::InfoLogLength: return GL_INFO_LOG_LENGTH;
    }
    return GL_COMPILE_STATUS;
}

GLenum toGl(FramebufferTarget value) {
    switch (value) {
        case FramebufferTarget::Framebuffer: return GL_FRAMEBUFFER;
        case FramebufferTarget::Read: return GL_READ_FRAMEBUFFER;
        case FramebufferTarget::Draw: return GL_DRAW_FRAMEBUFFER;
    }
    return GL_FRAMEBUFFER;
}

GLenum toGl(FramebufferAttachment value) {
    switch (value) {
        case FramebufferAttachment::Color0: return GL_COLOR_ATTACHMENT0;
        case FramebufferAttachment::Color1: return GL_COLOR_ATTACHMENT1;
        case FramebufferAttachment::Color2: return GL_COLOR_ATTACHMENT2;
        case FramebufferAttachment::Color3: return GL_COLOR_ATTACHMENT3;
        case FramebufferAttachment::Depth: return GL_DEPTH_ATTACHMENT;
        case FramebufferAttachment::Stencil: return GL_STENCIL_ATTACHMENT;
        case FramebufferAttachment::DepthStencil: return GL_DEPTH_STENCIL_ATTACHMENT;
    }
    return GL_COLOR_ATTACHMENT0;
}

GLenum toGl(RenderbufferFormat value) {
    switch (value) {
        case RenderbufferFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case RenderbufferFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
        case RenderbufferFormat::RGBA8: return GL_RGBA8;
    }
    return GL_DEPTH_COMPONENT24;
}

GLenum toGl(FramebufferBuffer value) {
    switch (value) {
        case FramebufferBuffer::None: return GL_NONE;
        case FramebufferBuffer::Color0: return GL_COLOR_ATTACHMENT0;
        case FramebufferBuffer::Color1: return GL_COLOR_ATTACHMENT1;
        case FramebufferBuffer::Color2: return GL_COLOR_ATTACHMENT2;
        case FramebufferBuffer::Color3: return GL_COLOR_ATTACHMENT3;
    }
    return GL_NONE;
}

GLenum toGl(PixelStoreParameter value) {
    switch (value) {
        case PixelStoreParameter::UnpackAlignment: return GL_UNPACK_ALIGNMENT;
        case PixelStoreParameter::UnpackRowLength: return GL_UNPACK_ROW_LENGTH;
    }
    return GL_UNPACK_ALIGNMENT;
}

struct GlState {
    GLboolean depthTest = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLboolean cullFace = GL_FALSE;
    GLboolean scissorTest = GL_FALSE;
    GLboolean depthMask = GL_TRUE;
    GLint depthFunc = GL_LESS;
    GLint cullFaceMode = GL_BACK;
    GLint blendSrcRgb = GL_ONE;
    GLint blendDstRgb = GL_ZERO;
    GLint blendSrcAlpha = GL_ONE;
    GLint blendDstAlpha = GL_ZERO;
    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    GLint viewport[4] = {0, 0, 0, 0};
    GLint activeTexture = GL_TEXTURE0;
    GLint program = 0;
    GLint vertexArray = 0;
    GLint arrayBuffer = 0;
    GLint elementBuffer = 0;
    GLint texture2D = 0;
    GLint framebuffer = 0;
};

std::vector<GlState> gStateStack;

GlState captureState() {
    GlState state;
    state.depthTest = glIsEnabled(GL_DEPTH_TEST);
    state.blend = glIsEnabled(GL_BLEND);
    state.cullFace = glIsEnabled(GL_CULL_FACE);
    state.scissorTest = glIsEnabled(GL_SCISSOR_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depthMask);
    glGetIntegerv(GL_DEPTH_FUNC, &state.depthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE, &state.cullFaceMode);
    glGetIntegerv(GL_BLEND_SRC_RGB, &state.blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &state.blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blendDstAlpha);
    glGetIntegerv(GL_POLYGON_MODE, state.polygonMode);
    glGetIntegerv(GL_VIEWPORT, state.viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);
    glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.arrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &state.elementBuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture2D);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state.framebuffer);
    return state;
}

void restoreState(const GlState& state) {
    glUseProgram(static_cast<GLuint>(state.program));
    glBindVertexArray(static_cast<GLuint>(state.vertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state.arrayBuffer));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(state.elementBuffer));
    glActiveTexture(static_cast<GLenum>(state.activeTexture));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture2D));
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(state.framebuffer));
    glDepthMask(state.depthMask);
    glDepthFunc(static_cast<GLenum>(state.depthFunc));
    glCullFace(static_cast<GLenum>(state.cullFaceMode));
    glBlendFuncSeparate(static_cast<GLenum>(state.blendSrcRgb),
                        static_cast<GLenum>(state.blendDstRgb),
                        static_cast<GLenum>(state.blendSrcAlpha),
                        static_cast<GLenum>(state.blendDstAlpha));
    glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(state.polygonMode[0]));
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2],
               state.viewport[3]);
    if (state.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (state.blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (state.cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (state.scissorTest) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

}  // namespace

void bindVertexArray(VertexArray vertexArray) {
    glBindVertexArray(static_cast<GLuint>(vertexArray));
}

void bindBuffer(BufferTarget target, Buffer buffer) {
    glBindBuffer(toGl(target), static_cast<GLuint>(buffer));
}

void bindTexture(TextureTarget target, Texture texture) {
    glBindTexture(toGl(target), static_cast<GLuint>(texture));
}

void useProgram(Program program) {
    glUseProgram(static_cast<GLuint>(program));
}

void bindFramebuffer(FramebufferTarget target, Framebuffer framebuffer) {
    glBindFramebuffer(toGl(target), static_cast<GLuint>(framebuffer));
}

void activeTexture(std::uint32_t unit) {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
}

void enable(Capability capability) {
    glEnable(toGl(capability));
}

void disable(Capability capability) {
    glDisable(toGl(capability));
}

void depthMask(bool enabled) {
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

void depthFunc(DepthFunc func) {
    glDepthFunc(toGl(func));
}

void cullFace(CullFaceMode mode) {
    glCullFace(toGl(mode));
}

void blendFunc(BlendFactor source, BlendFactor destination) {
    glBlendFunc(toGl(source), toGl(destination));
}

void blendFuncSeparate(BlendFactor sourceRgb, BlendFactor destinationRgb,
                       BlendFactor sourceAlpha, BlendFactor destinationAlpha) {
    glBlendFuncSeparate(toGl(sourceRgb), toGl(destinationRgb), toGl(sourceAlpha),
                        toGl(destinationAlpha));
}

void polygonMode(PolygonMode mode) {
    glPolygonMode(GL_FRONT_AND_BACK, toGl(mode));
}

void scissor(std::int32_t x, std::int32_t y, std::int32_t width,
             std::int32_t height) {
    glScissor(static_cast<GLint>(x), static_cast<GLint>(y),
              static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void lineWidth(float width) {
    glLineWidth(width);
}

void clearColor(float red, float green, float blue, float alpha) {
    glClearColor(red, green, blue, alpha);
}

void viewport(std::int32_t x, std::int32_t y, std::int32_t width,
              std::int32_t height) {
    glViewport(static_cast<GLint>(x), static_cast<GLint>(y),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void clear(bool color, bool depth, bool stencil) {
    GLbitfield mask = 0;
    if (color) mask |= GL_COLOR_BUFFER_BIT;
    if (depth) mask |= GL_DEPTH_BUFFER_BIT;
    if (stencil) mask |= GL_STENCIL_BUFFER_BIT;
    glClear(mask);
}

void pushState() {
    gStateStack.push_back(captureState());
}

void popState() {
    if (gStateStack.empty())
        return;
    const GlState state = gStateStack.back();
    gStateStack.pop_back();
    restoreState(state);
}

void bufferData(BufferTarget target, Buffer buffer, const void* data,
                std::uint32_t size, BufferUsage usage) {
    glBindBuffer(toGl(target), static_cast<GLuint>(buffer));
    glBufferData(toGl(target), static_cast<GLsizeiptr>(size), data, toGl(usage));
}

void bufferSubData(BufferTarget target, Buffer buffer, std::uint32_t offset,
                   const void* data, std::uint32_t size) {
    glBindBuffer(toGl(target), static_cast<GLuint>(buffer));
    glBufferSubData(toGl(target), static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size), data);
}

void texImage2D(TextureTarget target, std::int32_t level, TextureFormat format,
                std::int32_t width, std::int32_t height, std::int32_t border,
                PixelFormat pixelFormat, PixelType pixelType, const void* data) {
    glTexImage2D(toGl(target), static_cast<GLint>(level), static_cast<GLint>(toGl(format)),
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 static_cast<GLint>(border), toGl(pixelFormat), toGl(pixelType),
                 data);
}

void texParameteri(TextureTarget target, TextureParameter parameter,
                   TextureValue value) {
    glTexParameteri(toGl(target), toGl(parameter),
                    static_cast<GLint>(toGl(value)));
}

void texParameteri(TextureTarget target, TextureParameter parameter,
                   std::int32_t value) {
    glTexParameteri(toGl(target), toGl(parameter), static_cast<GLint>(value));
}

void texParameterfv(TextureTarget target, TextureParameter parameter,
                    const float* values) {
    glTexParameterfv(toGl(target), toGl(parameter), values);
}

void pixelStorei(PixelStoreParameter parameter, std::int32_t value) {
    glPixelStorei(toGl(parameter), static_cast<GLint>(value));
}

void generateMipmap(TextureTarget target) {
    glGenerateMipmap(toGl(target));
}

void vertexAttribPointer(std::uint32_t index, std::int32_t size,
                         VertexType type, bool normalized, std::int32_t stride,
                         std::uint32_t offset) {
    glVertexAttribPointer(static_cast<GLuint>(index), static_cast<GLint>(size),
                          toGl(type), normalized ? GL_TRUE : GL_FALSE,
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

void drawArrays(Primitive mode, std::int32_t first, std::int32_t count) {
    glDrawArrays(toGl(mode), static_cast<GLint>(first),
                 static_cast<GLsizei>(count));
}

void drawArraysInstanced(Primitive mode, std::int32_t first,
                         std::int32_t count, std::int32_t instanceCount) {
    glDrawArraysInstanced(toGl(mode), static_cast<GLint>(first),
                          static_cast<GLsizei>(count),
                          static_cast<GLsizei>(instanceCount));
}

void vertexAttribDivisor(std::uint32_t index, std::uint32_t divisor) {
    glVertexAttribDivisor(static_cast<GLuint>(index),
                          static_cast<GLuint>(divisor));
}

void drawElements(Primitive mode, std::int32_t count, IndexType type,
                  const void* indices) {
    glDrawElements(toGl(mode), static_cast<GLsizei>(count), toGl(type), indices);
}

void uniform1i(Uniform location, std::int32_t value) {
    glUniform1i(static_cast<GLint>(location), static_cast<GLint>(value));
}

void uniform1f(Uniform location, float value) {
    glUniform1f(static_cast<GLint>(location), value);
}

void uniform2f(Uniform location, float x, float y) {
    glUniform2f(static_cast<GLint>(location), x, y);
}

void uniform3f(Uniform location, float x, float y, float z) {
    glUniform3f(static_cast<GLint>(location), x, y, z);
}

void uniform4f(Uniform location, float x, float y, float z, float w) {
    glUniform4f(static_cast<GLint>(location), x, y, z, w);
}

void uniform1fv(Uniform location, std::int32_t count, const float* values) {
    glUniform1fv(static_cast<GLint>(location), static_cast<GLsizei>(count), values);
}

void uniform2fv(Uniform location, std::int32_t count, const float* values) {
    glUniform2fv(static_cast<GLint>(location), static_cast<GLsizei>(count), values);
}

void uniform3fv(Uniform location, std::int32_t count, const float* values) {
    glUniform3fv(static_cast<GLint>(location), static_cast<GLsizei>(count), values);
}

void uniformMatrix4fv(Uniform location, std::int32_t count, const float* values) {
    glUniformMatrix4fv(static_cast<GLint>(location), static_cast<GLsizei>(count),
                       GL_FALSE, values);
}

Shader createShader(ShaderStage stage) {
    return static_cast<Shader>(glCreateShader(toGl(stage)));
}

void shaderSource(Shader shader, const char* source) {
    const GLchar* sources[1] = {source};
    glShaderSource(static_cast<GLuint>(shader), 1, sources, nullptr);
}

void compileShader(Shader shader) {
    glCompileShader(static_cast<GLuint>(shader));
}

Program createProgram() {
    return static_cast<Program>(glCreateProgram());
}

void attachShader(Program program, Shader shader) {
    glAttachShader(static_cast<GLuint>(program), static_cast<GLuint>(shader));
}

void linkProgram(Program program) {
    glLinkProgram(static_cast<GLuint>(program));
}

Uniform getUniformLocation(Program program, const char* name) {
    return static_cast<Uniform>(
        glGetUniformLocation(static_cast<GLuint>(program), name));
}

void getUniformiv(Program program, Uniform location, std::int32_t* values,
                  std::int32_t count) {
    (void)count;
    glGetUniformiv(static_cast<GLuint>(program), static_cast<GLint>(location),
                   reinterpret_cast<GLint*>(values));
}

void getProgramiv(Program program, ShaderParameter parameter,
                  std::int32_t* value) {
    glGetProgramiv(static_cast<GLuint>(program), toGl(parameter),
                   reinterpret_cast<GLint*>(value));
}

void getShaderiv(Shader shader, ShaderParameter parameter, std::int32_t* value) {
    glGetShaderiv(static_cast<GLuint>(shader), toGl(parameter),
                  reinterpret_cast<GLint*>(value));
}

void getProgramInfoLog(Program program, std::int32_t capacity,
                       std::int32_t* length, char* log) {
    glGetProgramInfoLog(static_cast<GLuint>(program), static_cast<GLsizei>(capacity),
                        reinterpret_cast<GLsizei*>(length), log);
}

void getShaderInfoLog(Shader shader, std::int32_t capacity,
                      std::int32_t* length, char* log) {
    glGetShaderInfoLog(static_cast<GLuint>(shader), static_cast<GLsizei>(capacity),
                       reinterpret_cast<GLsizei*>(length), log);
}

std::uint32_t getError() {
    return static_cast<std::uint32_t>(glGetError());
}

void framebufferTexture2D(TextureTarget target, FramebufferAttachment attachment,
                          Texture texture) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, toGl(attachment), toGl(target),
                           static_cast<GLuint>(texture), 0);
}

void bindRenderbuffer(std::uint32_t renderbuffer) {
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer));
}

void renderbufferStorage(RenderbufferFormat format, std::int32_t width,
                         std::int32_t height) {
    glRenderbufferStorage(GL_RENDERBUFFER, toGl(format),
                          static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height));
}

void framebufferRenderbuffer(FramebufferAttachment attachment,
                             std::uint32_t renderbuffer) {
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, toGl(attachment), GL_RENDERBUFFER,
                              static_cast<GLuint>(renderbuffer));
}

void drawBuffer(FramebufferBuffer buffer) {
    glDrawBuffer(toGl(buffer));
}

void readBuffer(FramebufferBuffer buffer) {
    glReadBuffer(toGl(buffer));
}

bool checkFramebufferStatus() {
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

}  // namespace mimita::game::render

#endif  // MIMITA_GAME_DLL
