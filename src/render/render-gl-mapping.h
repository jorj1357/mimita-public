// 2026 11 14 1008
/* purpose
* Single mapping from the backend-neutral render vocabulary to OpenGL enums.
* Included by the EXE-only legacy implementation and the resident host thunks;
* the game.dll bridge forwards integers and never includes this header.
* Does NOT define contract operations, state, or backend behavior.
*/
#pragma once

#include <glad/glad.h>

#include "host/abi/render-types.h"

namespace mimita::render {

inline GLenum toGl(Primitive value) {
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

inline GLenum toGl(TextureTarget value) {
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

inline GLenum toGl(BufferTarget value) {
    switch (value) {
        case BufferTarget::Array: return GL_ARRAY_BUFFER;
        case BufferTarget::ElementArray: return GL_ELEMENT_ARRAY_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}

inline GLenum toGl(BufferUsage value) {
    switch (value) {
        case BufferUsage::Static: return GL_STATIC_DRAW;
        case BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
        case BufferUsage::Stream: return GL_STREAM_DRAW;
    }
    return GL_STATIC_DRAW;
}

inline GLenum toGl(Capability value) {
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

inline GLenum toGl(BlendFactor value) {
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

inline GLenum toGl(DepthFunc value) {
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

inline GLenum toGl(CullFaceMode value) {
    switch (value) {
        case CullFaceMode::Front: return GL_FRONT;
        case CullFaceMode::Back: return GL_BACK;
        case CullFaceMode::FrontAndBack: return GL_FRONT_AND_BACK;
    }
    return GL_BACK;
}

inline GLenum toGl(PolygonMode value) {
    switch (value) {
        case PolygonMode::Point: return GL_POINT;
        case PolygonMode::Line: return GL_LINE;
        case PolygonMode::Fill: return GL_FILL;
    }
    return GL_FILL;
}

inline GLenum toGl(IndexType value) {
    switch (value) {
        case IndexType::UInt16: return GL_UNSIGNED_SHORT;
        case IndexType::UInt32: return GL_UNSIGNED_INT;
    }
    return GL_UNSIGNED_INT;
}

inline GLenum toGl(TextureFormat value) {
    switch (value) {
        case TextureFormat::RGBA: return GL_RGBA;
        case TextureFormat::RGB: return GL_RGB;
        case TextureFormat::RGBA8: return GL_RGBA8;
        case TextureFormat::Depth: return GL_DEPTH_COMPONENT;
        case TextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
    }
    return GL_RGBA;
}

inline GLenum toGl(PixelFormat value) {
    switch (value) {
        case PixelFormat::RGBA: return GL_RGBA;
        case PixelFormat::RGB: return GL_RGB;
        case PixelFormat::RG: return GL_RG;
        case PixelFormat::Red: return GL_RED;
        case PixelFormat::Depth: return GL_DEPTH_COMPONENT;
    }
    return GL_RGBA;
}

inline GLenum toGl(PixelType value) {
    switch (value) {
        case PixelType::UnsignedByte: return GL_UNSIGNED_BYTE;
        case PixelType::Float: return GL_FLOAT;
    }
    return GL_UNSIGNED_BYTE;
}

inline GLenum toGl(TextureParameter value) {
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

inline GLenum toGl(TextureValue value) {
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

inline GLenum toGl(VertexType value) {
    switch (value) {
        case VertexType::Float: return GL_FLOAT;
        case VertexType::UInt32: return GL_UNSIGNED_INT;
    }
    return GL_FLOAT;
}

inline GLenum toGl(ShaderStage value) {
    switch (value) {
        case ShaderStage::Vertex: return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
    }
    return GL_VERTEX_SHADER;
}

inline GLenum toGl(ShaderParameter value) {
    switch (value) {
        case ShaderParameter::CompileStatus: return GL_COMPILE_STATUS;
        case ShaderParameter::LinkStatus: return GL_LINK_STATUS;
        case ShaderParameter::InfoLogLength: return GL_INFO_LOG_LENGTH;
    }
    return GL_COMPILE_STATUS;
}

inline GLenum toGl(FramebufferTarget value) {
    switch (value) {
        case FramebufferTarget::Framebuffer: return GL_FRAMEBUFFER;
        case FramebufferTarget::Read: return GL_READ_FRAMEBUFFER;
        case FramebufferTarget::Draw: return GL_DRAW_FRAMEBUFFER;
    }
    return GL_FRAMEBUFFER;
}

inline GLenum toGl(FramebufferAttachment value) {
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

inline GLenum toGl(RenderbufferFormat value) {
    switch (value) {
        case RenderbufferFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case RenderbufferFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
        case RenderbufferFormat::RGBA8: return GL_RGBA8;
    }
    return GL_DEPTH_COMPONENT24;
}

inline GLenum toGl(FramebufferBuffer value) {
    switch (value) {
        case FramebufferBuffer::None: return GL_NONE;
        case FramebufferBuffer::Color0: return GL_COLOR_ATTACHMENT0;
        case FramebufferBuffer::Color1: return GL_COLOR_ATTACHMENT1;
        case FramebufferBuffer::Color2: return GL_COLOR_ATTACHMENT2;
        case FramebufferBuffer::Color3: return GL_COLOR_ATTACHMENT3;
    }
    return GL_NONE;
}

inline GLenum toGl(PixelStoreParameter value) {
    switch (value) {
        case PixelStoreParameter::UnpackAlignment: return GL_UNPACK_ALIGNMENT;
        case PixelStoreParameter::UnpackRowLength: return GL_UNPACK_ROW_LENGTH;
    }
    return GL_UNPACK_ALIGNMENT;
}

inline GLbitfield toGl(BlitMask value) {
    switch (value) {
        case BlitMask::Color: return GL_COLOR_BUFFER_BIT;
        case BlitMask::Depth: return GL_DEPTH_BUFFER_BIT;
        case BlitMask::Stencil: return GL_STENCIL_BUFFER_BIT;
        case BlitMask::ColorDepth: return GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        case BlitMask::ColorStencil: return GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        case BlitMask::DepthStencil: return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        case BlitMask::All:
            return GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                   GL_STENCIL_BUFFER_BIT;
    }
    return GL_COLOR_BUFFER_BIT;
}

inline GLenum toGl(BlitFilter value) {
    switch (value) {
        case BlitFilter::Nearest: return GL_NEAREST;
        case BlitFilter::Linear: return GL_LINEAR;
    }
    return GL_NEAREST;
}

// Freeze the ABI values so a stale game.dll cannot silently disagree with the
// resident host about the integer that crosses CAP_RENDER.
static_assert(static_cast<std::uint32_t>(Primitive::Points) == 1, "ABI drift");
static_assert(static_cast<std::uint32_t>(TextureTarget::Texture2D) == 1, "ABI drift");
static_assert(static_cast<std::uint32_t>(TextureFormat::RGBA8) == 3, "ABI drift");
static_assert(static_cast<std::uint32_t>(PixelFormat::Red) == 4, "ABI drift");
static_assert(static_cast<std::uint32_t>(TextureValue::ClampToBorder) == 5, "ABI drift");
static_assert(static_cast<std::uint32_t>(FramebufferAttachment::Depth) == 5, "ABI drift");
static_assert(static_cast<std::uint32_t>(RenderbufferFormat::RGBA8) == 3, "ABI drift");
static_assert(static_cast<std::uint32_t>(PixelStoreParameter::UnpackRowLength) == 2, "ABI drift");

}  // namespace mimita::render
