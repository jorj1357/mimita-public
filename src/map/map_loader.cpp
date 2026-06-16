// C:\important\mimita-priv-v8\src\map\map_loader.cpp
//
// GLB loader notes:
// - Blender .glb files can store many meshes, materials, textures, and scene nodes.
// - A mesh primitive references vertex attributes by accessor name: POSITION, NORMAL,
//   TEXCOORD_0, etc.
// - Accessors may be tightly packed or strided through a BufferView. Always honor stride.
// - A material usually points at a texture index, which points at an image. In .glb files
//   that image is often embedded in the binary, so we upload model.images[] directly.
//
// This is intentionally not PBR. The loader only extracts enough data for the engine's
// stylized gameplay shader: position, normal, UV, and one base-color texture.

#include "map_common.h"
#include "map_loader.h"
#include "tiny_obj_loader.h"
#include "tinygltf/tiny_gltf.h"

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <unordered_set>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "utils/path_utils.h"
#include "world/texture-store.h"
#include "debug/debug-log.h"
#include "debug/gl-debug.h"

extern TextureStore gTextures;

GLBDebugData gGLBDebug;

void GLBDebugData::clear()
{
    materials.clear();
    images.clear();
    lights.clear();
    meshCount = 0;
    totalPrimitives = 0;
    loaded = false;
}

namespace {

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#define GLB_LOG(...) Debug::logAuto(Debug::Category::GLB, __VA_ARGS__)

bool glbVerbose()
{
    return DebugConfig::GLB_VERBOSE || std::getenv("MIMITA_GLB_VERBOSE") != nullptr;
}

bool shouldLogPrimitiveDetails(int meshIndex, int primitiveIndex)
{
    return glbVerbose();
}

bool validVec3(glm::vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool validVec2(glm::vec2 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y);
}

const char* accessorTypeName(int type)
{
    switch (type)
    {
        case TINYGLTF_TYPE_SCALAR: return "SCALAR";
        case TINYGLTF_TYPE_VEC2: return "VEC2";
        case TINYGLTF_TYPE_VEC3: return "VEC3";
        case TINYGLTF_TYPE_VEC4: return "VEC4";
        case TINYGLTF_TYPE_MAT4: return "MAT4";
        default: return "UNKNOWN";
    }
}

const char* componentTypeName(int type)
{
    switch (type)
    {
        case TINYGLTF_COMPONENT_TYPE_BYTE: return "BYTE";
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return "UNSIGNED_BYTE";
        case TINYGLTF_COMPONENT_TYPE_SHORT: return "SHORT";
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return "UNSIGNED_SHORT";
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return "UNSIGNED_INT";
        case TINYGLTF_COMPONENT_TYPE_FLOAT: return "FLOAT";
        default: return "UNKNOWN";
    }
}

bool validateAccessor(
    const tinygltf::Model& model,
    int accessorIndex,
    const char* label,
    int meshIndex,
    int primitiveIndex,
    int requiredType,
    int requiredComponentType,
    const tinygltf::Accessor** out
) {
    *out = nullptr;
    if (accessorIndex < 0 || accessorIndex >= (int)model.accessors.size())
    {
        GLB_LOG("[GLB ERROR] mesh=%d prim=%d %s accessor index out of range: %d accessors=%zu\n",
                meshIndex, primitiveIndex, label, accessorIndex, model.accessors.size());
        return false;
    }

    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
        GLB_LOG("[GLB] mesh=%d prim=%d %s accessor=%d count=%zu type=%s component=%s bufferView=%d byteOffset=%zu normalized=%d\n",
                meshIndex, primitiveIndex, label, accessorIndex, accessor.count,
                accessorTypeName(accessor.type), componentTypeName(accessor.componentType),
                accessor.bufferView, accessor.byteOffset, accessor.normalized ? 1 : 0);

    if (requiredType != 0 && accessor.type != requiredType)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d %s unsupported accessor type %s, expected %s\n",
                meshIndex, primitiveIndex, label, accessorTypeName(accessor.type), accessorTypeName(requiredType));
        return false;
    }

    if (requiredComponentType != 0 && accessor.componentType != requiredComponentType)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d %s unsupported component type %s, expected %s\n",
                meshIndex, primitiveIndex, label, componentTypeName(accessor.componentType), componentTypeName(requiredComponentType));
        return false;
    }

    if (accessor.count == 0)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d %s accessor is empty\n", meshIndex, primitiveIndex, label);
        return false;
    }

    if (accessor.bufferView < 0 || accessor.bufferView >= (int)model.bufferViews.size())
    {
        GLB_LOG("[GLB ERROR] mesh=%d prim=%d %s invalid bufferView=%d bufferViews=%zu\n",
                meshIndex, primitiveIndex, label, accessor.bufferView, model.bufferViews.size());
        return false;
    }

    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || view.buffer >= (int)model.buffers.size())
    {
        GLB_LOG("[GLB ERROR] mesh=%d prim=%d %s invalid buffer=%d buffers=%zu\n",
                meshIndex, primitiveIndex, label, view.buffer, model.buffers.size());
        return false;
    }

    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    size_t stride = accessor.ByteStride(view);
    if (stride == 0)
        stride = (size_t)tinygltf::GetComponentSizeInBytes(accessor.componentType) *
                 (size_t)tinygltf::GetNumComponentsInType(accessor.type);
    size_t elemSize = (size_t)tinygltf::GetComponentSizeInBytes(accessor.componentType) *
                      (size_t)tinygltf::GetNumComponentsInType(accessor.type);
    size_t lastOffset = accessor.count > 0 ? accessor.byteOffset + (accessor.count - 1) * stride + elemSize : accessor.byteOffset;

    if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
        GLB_LOG("[GLB] mesh=%d prim=%d %s bufferView=%d byteLength=%zu byteStride=%zu viewOffset=%zu buffer=%d bufferBytes=%zu requiredEnd=%zu\n",
                meshIndex, primitiveIndex, label, accessor.bufferView, view.byteLength, stride,
                view.byteOffset, view.buffer, buffer.data.size(), lastOffset);

    if (stride == 0 || elemSize == 0)
    {
        GLB_LOG("[GLB ERROR] mesh=%d prim=%d %s invalid stride/element size stride=%zu elem=%zu\n",
                meshIndex, primitiveIndex, label, stride, elemSize);
        return false;
    }

    if (lastOffset > view.byteLength)
    {
        GLB_LOG("[GLB ERROR] mesh=%d prim=%d %s accessor exceeds bufferView requiredEnd=%zu viewLength=%zu\n",
                meshIndex, primitiveIndex, label, lastOffset, view.byteLength);
        return false;
    }

    if (view.byteOffset + lastOffset > buffer.data.size())
    {
        GLB_LOG("[GLB ERROR] mesh=%d prim=%d %s accessor exceeds buffer bytes absoluteEnd=%zu bufferBytes=%zu\n",
                meshIndex, primitiveIndex, label, view.byteOffset + lastOffset, buffer.data.size());
        return false;
    }

    *out = &accessor;
    return true;
}

GLuint loadExternalImage(const std::string& uri, const std::string& glbDir)
{
    printf("[TEXTURE SEARCH] trying external URI: %s\n", uri.c_str());

    // Try URI as-is relative to CWD
    std::string tryPath = resolveAssetPath(uri);
    if (!tryPath.empty())
    {
        GLuint tex = gTextures.getPath(tryPath, false);
        if (tex)
        {
            printf("[TEXTURE SEARCH] FOUND %s\n", tryPath.c_str());
            return tex;
        }
    }

    // Try relative to GLB directory
    if (!glbDir.empty())
    {
        std::string relPath = glbDir + "/" + uri;
        printf("[TEXTURE SEARCH] trying %s\n", relPath.c_str());
        GLuint tex = gTextures.getPath(relPath, false);
        if (tex)
        {
            printf("[TEXTURE SEARCH] FOUND %s\n", relPath.c_str());
            return tex;
        }
    }

    // Try assets/textures/<filename>
    size_t slashPos = uri.find_last_of("/\\");
    std::string filename = (slashPos != std::string::npos) ? uri.substr(slashPos + 1) : uri;
    std::string texPath = "assets/textures/" + filename;
    printf("[TEXTURE SEARCH] trying %s\n", texPath.c_str());
    GLuint tex = gTextures.getPath(texPath, false);
    if (tex)
    {
        printf("[TEXTURE SEARCH] FOUND %s\n", texPath.c_str());
        return tex;
    }

    // Try without extension
    size_t dotPos = filename.rfind('.');
    if (dotPos != std::string::npos)
    {
        std::string nameOnly = filename.substr(0, dotPos);
        printf("[TEXTURE SEARCH] trying %s\n", nameOnly.c_str());
        tex = gTextures.get(nameOnly);
        if (tex)
        {
            printf("[TEXTURE SEARCH] FOUND assets/textures/%s.png\n", nameOnly.c_str());
            return tex;
        }
    }

    printf("[TEXTURE SEARCH] NOT FOUND %s\n", uri.c_str());
    return 0;
}

GLuint uploadGLBImage(const tinygltf::Image& image, int imageIndex, const std::string& glbDir)
{
    // Handle non-embedded (external) images
    if (!image.uri.empty() && (image.image.empty() || image.width <= 0 || image.height <= 0))
    {
        printf("[GLB IMAGE] index=%d uri=%s external image; trying file load\n",
               imageIndex, image.uri.c_str());
        return loadExternalImage(image.uri, glbDir);
    }

    if (image.image.empty() || image.width <= 0 || image.height <= 0)
    {
        printf("[GLB TEXTURE WARNING] image %d invalid data size=%zu dims=%dx%d\n",
                imageIndex, image.image.size(), image.width, image.height);
        // Try external URI as fallback
        if (!image.uri.empty())
        {
            printf("[GLB TEXTURE WARNING] image %d has uri=%s; trying external load\n",
                   imageIndex, image.uri.c_str());
            return loadExternalImage(image.uri, glbDir);
        }
        return 0;
    }

    if (image.component < 1 || image.component > 4)
    {
        GLB_LOG("[GLB TEXTURE WARNING] image %d unsupported component count=%d\n",
                imageIndex, image.component);
        return 0;
    }

    const size_t expectedBytes = (size_t)image.width * (size_t)image.height * (size_t)image.component;
    if (expectedBytes == 0 || image.image.size() < expectedBytes)
    {
        GLB_LOG("[GLB TEXTURE WARNING] image %d pixel buffer too small bytes=%zu expected=%zu dims=%dx%d components=%d\n",
                imageIndex, image.image.size(), expectedBytes, image.width, image.height, image.component);
        return 0;
    }

    // Mipmaps are smaller prefiltered versions of the same texture.
    // Without mipmaps, distant surfaces shimmer because a single screen pixel samples
    // a huge number of source texels. Mipmaps trade detail for stable movement clarity.
    // Trilinear filtering blends between mip levels so movement does not pop.
    // Anisotropic filtering helps surfaces viewed at glancing angles, like floors.
    GLuint tex = 0;
    MIMITA_GL_CLEAR_STAGE("uploadGLBImage");
    MIMITA_GL_CALL(glGenTextures(1, &tex));
    if (!tex)
    {
        GLB_LOG("[GLB TEXTURE WARNING] glGenTextures returned 0 for image %d\n", imageIndex);
        return 0;
    }
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, tex));

    GLenum srcFormat = GL_RGBA;
    if (image.component == 1) srcFormat = GL_RED;
    else if (image.component == 2) srcFormat = GL_RG;
    else if (image.component == 3) srcFormat = GL_RGB;
    else if (image.component == 4) srcFormat = GL_RGBA;

    GLB_LOG("[GLB] loaded texture image=%d name=%s size=%dx%d components=%d bytes=%zu tex=%u\n",
           imageIndex, image.name.c_str(), image.width, image.height, image.component, image.image.size(), tex);
    Debug::log(Debug::Category::GLB, "[TEXTURE] Uploading GLB image to GPU and generating mipmaps\n");

    MIMITA_GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    MIMITA_GL_CALL(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        image.width,
        image.height,
        0,
        srcFormat,
        GL_UNSIGNED_BYTE,
        image.image.data()
    ));

    // REPEAT lets UVs outside 0..1 tile. This is useful for blockout/material-style maps.
    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

    // Minification = texture is smaller on screen than in memory. Use mipmaps.
    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));

    MIMITA_GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    MIMITA_GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
    Debug::log(Debug::Category::GLB, "[TEXTURE] mipmaps generated for GLB texture tex=%u\n", tex);

    if (GLDebug::extensionSupported("GL_EXT_texture_filter_anisotropic"))
    {
        GLfloat maxAniso = 1.0f;
        MIMITA_GL_CALL(glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso));
        if (maxAniso > 1.0f)
        {
            GLfloat useAniso = maxAniso < 16.0f ? maxAniso : 16.0f;
            MIMITA_GL_CALL(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, useAniso));
            Debug::log(Debug::Category::GLB, "[TEXTURE] anisotropic filtering %.1fx applied to GLB texture\n", useAniso);
        }
    }
    else
    {
        Debug::logOnce(Debug::Category::GLB, "anisotropy-unsupported",
                       "[TEXTURE] GL_EXT_texture_filter_anisotropic unsupported; anisotropic filtering skipped\n");
    }

    MIMITA_GL_CALL(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
    return tex;
}

const unsigned char* accessorPtr(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    size_t stride = accessor.ByteStride(view);
    if (stride != 0)
        return stride;
    return (size_t)tinygltf::GetComponentSizeInBytes(accessor.componentType) *
           (size_t)tinygltf::GetNumComponentsInType(accessor.type);
}

glm::vec3 readVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, glm::vec3 fallback)
{
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
        return fallback;
    if (index >= accessor.count)
        return fallback;
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    glm::vec3 out{f[0], f[1], f[2]};
    return validVec3(out) ? out : fallback;
}

glm::vec2 readVec2(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, glm::vec2 fallback)
{
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2)
        return fallback;
    if (index >= accessor.count)
        return fallback;
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    glm::vec2 out{f[0], f[1]};
    return validVec2(out) ? out : fallback;
}

bool readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t i, unsigned int& out)
{
    out = 0;
    if (i >= accessor.count)
        return false;
    const unsigned char* base = accessorPtr(model, accessor);
    const unsigned char* p = base + i * accessorStride(model, accessor);
    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            out = *reinterpret_cast<const unsigned char*>(p);
            return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            out = *reinterpret_cast<const unsigned short*>(p);
            return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            out = *reinterpret_cast<const unsigned int*>(p);
            return true;
        default:
            GLB_LOG("[GLB WARNING] unsupported index component type %d\n", accessor.componentType);
            return false;
    }
}

bool nodeMatrix(const tinygltf::Node& node, int nodeIndex, glm::mat4& out)
{
    out = glm::mat4(1.0f);
    if (node.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                m[col][row] = (float)node.matrix[col * 4 + row];
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                if (!std::isfinite(m[col][row]))
                {
                    GLB_LOG("[GLB WARNING] node=%d has NaN/INF matrix; using identity\n", nodeIndex);
                    return false;
                }
        out = m;
        return true;
    }

    glm::vec3 t(0.0f);
    if (node.translation.size() == 3)
        t = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};
    if (!validVec3(t))
    {
        GLB_LOG("[GLB WARNING] node=%d invalid translation; using identity\n", nodeIndex);
        return false;
    }

    glm::quat r(1, 0, 0, 0);
    if (node.rotation.size() == 4)
        r = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
    if (!std::isfinite(r.w) || !std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.z))
    {
        GLB_LOG("[GLB WARNING] node=%d invalid rotation; using identity\n", nodeIndex);
        return false;
    }

    glm::vec3 s(1.0f);
    if (node.scale.size() == 3)
        s = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};
    if (!validVec3(s))
    {
        GLB_LOG("[GLB WARNING] node=%d invalid scale; using identity\n", nodeIndex);
        return false;
    }

    out = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
    return true;
}

void generateTriangleNormals(std::vector<Vertex>& verts, size_t first, size_t count)
{
    for (size_t i = first; i + 2 < first + count; i += 3)
    {
        glm::vec3 a = verts[i + 0].pos;
        glm::vec3 b = verts[i + 1].pos;
        glm::vec3 c = verts[i + 2].pos;
        glm::vec3 n = glm::cross(b - a, c - a);
        if (glm::length(n) < 0.0001f)
            n = {0, 0, 1};
        else
            n = glm::normalize(n);
        verts[i + 0].normal = n;
        verts[i + 1].normal = n;
        verts[i + 2].normal = n;
    }
}

void appendPrimitive(
    const tinygltf::Model& model,
    int meshIndex,
    int primitiveIndex,
    const tinygltf::Primitive& primitive,
    const glm::mat4& transform,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh
) {
    if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
        GLB_LOG("[GLB] mesh=%d prim=%d begin mode=%d material=%d indices=%d attributes=%zu\n",
                meshIndex, primitiveIndex, primitive.mode, primitive.material, primitive.indices, primitive.attributes.size());

    if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipping non-triangle primitive mode=%d\n",
                meshIndex, primitiveIndex, primitive.mode);
        return;
    }

    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end())
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: missing POSITION\n", meshIndex, primitiveIndex);
        return;
    }

    const tinygltf::Accessor* posAccessor = nullptr;
    if (!validateAccessor(model, posIt->second, "POSITION", meshIndex, primitiveIndex,
                          TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT, &posAccessor))
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: invalid POSITION accessor\n", meshIndex, primitiveIndex);
        return;
    }

    const tinygltf::Accessor* normalAccessor = nullptr;
    const tinygltf::Accessor* uvAccessor = nullptr;

    auto nIt = primitive.attributes.find("NORMAL");
    if (nIt != primitive.attributes.end())
    {
        if (!validateAccessor(model, nIt->second, "NORMAL", meshIndex, primitiveIndex,
                              TINYGLTF_TYPE_VEC3, TINYGLTF_COMPONENT_TYPE_FLOAT, &normalAccessor))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d invalid NORMAL; generating fallback normals\n",
                    meshIndex, primitiveIndex);
            normalAccessor = nullptr;
        }
    }
    else
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d missing NORMAL; generating fallback normals\n", meshIndex, primitiveIndex);

    auto uvIt = primitive.attributes.find("TEXCOORD_0");
    if (uvIt != primitive.attributes.end())
    {
        if (!validateAccessor(model, uvIt->second, "TEXCOORD_0", meshIndex, primitiveIndex,
                              TINYGLTF_TYPE_VEC2, TINYGLTF_COMPONENT_TYPE_FLOAT, &uvAccessor))
        {
            printf("[GLB UV WARNING] mesh=%d prim=%d invalid TEXCOORD_0; using uv=(0,0)\n",
                    meshIndex, primitiveIndex);
            uvAccessor = nullptr;
        }
        else if (uvAccessor && uvAccessor->count > 0)
        {
            glm::vec2 uv0 = readVec2(model, *uvAccessor, 0, {0,0});
            glm::vec2 uv1 = readVec2(model, *uvAccessor, std::min((size_t)1, uvAccessor->count - 1), {0,0});
            printf("[GLB UV] mesh=%d prim=%d hasTexcoord0=yes uvCount=%zu vertexCount=%zu uv0=(%.4f,%.4f) uv1=(%.4f,%.4f)\n",
                   meshIndex, primitiveIndex, uvAccessor->count, posAccessor->count,
                   uv0.x, uv0.y, uv1.x, uv1.y);
        }
    }
    else
        printf("[GLB UV] mesh=%d prim=%d hasTexcoord0=no; using uv=(0,0)\n", meshIndex, primitiveIndex);

    const tinygltf::Accessor* indexAccessor = nullptr;
    if (primitive.indices >= 0)
    {
        if (!validateAccessor(model, primitive.indices, "INDICES", meshIndex, primitiveIndex,
                              TINYGLTF_TYPE_SCALAR, 0, &indexAccessor))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: invalid indices accessor\n", meshIndex, primitiveIndex);
            return;
        }

        if (indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE &&
            indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT &&
            indexAccessor->componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d skipped: unsupported index component=%s\n",
                    meshIndex, primitiveIndex, componentTypeName(indexAccessor->componentType));
            return;
        }
    }

    if (primitive.material >= (int)model.materials.size())
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d material index out of range material=%d materials=%zu; using default\n",
                meshIndex, primitiveIndex, primitive.material, model.materials.size());

    Mesh::Batch batch;
    batch.materialIndex = primitive.material;
    batch.materialName = primitive.material >= 0 && primitive.material < (int)model.materials.size()
        ? model.materials[primitive.material].name
        : "default";
    static GLuint defaultTexture = 0;
    if (!defaultTexture)
        defaultTexture = gTextures.get("default");
    batch.texture = defaultTexture;
    if (primitive.material >= 0 && primitive.material < (int)materialTextures.size() && materialTextures[primitive.material])
        batch.texture = materialTextures[primitive.material];
    batch.first = mesh.verts.size();

    glm::mat3 normalXform(1.0f);
    float det = glm::determinant(glm::mat3(transform));
    if (std::fabs(det) > 0.000001f && std::isfinite(det))
        normalXform = glm::transpose(glm::inverse(glm::mat3(transform)));
    else
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d non-invertible transform for normals; using identity normal transform\n",
                meshIndex, primitiveIndex);

    auto pushVertex = [&](unsigned int vi) {
        if (vi >= posAccessor->count)
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d vertex index out of POSITION range vi=%u count=%zu\n",
                    meshIndex, primitiveIndex, vi, posAccessor->count);
            return false;
        }

        Vertex v{};
        glm::vec3 pos = readVec3(model, *posAccessor, vi, {0,0,0});
        if (!validVec3(pos))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d POSITION NaN/INF vi=%u; vertex skipped\n",
                    meshIndex, primitiveIndex, vi);
            return false;
        }

        v.pos = glm::vec3(transform * glm::vec4(pos, 1.0f));
        if (!validVec3(v.pos))
        {
            GLB_LOG("[GLB WARNING] mesh=%d prim=%d transformed POSITION NaN/INF vi=%u; vertex skipped\n",
                    meshIndex, primitiveIndex, vi);
            return false;
        }

        if (normalAccessor && vi < normalAccessor->count)
        {
            glm::vec3 n = readVec3(model, *normalAccessor, vi, {0,0,1});
            glm::vec3 transformedN = normalXform * n;
            if (glm::length(transformedN) > 0.000001f && validVec3(transformedN))
                v.normal = glm::normalize(transformedN);
            else
                v.normal = {0,0,1};
        }
        else
        {
            v.normal = {0,0,0};
        }

        if (uvAccessor && vi < uvAccessor->count)
            v.uv = readVec2(model, *uvAccessor, vi, {0,0});
        else
            v.uv = {0,0};

        mesh.verts.push_back(v);
        return true;
    };

    if (primitive.indices >= 0)
    {
        if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
            GLB_LOG("[GLB] mesh=%d prim=%d extracting indexed vertices indexCount=%zu positionCount=%zu\n",
                    meshIndex, primitiveIndex, indexAccessor->count, posAccessor->count);
        for (size_t i = 0; i < indexAccessor->count; ++i)
        {
            unsigned int vi = 0;
            if (!readIndex(model, *indexAccessor, i, vi))
            {
                GLB_LOG("[GLB WARNING] mesh=%d prim=%d failed reading index i=%zu; skipped\n",
                        meshIndex, primitiveIndex, i);
                continue;
            }
            pushVertex(vi);
        }
    }
    else
    {
        if (shouldLogPrimitiveDetails(meshIndex, primitiveIndex))
            GLB_LOG("[GLB] mesh=%d prim=%d extracting non-indexed vertices positionCount=%zu\n",
                    meshIndex, primitiveIndex, posAccessor->count);
        for (size_t i = 0; i < posAccessor->count; ++i)
            pushVertex((unsigned int)i);
    }

    batch.count = mesh.verts.size() - batch.first;
    if (batch.count < 3)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d produced fewer than 3 vertices; skipped batch\n",
                meshIndex, primitiveIndex);
        mesh.verts.resize(batch.first);
        return;
    }

    size_t remainder = batch.count % 3;
    if (remainder != 0)
    {
        GLB_LOG("[GLB WARNING] mesh=%d prim=%d vertex count not multiple of 3 count=%zu; truncating remainder=%zu\n",
                meshIndex, primitiveIndex, batch.count, remainder);
        mesh.verts.resize(mesh.verts.size() - remainder);
        batch.count -= remainder;
    }

    if (!normalAccessor)
        generateTriangleNormals(mesh.verts, batch.first, batch.count);

    mesh.batches.push_back(batch);
    static int loggedPrimitives = 0;
    if (loggedPrimitives < 20 || (loggedPrimitives % 100) == 0)
        GLB_LOG("[GLB] loaded mesh=%d prim=%d material=%s verts=%zu triangles=%zu texture=%u\n",
                meshIndex, primitiveIndex, batch.materialName.c_str(), batch.count, batch.count / 3, batch.texture);
    loggedPrimitives++;
}

static bool isSkyNode(const std::string& name)
{
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower += (char)std::tolower((unsigned char)c);
    return lower.find("sky") != std::string::npos ||
           lower.find("skybox") != std::string::npos ||
           lower.find("skydome") != std::string::npos ||
           lower.find("sky_dome") != std::string::npos ||
           lower.find("environment") != std::string::npos ||
           lower.find("worldsphere") != std::string::npos;
}

void walkNode(
    const tinygltf::Model& model,
    int nodeIndex,
    const glm::mat4& parent,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh,
    std::unordered_set<int>& activeNodes,
    int depth,
    Mesh* skyMesh = nullptr
) {
    if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size())
    {
        GLB_LOG("[GLB WARNING] node index out of range node=%d nodes=%zu\n", nodeIndex, model.nodes.size());
        return;
    }

    if (depth > 128)
    {
        GLB_LOG("[GLB ERROR] node traversal depth exceeded at node=%d; possible malformed hierarchy\n", nodeIndex);
        return;
    }

    if (activeNodes.find(nodeIndex) != activeNodes.end())
    {
        GLB_LOG("[GLB ERROR] cyclic node hierarchy detected at node=%d; skipping recursion\n", nodeIndex);
        return;
    }

    activeNodes.insert(nodeIndex);

    const tinygltf::Node& node = model.nodes[nodeIndex];
    if (glbVerbose() || depth < 2 || nodeIndex % 100 == 0)
        GLB_LOG("[GLB] node=%d name=%s mesh=%d children=%zu depth=%d\n",
                nodeIndex, node.name.c_str(), node.mesh, node.children.size(), depth);

    glm::mat4 local(1.0f);
    nodeMatrix(node, nodeIndex, local);
    glm::mat4 world = parent * local;

    static int loggedNodes = 0;
    if (node.mesh >= 0 && node.mesh < (int)model.meshes.size())
    {
        const tinygltf::Mesh& gltfMesh = model.meshes[node.mesh];
        Mesh& target = (skyMesh && isSkyNode(node.name)) ? *skyMesh : mesh;

        if (loggedNodes < 20 || (loggedNodes % 100) == 0)
        {
            if (&target == skyMesh)
                GLB_LOG("[GLB] SKY node=%d name=%s meshIndex=%d meshName=%s primitives=%zu\n",
                        nodeIndex, node.name.c_str(), node.mesh, gltfMesh.name.c_str(), gltfMesh.primitives.size());
            else
                GLB_LOG("[GLB] node=%d meshIndex=%d meshName=%s primitives=%zu\n",
                        nodeIndex, node.mesh, gltfMesh.name.c_str(), gltfMesh.primitives.size());
        }
        loggedNodes++;

        if (&target == skyMesh)
            GLB_LOG("[GLB] ROUTING TO SKY MESH node=%d name=%s\n", nodeIndex, node.name.c_str());

        for (int primIndex = 0; primIndex < (int)gltfMesh.primitives.size(); ++primIndex)
            appendPrimitive(model, node.mesh, primIndex, gltfMesh.primitives[primIndex], world, materialTextures, target);
    }
    else if (node.mesh >= 0)
    {
        GLB_LOG("[GLB WARNING] node=%d mesh index out of range mesh=%d meshes=%zu\n",
                nodeIndex, node.mesh, model.meshes.size());
    }

    for (int child : node.children)
        walkNode(model, child, world, materialTextures, mesh, activeNodes, depth + 1, skyMesh);

    activeNodes.erase(nodeIndex);
}

}

Mesh loadOBJ(const std::string& path)
{
    std::string resolvedPath = resolveAssetPath(path);
    printf("OBJ path = %s\n", resolvedPath.c_str());

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(resolvedPath.c_str()))
    {
        printf("OBJ error: %s\n", reader.Error().c_str());
        return Mesh{};
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    Mesh mesh;
    for (const auto& shape : shapes)
    {
        for (auto idx : shape.mesh.indices)
        {
            Vertex v{};
            v.pos = {
                attrib.vertices[3 * idx.vertex_index + 0],
                attrib.vertices[3 * idx.vertex_index + 1],
                attrib.vertices[3 * idx.vertex_index + 2]
            };
            v.normal = {0, 0, 1};
            v.uv = {0, 0};
            mesh.verts.push_back(v);
        }
    }

    generateTriangleNormals(mesh.verts, 0, mesh.verts.size());
    Mesh::Batch batch;
    batch.materialName = "default";
    batch.texture = gTextures.get("default");
    batch.first = 0;
    batch.count = mesh.verts.size();
    mesh.batches.push_back(batch);

    printf("OBJ verts = %zu\n", mesh.verts.size());
    return mesh;
}

Mesh loadGLB(const std::string& path, bool /*storeDebugInfo*/, Mesh* skyMesh)
{
    std::string resolvedPath = resolveAssetPath(path);
    GLB_LOG("[GLB] path = %s\n", resolvedPath.c_str());

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;

    Debug::log(Debug::Category::GLB, "[GLB] before tinygltf LoadBinaryFromFile\n");
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, resolvedPath);
    Debug::log(Debug::Category::GLB, "[GLB] after tinygltf LoadBinaryFromFile ok=%d\n", ok ? 1 : 0);

    if (!warn.empty()) GLB_LOG("[GLB WARNING] %s\n", warn.c_str());
    if (!err.empty()) GLB_LOG("[GLB ERROR] %s\n", err.c_str());
    if (!ok)
    {
        GLB_LOG("[GLB ERROR] Failed to load GLB\n");
        return Mesh{};
    }

    Debug::logOnce(Debug::Category::GLB, resolvedPath.c_str(), "[GLB] model meshes=%zu nodes=%zu materials=%zu textures=%zu images=%zu scenes=%zu buffers=%zu bufferViews=%zu accessors=%zu defaultScene=%d\n",
            model.meshes.size(), model.nodes.size(), model.materials.size(), model.textures.size(),
            model.images.size(), model.scenes.size(), model.buffers.size(), model.bufferViews.size(),
            model.accessors.size(), model.defaultScene);

    for (int meshIndex = 0; meshIndex < (int)model.meshes.size(); ++meshIndex)
    {
        const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];
        // if (glbVerbose() || meshIndex < 20 || meshIndex % 100 == 0)
        if (glbVerbose())
            GLB_LOG("[GLB] inspect mesh=%d name=%s primitives=%zu\n",
                    meshIndex, gltfMesh.name.c_str(), gltfMesh.primitives.size());
        for (int primIndex = 0; primIndex < (int)gltfMesh.primitives.size(); ++primIndex)
        {
            const tinygltf::Primitive& prim = gltfMesh.primitives[primIndex];
            if (shouldLogPrimitiveDetails(meshIndex, primIndex))
            {
                GLB_LOG("[GLB] inspect mesh=%d prim=%d mode=%d material=%d indices=%d attrCount=%zu\n",
                        meshIndex, primIndex, prim.mode, prim.material, prim.indices, prim.attributes.size());
                for (const auto& attr : prim.attributes)
                {
                    GLB_LOG("[GLB] inspect mesh=%d prim=%d attr=%s accessor=%d\n",
                            meshIndex, primIndex, attr.first.c_str(), attr.second);
                }
            }
        }
    }

    // Derive GLB directory for external texture resolution
    std::string glbDir;
    {
        size_t slashPos = resolvedPath.find_last_of("/\\");
        if (slashPos != std::string::npos)
            glbDir = resolvedPath.substr(0, slashPos);
    }

    std::vector<GLuint> imageTextures(model.images.size(), 0);
    for (size_t i = 0; i < model.images.size(); ++i)
    {
        const tinygltf::Image& img = model.images[i];
        bool embedded = img.bufferView >= 0;
        printf("[GLB IMAGE] index=%zu name=%s width=%d height=%d components=%d embedded=%s bv=%d\n",
               i, img.name.c_str(), img.width, img.height, img.component,
               embedded ? "yes" : "no", img.bufferView);
        imageTextures[i] = uploadGLBImage(img, (int)i, glbDir);
    }

    // Temp storage for solid-color textures (created before mesh exists)
    std::vector<GLuint> colorTextures;

    std::vector<GLuint> materialTextures(model.materials.size(), 0);
    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const tinygltf::Material& mat = model.materials[i];

        int baseColorTexIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
        const std::vector<double>& bcf = mat.pbrMetallicRoughness.baseColorFactor;
        printf("[GLB MATERIAL] material=%zu name=%s baseColorTexture=%d baseColorFactor=[%.2f,%.2f,%.2f,%.2f]\n",
               i, mat.name.c_str(), baseColorTexIndex,
               bcf.size() >= 4 ? bcf[0] : 1.0,
               bcf.size() >= 4 ? bcf[1] : 1.0,
               bcf.size() >= 4 ? bcf[2] : 1.0,
               bcf.size() >= 4 ? bcf[3] : 1.0);

        const auto& pbr = mat.pbrMetallicRoughness;
        int texIndex = pbr.baseColorTexture.index;

        if (texIndex >= 0 && texIndex < (int)model.textures.size())
        {
            int imageIndex = model.textures[texIndex].source;
            printf("[GLB MATERIAL] material=%zu textureIndex=%d sourceImage=%d\n",
                   i, texIndex, imageIndex);
            if (imageIndex >= 0 && imageIndex < (int)imageTextures.size())
            {
                materialTextures[i] = imageTextures[imageIndex];
            }
            else
            {
                printf("[GLB WARNING] material=%zu texture source image out of range image=%d images=%zu\n",
                       i, imageIndex, imageTextures.size());
            }
        }
        else if (texIndex >= 0)
        {
            printf("[GLB WARNING] material=%zu texture index out of range texture=%d textures=%zu\n",
                   i, texIndex, model.textures.size());
        }

        if (!materialTextures[i])
        {
            bool hasColorFactor = bcf.size() >= 3;
            if (hasColorFactor)
            {
                float r = (float)(bcf.size() >= 1 ? bcf[0] : 1.0);
                float g = (float)(bcf.size() >= 2 ? bcf[1] : 1.0);
                float b = (float)(bcf.size() >= 3 ? bcf[2] : 1.0);
                float a = (float)(bcf.size() >= 4 ? bcf[3] : 1.0);

                printf("[GLB MATERIAL] material=%zu (%s) no texture; generating 1x1 from baseColorFactor (%.2f,%.2f,%.2f,%.2f)\n",
                       i, mat.name.c_str(), r, g, b, a);

                GLuint tex = 0;
                glGenTextures(1, &tex);
                if (tex)
                {
                    glBindTexture(GL_TEXTURE_2D, tex);
                    unsigned char pixels[4];
                    pixels[0] = (unsigned char)(r * 255.0f);
                    pixels[1] = (unsigned char)(g * 255.0f);
                    pixels[2] = (unsigned char)(b * 255.0f);
                    pixels[3] = (unsigned char)(a * 255.0f);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    materialTextures[i] = tex;
                    colorTextures.push_back(tex);
                }
                else
                {
                    printf("[GLB WARNING] material=%zu failed to create solid color texture; using default.png\n", i);
                    materialTextures[i] = gTextures.get("default");
                }
            }
            else
            {
                printf("[GLB TEXTURE WARNING] material=%zu (%s) has no baseColor texture and no baseColorFactor; using default.png\n",
                       i, mat.name.c_str());
                materialTextures[i] = gTextures.get("default");
            }
        }

        // Check KHR_texture_transform
        const auto& bct = pbr.baseColorTexture;
        auto texExtIt = bct.extensions.find("KHR_texture_transform");
        if (texExtIt != bct.extensions.end())
        {
            const tinygltf::Value& extVal = texExtIt->second;
            printf("[GLB MATERIAL] material=%zu KHR_texture_transform present\n", i);
        }

        printf("[GLB MATERIAL] material=%zu name=%s texture=%u\n", i, mat.name.c_str(), materialTextures[i]);
    }

    Mesh mesh;
    for (GLuint texture : imageTextures)
    {
        if (texture)
            mesh.ownedTextures.push_back(texture);
    }
    for (GLuint texture : colorTextures)
    {
        if (texture)
            mesh.ownedTextures.push_back(texture);
    }
    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
    {
        const tinygltf::Scene& scene = model.scenes[sceneIndex];
        GLB_LOG("[GLB] walking scene=%d nodes=%zu\n", sceneIndex, scene.nodes.size());
        for (int node : scene.nodes)
        {
            std::unordered_set<int> activeNodes;
            walkNode(model, node, glm::mat4(1.0f), materialTextures, mesh, activeNodes, 0, skyMesh);
        }
    }
    else
    {
        GLB_LOG("[GLB WARNING] no valid scene; loading meshes without node transforms sceneIndex=%d scenes=%zu\n",
                sceneIndex, model.scenes.size());
        for (int meshIndex = 0; meshIndex < (int)model.meshes.size(); ++meshIndex)
        {
            const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];
            for (int primIndex = 0; primIndex < (int)gltfMesh.primitives.size(); ++primIndex)
                appendPrimitive(model, meshIndex, primIndex, gltfMesh.primitives[primIndex], glm::mat4(1.0f), materialTextures, mesh);
        }
    }

    std::vector<Mesh::Batch> merged;
    for (const Mesh::Batch& batch : mesh.batches)
    {
        if (!merged.empty() &&
            merged.back().texture == batch.texture &&
            merged.back().first + merged.back().count == batch.first)
        {
            merged.back().count += batch.count;
        }
        else
        {
            merged.push_back(batch);
        }
    }
    if (merged.size() != mesh.batches.size())
        GLB_LOG("[GLB] merged material batches %zu -> %zu to reduce texture binds\n", mesh.batches.size(), merged.size());
    mesh.batches = merged;

    // Store GLB debug data
    {
        gGLBDebug.clear();
        gGLBDebug.meshCount = (int)model.meshes.size();
        gGLBDebug.loaded = true;

        for (size_t i = 0; i < model.materials.size(); ++i)
        {
            const tinygltf::Material& mat = model.materials[i];
            const auto& pbr = mat.pbrMetallicRoughness;
            GLBMaterialInfo mi;
            mi.index = (int)i;
            mi.name = mat.name;
            mi.baseColorTextureIndex = pbr.baseColorTexture.index;
            mi.hasTexture = mi.baseColorTextureIndex >= 0;
            mi.hasColorFactor = pbr.baseColorFactor.size() >= 3;
            for (int k = 0; k < 4 && k < (int)pbr.baseColorFactor.size(); ++k)
                mi.baseColorFactor[k] = pbr.baseColorFactor[k];
            for (int k = pbr.baseColorFactor.size(); k < 4; ++k)
                mi.baseColorFactor[k] = (k == 3) ? 1.0 : 0.0;

            mi.hasKhrTextureTransform = false;
            auto extIt = pbr.baseColorTexture.extensions.find("KHR_texture_transform");
            if (extIt != pbr.baseColorTexture.extensions.end())
            {
                mi.hasKhrTextureTransform = true;
                const tinygltf::Value& extVal = extIt->second;
                if (extVal.IsObject())
                {
                    tinygltf::Value::Object extObj = extVal.Get<tinygltf::Value::Object>();
                    auto offIt = extObj.find("offset");
                    if (offIt != extObj.end() && offIt->second.IsArray())
                    {
                        tinygltf::Value::Array arr = offIt->second.Get<tinygltf::Value::Array>();
                        if (arr.size() >= 2)
                        {
                            mi.texTransformOffset[0] = arr[0].GetNumberAsDouble();
                            mi.texTransformOffset[1] = arr[1].GetNumberAsDouble();
                        }
                    }
                    auto scaleIt = extObj.find("scale");
                    if (scaleIt != extObj.end() && scaleIt->second.IsArray())
                    {
                        tinygltf::Value::Array arr = scaleIt->second.Get<tinygltf::Value::Array>();
                        if (arr.size() >= 2)
                        {
                            mi.texTransformScale[0] = arr[0].GetNumberAsDouble();
                            mi.texTransformScale[1] = arr[1].GetNumberAsDouble();
                        }
                    }
                }
            }
            gGLBDebug.materials.push_back(mi);
        }

        for (size_t i = 0; i < model.images.size(); ++i)
        {
            const tinygltf::Image& img = model.images[i];
            GLBImageInfo ii;
            ii.index = (int)i;
            ii.name = img.name;
            ii.width = img.width;
            ii.height = img.height;
            ii.components = img.component;
            ii.embedded = img.bufferView >= 0;
            ii.uri = img.uri;
            gGLBDebug.images.push_back(ii);
        }

        // Import KHR_lights_punctual if present
        {
            auto lightsExtIt = model.extensions.find("KHR_lights_punctual");
            if (lightsExtIt != model.extensions.end())
            {
                const tinygltf::Value& lightsExt = lightsExtIt->second;
                if (lightsExt.IsObject())
                {
                    tinygltf::Value::Object lightsData = lightsExt.Get<tinygltf::Value::Object>();
                    auto lightsArrayIt = lightsData.find("lights");
                    if (lightsArrayIt != lightsData.end() && lightsArrayIt->second.IsArray())
                    {
                        tinygltf::Value::Array lightsArr = lightsArrayIt->second.Get<tinygltf::Value::Array>();
                        for (size_t li = 0; li < lightsArr.size(); ++li)
                        {
                            const tinygltf::Value& lv = lightsArr[li];
                            if (!lv.IsObject()) continue;
                            tinygltf::Value::Object l = lv.Get<tinygltf::Value::Object>();
                            GLBLightInfo info;
                            auto nameIt = l.find("name");
                            info.name = (nameIt != l.end() && nameIt->second.IsString())
                                ? nameIt->second.Get<std::string>() : ("light_" + std::to_string(li));
                            auto typeIt = l.find("type");
                            info.type = (typeIt != l.end() && typeIt->second.IsString())
                                ? typeIt->second.Get<std::string>() : "point";
                            auto colorIt = l.find("color");
                            if (colorIt != l.end() && colorIt->second.IsArray())
                            {
                                tinygltf::Value::Array cArr = colorIt->second.Get<tinygltf::Value::Array>();
                                for (int k = 0; k < 3 && k < (int)cArr.size(); ++k)
                                    info.color[k] = cArr[k].GetNumberAsDouble();
                            }
                            auto intensityIt = l.find("intensity");
                            info.intensity = (intensityIt != l.end() && intensityIt->second.IsNumber())
                                ? intensityIt->second.Get<double>() : 1.0;
                            auto rangeIt = l.find("range");
                            info.range = (rangeIt != l.end() && rangeIt->second.IsNumber())
                                ? rangeIt->second.Get<double>() : 0.0;
                            info.position[0] = info.position[1] = info.position[2] = 0.0;
                            info.direction[0] = info.direction[1] = info.direction[2] = 0.0;
                            info.innerConeAngle = 0.0;
                            info.outerConeAngle = 0.0;
                            if (info.type == "spot")
                            {
                                auto innerIt = l.find("innerConeAngle");
                                info.innerConeAngle = (innerIt != l.end() && innerIt->second.IsNumber())
                                    ? innerIt->second.Get<double>() : 0.0;
                                auto outerIt = l.find("outerConeAngle");
                                info.outerConeAngle = (outerIt != l.end() && outerIt->second.IsNumber())
                                    ? outerIt->second.Get<double>() : 0.7853981633974483;
                            }
                            gGLBDebug.lights.push_back(info);
                            printf("[GLB LIGHT] imported light=%zu name=%s type=%s intensity=%.2f range=%.2f\n",
                                   li, info.name.c_str(), info.type.c_str(), info.intensity, info.range);
                        }
                    }
                }
            }
        }

        // For each node, check if it references a light (KHR_lights_punctual node extension)
        for (size_t ni = 0; ni < model.nodes.size(); ++ni)
        {
            const tinygltf::Node& node = model.nodes[ni];
            auto nodeLightIt = node.extensions.find("KHR_lights_punctual");
            if (nodeLightIt != node.extensions.end())
            {
                const tinygltf::Value& nodeLightVal = nodeLightIt->second;
                if (nodeLightVal.IsObject())
                {
                    tinygltf::Value::Object nodeLightData = nodeLightVal.Get<tinygltf::Value::Object>();
                    auto lightIdxIt = nodeLightData.find("light");
                    if (lightIdxIt != nodeLightData.end() && lightIdxIt->second.IsNumber())
                    {
                        int lightIdx = lightIdxIt->second.Get<int>();
                        if (lightIdx >= 0 && lightIdx < (int)gGLBDebug.lights.size())
                        {
                            glm::mat4 nodeXform(1.0f);
                            nodeMatrix(node, (int)ni, nodeXform);
                            auto& lightInfo = gGLBDebug.lights[lightIdx];
                            lightInfo.position[0] = nodeXform[3][0];
                            lightInfo.position[1] = nodeXform[3][1];
                            lightInfo.position[2] = nodeXform[3][2];
                            glm::vec3 dir = nodeXform * glm::vec4(0, 0, -1, 0);
                            lightInfo.direction[0] = dir.x;
                            lightInfo.direction[1] = dir.y;
                            lightInfo.direction[2] = dir.z;
                            printf("[GLB LIGHT] node=%zu name=%s lightIdx=%d pos=(%.2f,%.2f,%.2f)\n",
                                   ni, node.name.c_str(), lightIdx,
                                   lightInfo.position[0], lightInfo.position[1], lightInfo.position[2]);
                        }
                    }
                }
            }
        }
    }

    GLB_LOG("[GLB] verts=%zu triangles=%zu batches=%zu\n", mesh.verts.size(), mesh.verts.size() / 3, mesh.batches.size());
    return mesh;
}

void releaseMeshGLResources(Mesh& mesh)
{
    if (!mesh.ownedTextures.empty())
    {
        glDeleteTextures(
            (GLsizei)mesh.ownedTextures.size(),
            mesh.ownedTextures.data());
        GLB_LOG("[GLB] released owned textures=%zu\n", mesh.ownedTextures.size());
    }
    mesh.ownedTextures.clear();
}
