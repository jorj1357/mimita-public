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

extern void appendGLBMeshPrimitive(
    const tinygltf::Model& model,
    int meshIndex,
    int primitiveIndex,
    const tinygltf::Primitive& primitive,
    const glm::mat4& transform,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh
);

namespace {

bool glbVerbose()
{
    return DebugConfig::GLB_VERBOSE || std::getenv("MIMITA_GLB_VERBOSE") != nullptr;
}

bool validVec3(glm::vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool isSkyNode(const std::string& name)
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
            appendGLBMeshPrimitive(model, node.mesh, primIndex, gltfMesh.primitives[primIndex], world, materialTextures, target);
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

} // anonymous namespace

void walkGLBScene(
    const tinygltf::Model& model,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh,
    int sceneIndex,
    Mesh* skyMesh)
{
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
            {
                std::vector<GLuint> dummyMaterials;
                appendGLBMeshPrimitive(model, meshIndex, primIndex, gltfMesh.primitives[primIndex], glm::mat4(1.0f), materialTextures, mesh);
            }
        }
    }
}
