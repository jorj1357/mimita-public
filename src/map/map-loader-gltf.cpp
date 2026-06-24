#include "map_common.h"
#include "map_loader.h"
#include "map-loader-mesh.h"
#include "tinygltf/tiny_gltf.h"

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include <unordered_set>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "world/texture-store.h"
#include "debug/debug-log.h"

extern TextureStore gTextures;

#define GLB_LOG(...) Debug::logAuto(Debug::Category::GLB, __VA_ARGS__)

namespace {

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

} // anonymous namespace
