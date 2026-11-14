// 2026-11-13
/* purpose
* Backend-neutral render vocabulary shared by the resident host and the game
* generation. These are the values that cross CAP_RENDER; neither side may
* depend on OpenGL constants.
* Does NOT define device handles, command layout, or backend behavior.
*/
#pragma once

#include <cstdint>

namespace mimita::render {

enum class Primitive : std::uint32_t {
    Points = 1,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip,
    TriangleFan,
};

enum class TextureTarget : std::uint32_t {
    Texture2D = 1,
    TextureCube,
    CubePositiveX,
    CubeNegativeX,
    CubePositiveY,
    CubeNegativeY,
    CubePositiveZ,
    CubeNegativeZ,
};

enum class BufferTarget : std::uint32_t {
    Array = 1,
    ElementArray,
};

enum class BufferUsage : std::uint32_t {
    Static = 1,
    Dynamic,
    Stream,
};

enum class Capability : std::uint32_t {
    DepthTest = 1,
    Blend,
    CullFace,
    ScissorTest,
    StencilTest,
    PolygonOffsetFill,
    Multisample,
};

enum class BlendFactor : std::uint32_t {
    Zero = 1,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
};

enum class DepthFunc : std::uint32_t {
    Never = 1,
    Less,
    Equal,
    Lequal,
    Greater,
    Notequal,
    Gequal,
    Always,
};

enum class CullFaceMode : std::uint32_t {
    Front = 1,
    Back,
    FrontAndBack,
};

enum class PolygonMode : std::uint32_t {
    Point = 1,
    Line,
    Fill,
};

enum class IndexType : std::uint32_t {
    UInt16 = 1,
    UInt32,
};

enum class TextureFormat : std::uint32_t {
    RGBA = 1,
    RGB,
    Depth,
    Depth24,
};

enum class PixelFormat : std::uint32_t {
    RGBA = 1,
    RGB,
    Red,
    Depth,
};

enum class PixelType : std::uint32_t {
    UnsignedByte = 1,
    Float,
};

enum class TextureParameter : std::uint32_t {
    MinFilter = 1,
    MagFilter,
    WrapS,
    WrapT,
    WrapR,
    MaxLevel,
    BorderColor,
};

enum class TextureValue : std::uint32_t {
    Nearest = 1,
    Linear,
    LinearMipmapLinear,
    ClampToEdge,
    ClampToBorder,
    Repeat,
    MirroredRepeat,
};

enum class VertexType : std::uint32_t {
    Float = 1,
    UInt32,
};

enum class ShaderStage : std::uint32_t {
    Vertex = 1,
    Fragment,
};

enum class ShaderParameter : std::uint32_t {
    CompileStatus = 1,
    LinkStatus,
    InfoLogLength,
};

enum class FramebufferTarget : std::uint32_t {
    Framebuffer = 1,
    Read,
    Draw,
};

enum class FramebufferAttachment : std::uint32_t {
    Color0 = 1,
    Color1,
    Color2,
    Color3,
    Depth,
    Stencil,
    DepthStencil,
};

enum class RenderbufferFormat : std::uint32_t {
    Depth24 = 1,
    Depth24Stencil8,
    RGBA8,
};

enum class FramebufferBuffer : std::uint32_t {
    None = 1,
    Color0,
    Color1,
    Color2,
    Color3,
};

}  // namespace mimita::render
