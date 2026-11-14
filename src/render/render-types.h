// 2026-11-13
/* purpose
* Game-side names for the shared, backend-neutral render vocabulary.
* The authoritative enum values live in host/abi/render-types.h so the resident
* host and the game generation cannot drift.
* Does NOT expose GL constants, device handles, or backend-specific values.
*/
#pragma once

#include <cstdint>

#include "host/abi/render-types.h"

namespace mimita::game::render {

using mimita::render::Primitive;
using mimita::render::TextureTarget;
using mimita::render::BufferTarget;
using mimita::render::BufferUsage;
using mimita::render::Capability;
using mimita::render::BlendFactor;
using mimita::render::DepthFunc;
using mimita::render::CullFaceMode;
using mimita::render::PolygonMode;
using mimita::render::IndexType;
using mimita::render::TextureFormat;
using mimita::render::PixelFormat;
using mimita::render::PixelType;
using mimita::render::TextureParameter;
using mimita::render::TextureValue;
using mimita::render::VertexType;
using mimita::render::ShaderStage;
using mimita::render::ShaderParameter;
using mimita::render::FramebufferTarget;
using mimita::render::FramebufferAttachment;
using mimita::render::RenderbufferFormat;
using mimita::render::FramebufferBuffer;

using Program = std::uint32_t;
using Uniform = std::int32_t;
using RenderTarget = std::uint32_t;
using Shader = std::uint32_t;
using Texture = std::uint32_t;
using Buffer = std::uint32_t;
using VertexArray = std::uint32_t;
using Framebuffer = std::uint32_t;

// Cube face i (0..5) as a texImage2D target.
inline TextureTarget cubeFace(int index) {
    return static_cast<TextureTarget>(
        static_cast<std::uint32_t>(TextureTarget::CubePositiveX) +
        static_cast<std::uint32_t>(index < 0 ? 0 : (index > 5 ? 5 : index)));
}

}  // namespace mimita::game::render
