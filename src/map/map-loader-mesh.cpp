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

namespace {

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
                appendPrimitive(model, meshIndex, primIndex, gltfMesh.primitives[primIndex], glm::mat4(1.0f), materialTextures, mesh);
            }
        }
    }
}
