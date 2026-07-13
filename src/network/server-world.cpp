#include "network/server.h"
#include "utils/path_utils.h"
#include "tinygltf/tiny_gltf.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>

namespace MimitaNet {
namespace {

static char gTimestampBuf[64];

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
    if (stride)
        return stride;
    return (size_t)tinygltf::GetComponentSizeInBytes(accessor.componentType) *
           (size_t)tinygltf::GetNumComponentsInType(accessor.type);
}

glm::vec3 readVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index)
{
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    return {f[0], f[1], f[2]};
}

bool readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, unsigned int& out)
{
    const unsigned char* base = accessorPtr(model, accessor);
    const unsigned char* p = base + index * accessorStride(model, accessor);
    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: out = *reinterpret_cast<const unsigned char*>(p); return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: out = *reinterpret_cast<const unsigned short*>(p); return true;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: out = *reinterpret_cast<const unsigned int*>(p); return true;
        default: return false;
    }
}

glm::mat4 nodeTransform(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 out(1.0f);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                out[c][r] = (float)node.matrix[c * 4 + r];
        return out;
    }

    glm::vec3 t(0.0f);
    if (node.translation.size() == 3)
        t = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};
    glm::quat q(1, 0, 0, 0);
    if (node.rotation.size() == 4)
        q = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
    glm::vec3 s(1.0f);
    if (node.scale.size() == 3)
        s = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};
    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q) * glm::scale(glm::mat4(1.0f), s);
}

void addTriangle(HeadlessWorld& world, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 n = glm::cross(b - a, c - a);
    float len = glm::length(n);
    if (len < 0.000001f)
        return;

    CollisionTriangle tri;
    tri.a = a;
    tri.b = b;
    tri.c = c;
    tri.normal = n / len;
    world.triangles.push_back(tri);

    glm::vec3 mn = glm::min(glm::min(a, b), c);
    glm::vec3 mx = glm::max(glm::max(a, b), c);
    if (world.triangles.size() == 1)
    {
        world.boundsMin = mn;
        world.boundsMax = mx;
    }
    else
    {
        world.boundsMin = glm::min(world.boundsMin, mn);
        world.boundsMax = glm::max(world.boundsMax, mx);
    }
}

void appendPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& prim, const glm::mat4& transform, HeadlessWorld& world)
{
    if (prim.mode != TINYGLTF_MODE_TRIANGLES)
        return;
    auto posIt = prim.attributes.find("POSITION");
    if (posIt == prim.attributes.end())
        return;

    const tinygltf::Accessor& pos = model.accessors[posIt->second];
    if (pos.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || pos.type != TINYGLTF_TYPE_VEC3 || pos.bufferView < 0)
        return;

    auto vertexAt = [&](unsigned int i) {
        return glm::vec3(transform * glm::vec4(readVec3(model, pos, i), 1.0f));
    };

    if (prim.indices >= 0)
    {
        const tinygltf::Accessor& idx = model.accessors[prim.indices];
        for (size_t i = 0; i + 2 < idx.count; i += 3)
        {
            unsigned int ia = 0, ib = 0, ic = 0;
            if (readIndex(model, idx, i + 0, ia) && readIndex(model, idx, i + 1, ib) && readIndex(model, idx, i + 2, ic) &&
                ia < pos.count && ib < pos.count && ic < pos.count)
                addTriangle(world, vertexAt(ia), vertexAt(ib), vertexAt(ic));
        }
    }
    else
    {
        for (size_t i = 0; i + 2 < pos.count; i += 3)
            addTriangle(world, vertexAt((unsigned)i), vertexAt((unsigned)i + 1), vertexAt((unsigned)i + 2));
    }
}

void walkNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parent, HeadlessWorld& world)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size())
        return;
    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 transform = parent * nodeTransform(node);

    if (node.mesh >= 0 && node.mesh < (int)model.meshes.size())
        for (const tinygltf::Primitive& prim : model.meshes[node.mesh].primitives)
            appendPrimitive(model, prim, transform, world);

    for (int child : node.children)
        walkNode(model, child, transform, world);
}

} // namespace

const char* serverTimestamp()
{
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    snprintf(gTimestampBuf, sizeof(gTimestampBuf), "[%02d:%02d:%02d]",
             t->tm_hour, t->tm_min, t->tm_sec);
    return gTimestampBuf;
}

bool loadHeadlessWorld(const char* path, HeadlessWorld& world)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    std::string resolved = resolveAssetPath(path);
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, resolved);
    if (!warn.empty()) printf("%s [SERVER WORLD WARNING] %s\n", serverTimestamp(), warn.c_str());
    if (!err.empty()) printf("%s [SERVER WORLD ERROR] %s\n", serverTimestamp(), err.c_str());
    if (!ok)
        return false;

    int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
        for (int node : model.scenes[sceneIndex].nodes)
            walkNode(model, node, glm::mat4(1.0f), world);

    // Extract spawnpoints — recursive walk with full parent transform accumulation
    {
        std::function<void(int, glm::mat4)> walkForSpawns;
        walkForSpawns = [&](int nodeIndex, glm::mat4 parentXform) {
            if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size()) return;
            const tinygltf::Node& node = model.nodes[nodeIndex];
            glm::mat4 worldXform = parentXform * nodeTransform(node);

            std::string lowerName = node.name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            if (lowerName.find("spawn") != std::string::npos)
            {
                glm::vec3 pos = glm::vec3(worldXform[3]);
                if (std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z))
                {
                    ServerSpawnPoint sp;
                    sp.position = pos;
                    glm::vec3 forward = glm::normalize(glm::vec3(worldXform[1]));
                    sp.yaw = std::atan2(forward.y, forward.x);
                    world.spawnPoints.push_back(sp);
                    printf("%s [SPAWNPOINT] node=\"%s\" position=(%.2f,%.2f,%.2f) yaw=%.1f\n",
                           serverTimestamp(), node.name.c_str(), pos.x, pos.y, pos.z,
                           glm::degrees(sp.yaw));
                }
            }
            for (int child : node.children)
                walkForSpawns(child, worldXform);
        };

        int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
        if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
        {
            for (int node : model.scenes[sceneIndex].nodes)
                walkForSpawns(node, glm::mat4(1.0f));
        }
        else
        {
            // Fallback: walk all root-level nodes
            for (int i = 0; i < (int)model.nodes.size(); ++i)
                walkForSpawns(i, glm::mat4(1.0f));
        }
    }

    printf("%s [SERVER WORLD] loaded map collision triangles=%zu spawnpoints=%zu bounds=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)\n",
           serverTimestamp(), world.triangles.size(), world.spawnPoints.size(),
           world.boundsMin.x, world.boundsMin.y, world.boundsMin.z,
           world.boundsMax.x, world.boundsMax.y, world.boundsMax.z);
    return !world.triangles.empty();
}

} // namespace MimitaNet
