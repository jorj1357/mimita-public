// 2026-09-30
/* purpose
* Implement the GL-free GLB -> destructible BooleanMesh import. This file reuses
* the shared GLB accessor helpers (map-loader-gltf-accessors.h) and the asset
* path resolver instead of reimplementing binary parsing. Manifold is reached
* only through the boolean wrapper for validation.
* Does NOT upload textures or touch GL, own cut history, or render.
*/

#include "impact/destructible-mesh-loader.h"

#include <cfloat>
#include <cmath>
#include <mutex>
#include <unordered_map>

#include <tinygltf/tiny_gltf.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "map/map-loader-gltf-accessors.h"
#include "utils/path_utils.h"

namespace MimitaImpact {
namespace {

constexpr int kMaxNodeDepth = 64;

glm::mat4 nodeLocalMatrix(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16)
    {
        glm::mat4 m(1.0f);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                m[c][r] = (float)node.matrix[c * 4 + r];
        return m;
    }

    glm::mat4 m(1.0f);
    if (node.translation.size() == 3)
        m = glm::translate(m, glm::vec3((float)node.translation[0],
                                        (float)node.translation[1],
                                        (float)node.translation[2]));
    if (node.rotation.size() == 4)
    {
        const glm::quat q((float)node.rotation[3], (float)node.rotation[0],
                          (float)node.rotation[1], (float)node.rotation[2]);
        m = m * glm::mat4_cast(q);
    }
    if (node.scale.size() == 3)
        m = glm::scale(m, glm::vec3((float)node.scale[0], (float)node.scale[1],
                                    (float)node.scale[2]));
    return m;
}

void appendNode(const tinygltf::Model& model, int nodeIndex,
                const glm::mat4& parent, BooleanMesh& out, int depth)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size() ||
        depth > kMaxNodeDepth)
        return;

    const tinygltf::Node& node = model.nodes[nodeIndex];
    const glm::mat4 world = parent * nodeLocalMatrix(node);

    if (node.mesh >= 0 && node.mesh < (int)model.meshes.size())
    {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const tinygltf::Primitive& prim : mesh.primitives)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES)
                continue;
            const auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end())
                continue;
            const int posIndex = posIt->second;
            if (posIndex < 0 || posIndex >= (int)model.accessors.size())
                continue;
            const tinygltf::Accessor& posAcc = model.accessors[posIndex];

            const tinygltf::Accessor* uvAcc = nullptr;
            const auto uvIt = prim.attributes.find("TEXCOORD_0");
            if (uvIt != prim.attributes.end() && uvIt->second >= 0 &&
                uvIt->second < (int)model.accessors.size())
                uvAcc = &model.accessors[uvIt->second];

            const uint32_t materialId =
                prim.material >= 0 ? (uint32_t)prim.material : 0u;

            auto corner = [&](unsigned vi) {
                BooleanMeshVertex v;
                const glm::vec3 local = readVec3(model, posAcc, vi, glm::vec3(0.0f));
                v.position = glm::vec3(world * glm::vec4(local, 1.0f));
                v.uv = uvAcc ? readVec2(model, *uvAcc, vi, glm::vec2(0.0f))
                             : glm::vec2(0.0f);
                v.materialId = materialId;
                return v;
            };
            auto emit = [&](unsigned a, unsigned b, unsigned c) {
                BooleanMeshVertex v0 = corner(a);
                BooleanMeshVertex v1 = corner(b);
                BooleanMeshVertex v2 = corner(c);
                glm::vec3 n = glm::cross(v1.position - v0.position,
                                         v2.position - v0.position);
                const float len = glm::length(n);
                if (len <= 1e-12f)
                    return;
                n /= len;
                v0.normal = v1.normal = v2.normal = n;
                const uint32_t base = (uint32_t)out.vertices.size();
                out.vertices.push_back(v0);
                out.vertices.push_back(v1);
                out.vertices.push_back(v2);
                out.indices.push_back(base);
                out.indices.push_back(base + 1u);
                out.indices.push_back(base + 2u);
            };

            if (prim.indices >= 0 && prim.indices < (int)model.accessors.size())
            {
                const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
                for (size_t i = 0; i + 2 < idxAcc.count; i += 3)
                {
                    unsigned a = 0, b = 0, c = 0;
                    if (readIndex(model, idxAcc, i, a) &&
                        readIndex(model, idxAcc, i + 1, b) &&
                        readIndex(model, idxAcc, i + 2, c))
                        emit(a, b, c);
                }
            }
            else
            {
                for (size_t i = 0; i + 2 < posAcc.count; i += 3)
                    emit((unsigned)i, (unsigned)(i + 1), (unsigned)(i + 2));
            }
        }
    }

    for (int child : node.children)
        appendNode(model, child, world, out, depth + 1);
}

double signedVolume(const BooleanMesh& mesh)
{
    double volume = 0.0;
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const glm::dvec3 a(mesh.vertices[mesh.indices[i]].position);
        const glm::dvec3 b(mesh.vertices[mesh.indices[i + 1]].position);
        const glm::dvec3 c(mesh.vertices[mesh.indices[i + 2]].position);
        volume += glm::dot(a, glm::cross(b, c)) / 6.0;
    }
    return volume;
}

void flipWinding(BooleanMesh& mesh)
{
    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        std::swap(mesh.indices[i + 1], mesh.indices[i + 2]);
    for (BooleanMeshVertex& v : mesh.vertices)
        v.normal = -v.normal;
}

bool recenterToAabb(BooleanMesh& mesh, glm::vec3& outHalfExtents)
{
    if (mesh.vertices.empty())
        return false;
    glm::vec3 mn(FLT_MAX);
    glm::vec3 mx(-FLT_MAX);
    for (const BooleanMeshVertex& v : mesh.vertices)
    {
        mn = glm::min(mn, v.position);
        mx = glm::max(mx, v.position);
    }
    const glm::vec3 center = (mn + mx) * 0.5f;
    for (BooleanMeshVertex& v : mesh.vertices)
        v.position -= center;
    outHalfExtents = glm::max((mx - mn) * 0.5f, glm::vec3(1e-3f));
    return true;
}

std::mutex gCacheMutex;
std::unordered_map<std::string, DestructibleMeshLoad> gCache;

} // namespace

DestructibleMeshLoad loadDestructibleMeshFromGLB(const std::string& path)
{
    DestructibleMeshLoad result;
    if (path.empty())
    {
        result.error = "empty model path";
        return result;
    }

    std::lock_guard<std::mutex> lock(gCacheMutex);
    const std::string resolved = resolveAssetPath(path);
    const auto cached = gCache.find(resolved);
    if (cached != gCache.end())
        return cached->second;

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string errors;
    std::string warnings;
    if (!loader.LoadBinaryFromFile(&model, &errors, &warnings, resolved))
    {
        result.error = errors.empty() ? "GLB parse failed" : errors;
        gCache[resolved] = result;
        return result;
    }

    BooleanMesh mesh;
    const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
    {
        for (int node : model.scenes[sceneIndex].nodes)
            appendNode(model, node, glm::mat4(1.0f), mesh, 0);
    }
    else
    {
        for (int i = 0; i < (int)model.nodes.size(); ++i)
            appendNode(model, i, glm::mat4(1.0f), mesh, 0);
    }

    if (mesh.indices.empty())
    {
        result.error = "GLB contains no triangles";
        gCache[resolved] = result;
        return result;
    }

    // A mesh authored inside-out still validates as a manifold but would invert
    // the boolean, so make the winding consistently outward first.
    if (signedVolume(mesh) < 0.0)
        flipWinding(mesh);

    if (!recenterToAabb(mesh, result.halfExtents))
    {
        result.error = "GLB mesh has no vertices";
        gCache[resolved] = result;
        return result;
    }

    std::string reason;
    const BooleanError valid = booleanValidate(mesh, &reason);
    if (valid != BooleanError::None)
    {
        result.error = "not a closed manifold (" + reason + ")";
        gCache[resolved] = result;
        return result;
    }

    result.success = true;
    result.mesh = std::move(mesh);
    gCache[resolved] = result;
    return result;
}

} // namespace MimitaImpact