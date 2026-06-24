#include "map_common.h"
#include "map_loader.h"
#include "map-loader-mesh.h"
#include "tinygltf/tiny_gltf.h"

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "debug/debug-log.h"

#define GLB_LOG(...) Debug::logAuto(Debug::Category::GLB, __VA_ARGS__)

namespace {

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
    if (!(std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z)))
        return fallback;
    return out;
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
    if (!(std::isfinite(out.x) && std::isfinite(out.y)))
        return fallback;
    return out;
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

} // anonymous namespace
