#include "player.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "debug/debug-log.h"
#include "map/map_loader.h"
#include "tinygltf/tiny_gltf.h"
#include "utils/path_utils.h"
#include "world/texture-store.h"

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

    glm::quat r(1, 0, 0, 0);
    if (node.rotation.size() == 4)
        r = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);

    glm::vec3 s(1.0f);
    if (node.scale.size() == 3)
        s = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};

    return glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
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

glm::vec3 readVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index)
{
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
        return glm::vec3(0.0f);
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    return {f[0], f[1], f[2]};
}

glm::vec2 readVec2(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index)
{
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2)
        return glm::vec2(0.0f);
    const unsigned char* base = accessorPtr(model, accessor);
    const float* f = reinterpret_cast<const float*>(base + index * accessorStride(model, accessor));
    return {f[0], f[1]};
}

unsigned int readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t i)
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

bool isPlayerBodyPart(const std::string& name)
{
    return name == "head" || name == "torso" ||
           name == "leftArm" || name == "rightArm" ||
           name == "leftLeg" || name == "rightLeg";
}

void appendNodeCollider(
    const tinygltf::Model& model,
    int nodeIndex,
    Collider& collider
) {
    const tinygltf::Node& node = model.nodes[nodeIndex];
    if (node.mesh < 0 || node.mesh >= (int)model.meshes.size())
        return;

    bool boundsSet = false;
    const tinygltf::Mesh& mesh = model.meshes[node.mesh];
    for (const tinygltf::Primitive& primitive : mesh.primitives)
    {
        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end())
            continue;

        const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
        std::vector<glm::vec3> positions;
        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            positions.reserve(indexAccessor.count);
            for (size_t i = 0; i < indexAccessor.count; ++i)
                positions.push_back(readVec3(model, posAccessor, readIndex(model, indexAccessor, i)));
        }
        else
        {
            positions.reserve(posAccessor.count);
            for (size_t i = 0; i < posAccessor.count; ++i)
                positions.push_back(readVec3(model, posAccessor, i));
        }

        for (size_t i = 0; i + 2 < positions.size(); i += 3)
        {
            CollisionTriangle tri;
            tri.a = positions[i + 0];
            tri.b = positions[i + 1];
            tri.c = positions[i + 2];
            glm::vec3 n = glm::cross(tri.b - tri.a, tri.c - tri.a);
            float len = glm::length(n);
            if (len < 0.000001f)
                continue;
            tri.normal = n / len;
            collider.triangles.push_back(tri);

            auto addSample = [&](glm::vec3 p) {
                for (glm::vec3 existing : collider.samplePoints)
                    if (glm::length(existing - p) < 0.0001f)
                        return;
                collider.samplePoints.push_back(p);
            };
            addSample(tri.a);
            addSample(tri.b);
            addSample(tri.c);

            glm::vec3 mn = glm::min(glm::min(tri.a, tri.b), tri.c);
            glm::vec3 mx = glm::max(glm::max(tri.a, tri.b), tri.c);
            if (!boundsSet)
            {
                collider.localMin = mn;
                collider.localMax = mx;
                boundsSet = true;
            }
            else
            {
                collider.localMin = glm::min(collider.localMin, mn);
                collider.localMax = glm::max(collider.localMax, mx);
            }
        }
    }
}

void appendNodeRenderMesh(
    const tinygltf::Model& model,
    int nodeIndex,
    Mesh& out
) {
    const tinygltf::Node& node = model.nodes[nodeIndex];
    if (node.mesh < 0 || node.mesh >= (int)model.meshes.size())
        return;

    const tinygltf::Mesh& mesh = model.meshes[node.mesh];
    for (const tinygltf::Primitive& primitive : mesh.primitives)
    {
        if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
            continue;

        auto posIt = primitive.attributes.find("POSITION");
        if (posIt == primitive.attributes.end())
            continue;

        const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
        const tinygltf::Accessor* normalAccessor = nullptr;
        const tinygltf::Accessor* uvAccessor = nullptr;

        auto normalIt = primitive.attributes.find("NORMAL");
        if (normalIt != primitive.attributes.end())
            normalAccessor = &model.accessors[normalIt->second];

        auto uvIt = primitive.attributes.find("TEXCOORD_0");
        if (uvIt != primitive.attributes.end())
            uvAccessor = &model.accessors[uvIt->second];

        Mesh::Batch batch;
        batch.materialIndex = primitive.material;
        batch.materialName = "player_body";
        batch.texture = gTextures.get("default");
        batch.first = out.verts.size();

        auto pushVertex = [&](size_t vertexIndex) {
            Vertex v{};
            v.pos = readVec3(model, posAccessor, vertexIndex);
            v.normal = normalAccessor ? readVec3(model, *normalAccessor, vertexIndex) : glm::vec3(0.0f, 0.0f, 1.0f);
            v.uv = uvAccessor ? readVec2(model, *uvAccessor, vertexIndex) : glm::vec2(0.0f);
            out.verts.push_back(v);
        };

        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            for (size_t i = 0; i < indexAccessor.count; ++i)
            {
                unsigned int vertexIndex = readIndex(model, indexAccessor, i);
                if (vertexIndex < posAccessor.count)
                    pushVertex(vertexIndex);
            }
        }
        else
        {
            for (size_t i = 0; i < posAccessor.count; ++i)
                pushVertex(i);
        }

        batch.count = out.verts.size() - batch.first;
        if (batch.count > 0)
            out.batches.push_back(batch);
    }
}

bool Player::loadModel(const char* path)
{
    renderMesh = loadGLB(path);
    modelLoaded = !renderMesh.verts.empty();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    std::string resolvedPath = resolveAssetPath(path);
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, resolvedPath);

    if (!warn.empty()) printf("[PLAYER GLB WARNING] %s\n", warn.c_str());
    if (!err.empty()) printf("[PLAYER GLB ERROR] %s\n", err.c_str());
    if (!ok)
    {
        printf("[PLAYER GLB ERROR] failed hierarchy load %s\n", resolvedPath.c_str());
        return modelLoaded;
    }

    nodes.clear();
    nodes.resize(model.nodes.size());
    restLocalTransforms.clear();
    restLocalTransforms.resize(model.nodes.size(), glm::mat4(1.0f));
    bodyColliders.clear();
    bodyParts.clear();
    bodyPartMeshes.clear();
    perfectPoseSkeleton.nodes.clear();
    perfectPoseSkeleton.nodes.resize(model.nodes.size());
    perfectPoseSkeleton.restLocalTransforms.clear();
    perfectPoseSkeleton.restLocalTransforms.resize(model.nodes.size(), glm::mat4(1.0f));
    physicalBody.parts.clear();
    physicalBody.partMeshes.clear();

    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const tinygltf::Node& gltfNode = model.nodes[i];
        TransformNode& node = nodes[i];
        node.name = gltfNode.name;
        node.localTransform = nodeMatrix(gltfNode);
        restLocalTransforms[i] = node.localTransform;
        node.worldTransform = node.localTransform;
        node.children = gltfNode.children;

        TransformNode& poseNode = perfectPoseSkeleton.nodes[i];
        poseNode.name = gltfNode.name;
        poseNode.localTransform = node.localTransform;
        poseNode.worldTransform = node.worldTransform;
        poseNode.children = node.children;
        perfectPoseSkeleton.restLocalTransforms[i] = node.localTransform;

        for (int child : gltfNode.children)
            if (child >= 0 && child < (int)nodes.size())
            {
                nodes[child].parent = i;
                perfectPoseSkeleton.nodes[child].parent = i;
            }
    }

    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const std::string& name = nodes[i].name;
        if (!isPlayerBodyPart(name))
            continue;

        Collider collider;
        collider.name = name;
        appendNodeCollider(model, i, collider);
        bodyColliders.push_back(collider);

        BodyPart part;
        part.name = name;
        part.nodeIndex = i;
        part.collider = collider;
        bodyParts.push_back(part);

        PhysicalBodyPart physicalPart;
        physicalPart.name = name;
        physicalPart.nodeIndex = i;
        physicalPart.collider = collider;
        physicalBody.parts.push_back(physicalPart);

        Mesh bodyMesh;
        appendNodeRenderMesh(model, i, bodyMesh);
        bodyPartMeshes.push_back(bodyMesh);
        physicalBody.partMeshes.push_back(bodyMesh);

        Debug::log(
            Debug::Category::GLB,
            "[PLAYER GLB] body collider=%s localTriangles=%zu localVerts=%zu localMin=(%.2f %.2f %.2f) localMax=(%.2f %.2f %.2f)\n",
            collider.name.c_str(),
            collider.triangles.size(),
            bodyMesh.verts.size(),
            collider.localMin.x, collider.localMin.y, collider.localMin.z,
            collider.localMax.x, collider.localMax.y, collider.localMax.z
        );
    }

    updateModelWorldTransforms();
    printf("[PLAYER GLB] hierarchy nodes=%zu bodyColliders=%zu root=plrOrigin expected\n",
           nodes.size(), bodyColliders.size());
    return modelLoaded;
}
