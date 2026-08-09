// 08 08 2026, 17 20
/* purpose
* Loads the default player body shape headlessly (CPU-only, no GL) so the
* authoritative server can reconstruct real body-part hitboxes for players and
* NPCs at the rewound pose. This is the one source of truth for body-part hit
* validation — no invisible capsules anywhere.
* Does NOT render, animate, or own per-entity pose state.
* Does NOT touch GPU resources or require a window/GL context.
*/

#include "network/server.h"

#include "tinygltf/tiny_gltf.h"
#include "utils/path_utils.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace MimitaNet {
namespace {

constexpr const char* kDefaultBodyPath =
    "assets/entity/player/default/mimita-char-no-animations-v4.glb";

bool isBodyPartName(const std::string& name)
{
    return name == "head" || name == "torso" ||
           name == "leftArm" || name == "rightArm" ||
           name == "leftLeg" || name == "rightLeg";
}

glm::mat4 nodeMatrix(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row)
                m[col][row] = (float)node.matrix[col * 4 + row];
        return m;
    }
    glm::vec3 t(0.0f);
    if (node.translation.size() == 3)
        t = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};
    glm::quat r(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.rotation.size() == 4)
        r = glm::quat((float)node.rotation[3], (float)node.rotation[0],
                      (float)node.rotation[1], (float)node.rotation[2]);
    glm::vec3 s(1.0f);
    if (node.scale.size() == 3)
        s = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};
    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) *
           glm::scale(glm::mat4(1.0f), s);
}

const unsigned char* accessorPtr(const tinygltf::Model& model,
                                 const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    return buffer.data.data() + view.byteOffset + accessor.byteOffset;
}

size_t accessorStride(const tinygltf::Model& model,
                      const tinygltf::Accessor& accessor)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    size_t stride = accessor.ByteStride(view);
    if (stride != 0)
        return stride;
    return (size_t)tinygltf::GetComponentSizeInBytes(accessor.componentType) *
           (size_t)tinygltf::GetNumComponentsInType(accessor.type);
}

glm::vec3 readVec3(const tinygltf::Model& model,
                   const tinygltf::Accessor& accessor, size_t index)
{
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.type != TINYGLTF_TYPE_VEC3)
        return glm::vec3(0.0f);
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    return {f[0], f[1], f[2]};
}

unsigned int readIndex(const tinygltf::Model& model,
                       const tinygltf::Accessor& accessor, size_t i)
{
    const unsigned char* base = accessorPtr(model, accessor);
    const unsigned char* p = base + i * accessorStride(model, accessor);
    switch (accessor.componentType)
    {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *reinterpret_cast<const unsigned char*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return *reinterpret_cast<const unsigned short*>(p);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return *reinterpret_cast<const unsigned int*>(p);
        default:
            return 0;
    }
}

// Gather a node's mesh positions in NODE-LOCAL space into an AABB.
bool nodeMeshLocalAabb(const tinygltf::Model& model, const tinygltf::Node& node,
                       glm::vec3& outMin, glm::vec3& outMax)
{
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(-std::numeric_limits<float>::max());
    bool any = false;
    auto addMesh = [&](int meshIdx) {
        if (meshIdx < 0 || meshIdx >= (int)model.meshes.size())
            return;
        const tinygltf::Mesh& mesh = model.meshes[meshIdx];
        for (const auto& prim : mesh.primitives)
        {
            auto it = prim.attributes.find("POSITION");
            if (it == prim.attributes.end())
                continue;
            const tinygltf::Accessor& acc = model.accessors[it->second];
            auto positions = [&](size_t i) { return readVec3(model, acc, i); };
            if (prim.indices >= 0)
            {
                const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
                for (size_t i = 0; i < idxAcc.count; ++i)
                {
                    const glm::vec3 p = positions(readIndex(model, idxAcc, i));
                    mn = glm::min(mn, p); mx = glm::max(mx, p); any = true;
                }
            }
            else
            {
                for (size_t i = 0; i < acc.count; ++i)
                {
                    const glm::vec3 p = positions(i);
                    mn = glm::min(mn, p); mx = glm::max(mx, p); any = true;
                }
            }
        }
    };
    addMesh(node.mesh);
    if (!any)
        return false;
    outMin = mn;
    outMax = mx;
    return true;
}

} // namespace

std::vector<ServerPlayerBodyPartTemplate> gServerBodyTemplate;

bool loadServerBodyTemplateFromGlb(const char* path,
                                   std::vector<ServerPlayerBodyPartTemplate>& out)
{
    out.clear();
    const std::string resolved = resolveAssetPath(path ? path : kDefaultBodyPath);

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, resolved))
    {
        Debug::warn(Debug::Category::NpcCombat,
                    "[BODY TEMPLATE] failed to load %s: %s\n",
                    resolved.c_str(), err.c_str());
        return false;
    }

    const size_t n = model.nodes.size();
    if (n == 0)
        return false;

    std::vector<glm::mat4> local(n, glm::mat4(1.0f));
    std::vector<int> parent(n, -1);
    for (size_t i = 0; i < n; ++i)
    {
        local[i] = nodeMatrix(model.nodes[i]);
        for (int child : model.nodes[i].children)
            if (child >= 0 && child < (int)n)
                parent[child] = (int)i;
    }

    // World transforms at the default pose (root-anchored at origin).
    std::vector<glm::mat4> world(n, glm::mat4(1.0f));
    for (size_t i = 0; i < n; ++i)
    {
        if (parent[i] >= 0)
            continue; // non-root; handled via traversal below
        std::vector<int> stack = {(int)i};
        while (!stack.empty())
        {
            const int idx = stack.back();
            stack.pop_back();
            world[idx] = (parent[idx] >= 0) ? world[parent[idx]] * local[idx] : local[idx];
            for (int child : model.nodes[idx].children)
                if (child >= 0 && child < (int)n)
                    stack.push_back(child);
        }
    }

    // Root position = world origin of the root node (model rooted at player.pos).
    glm::vec3 rootPos(0.0f);
    for (size_t i = 0; i < n; ++i)
    {
        if (parent[i] < 0)
        {
            rootPos = glm::vec3(world[i] * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            break;
        }
    }

    for (size_t i = 0; i < n; ++i)
    {
        const std::string& name = model.nodes[i].name;
        if (!isBodyPartName(name))
            continue;
        glm::vec3 mn, mx;
        if (!nodeMeshLocalAabb(model, model.nodes[i], mn, mx))
            continue;
        // Match the client's collideBeam exactly: center from the world
        // transform of the node-local center, half = node-local half (clamped).
        const glm::vec3 localCenter = (mn + mx) * 0.5f;
        const glm::vec3 half = glm::max((mx - mn) * 0.5f, glm::vec3(0.12f));
        const glm::vec3 worldCenter = glm::vec3(world[i] * glm::vec4(localCenter, 1.0f));

        ServerPlayerBodyPartTemplate t;
        t.offset = worldCenter - rootPos;
        t.half = half;
        if (name == "head")
            t.bodyPart = 1;
        else if (name.find("leg") != std::string::npos)
            t.bodyPart = 2;
        else
            t.bodyPart = 0;
        out.push_back(t);
    }

    Debug::warn(Debug::Category::NpcCombat,
                "[BODY TEMPLATE] loaded %zu body parts from %s\n",
                out.size(), resolved.c_str());
    return !out.empty();
}

} // namespace MimitaNet
