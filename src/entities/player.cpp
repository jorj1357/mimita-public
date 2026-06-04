// C:\important\quiet\n\mimita-priv-v7\src\entities\player.cpp
// feb 10 2026 CLEANED : slim + correct

#include "player.h"

#include <vector>
#include <string>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>

#include "physics/config.h"
#include "map/map_loader.h"
#include "tinygltf/tiny_gltf.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "audio/audio.h"
#include "utils/path_utils.h"
#include "debug/debug-log.h"

// globals (engine-level)
extern TextureStore gTextures;
extern Renderer* gRenderer;

// =====================================================
// Capsule render (debug/simple visual)
// =====================================================

static GLuint capsuleVAO = 0;
static GLuint capsuleVBO = 0;
static int    capsuleVertCount = 0;
static GLuint playerVAO = 0;
static GLuint playerVBO = 0;
static size_t playerUploadedVertCount = (size_t)-1;
static GLuint bodyPartVAO = 0;
static GLuint bodyPartVBO = 0;

namespace {

const char* PLAYER_GLB_PATH = "assets/entity/player/default/mimita-char-no-animations-v4.glb";

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

void uploadBodyPartMesh(const Mesh& mesh)
{
    if (!bodyPartVAO) glGenVertexArrays(1, &bodyPartVAO);
    if (!bodyPartVBO) glGenBuffers(1, &bodyPartVBO);

    glBindVertexArray(bodyPartVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bodyPartVBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
}

glm::mat4 poseMatrix(const ProceduralPose& pose)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pose.translation);
    m = glm::rotate(m, glm::radians(pose.rotationEuler.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.z), glm::vec3(0, 0, 1));
    return m;
}

glm::vec3 springVec3(SpringState& spring, const glm::vec3& target, float stiffness, float damping, float dt)
{
    float safeDt = std::min(dt, 0.05f);
    glm::vec3 acceleration = (target - spring.value) * stiffness - spring.velocity * damping;
    spring.velocity += acceleration * safeDt;
    spring.value += spring.velocity * safeDt;
    return spring.value;
}

void uploadPlayerMeshIfNeeded(const Mesh& mesh)
{
    if (playerVAO && playerUploadedVertCount == mesh.verts.size())
        return;

    if (!playerVAO) glGenVertexArrays(1, &playerVAO);
    if (!playerVBO) glGenBuffers(1, &playerVBO);

    glBindVertexArray(playerVAO);
    glBindBuffer(GL_ARRAY_BUFFER, playerVBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);

    playerUploadedVertCount = mesh.verts.size();
    printf("[PLAYER GLB] uploaded verts=%zu triangles=%zu batches=%zu\n",
           mesh.verts.size(), mesh.verts.size() / 3, mesh.batches.size());
}

}

static void initCapsuleMesh()
{
    if (capsuleVAO) return;

    struct V {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec3 normal;
    };

    constexpr int slices = 16;
    constexpr int stacks = 8;
    constexpr float PI = 3.1415926535f;

    float r = PLAYER_RADIUS;
    float h = PLAYER_HEIGHT;

    float cylinderHalf = h * 0.5f - r;

    std::vector<V> verts;

    // ===============================
    // CYLINDER
    // ===============================

    for (int i = 0; i < slices; i++)
    {
        float u0 = float(i) / slices;
        float u1 = float(i+1) / slices;

        float a0 = u0 * 2 * PI;
        float a1 = u1 * 2 * PI;

        glm::vec3 p0(r*cos(a0), r*sin(a0), -cylinderHalf);
        glm::vec3 p1(r*cos(a1), r*sin(a1), -cylinderHalf);
        glm::vec3 p2(r*cos(a0), r*sin(a0),  cylinderHalf);
        glm::vec3 p3(r*cos(a1), r*sin(a1),  cylinderHalf);

        verts.push_back({p0,{u0,0}, glm::normalize(glm::vec3(p0.x,p0.y,0.0f))});
        verts.push_back({p1,{u1,0}, glm::normalize(glm::vec3(p1.x,p1.y,0.0f))});
        verts.push_back({p2,{u0,1}, glm::normalize(glm::vec3(p2.x,p2.y,0.0f))});

        verts.push_back({p1,{u1,0}, glm::normalize(glm::vec3(p1.x,p1.y,0.0f))});
        verts.push_back({p3,{u1,1}, glm::normalize(glm::vec3(p3.x,p3.y,0.0f))});
        verts.push_back({p2,{u0,1}, glm::normalize(glm::vec3(p2.x,p2.y,0.0f))});
    }

    // ===============================
    // TOP HEMISPHERE
    // ===============================

    for (int j = 0; j < stacks; j++)
    {
        float v0 = float(j) / stacks;
        float v1 = float(j+1) / stacks;

        float phi0 = v0 * PI * 0.5f;
        float phi1 = v1 * PI * 0.5f;

        for (int i = 0; i < slices; i++)
        {
            float u0 = float(i) / slices;
            float u1 = float(i+1) / slices;

            float a0 = u0 * 2 * PI;
            float a1 = u1 * 2 * PI;

            glm::vec3 p0(
                r * cos(a0) * cos(phi0),
                r * sin(a0) * cos(phi0),
                r * sin(phi0) + cylinderHalf
            );

            glm::vec3 p1(
                r * cos(a1) * cos(phi0),
                r * sin(a1) * cos(phi0),
                r * sin(phi0) + cylinderHalf
            );

            glm::vec3 p2(
                r * cos(a0) * cos(phi1),
                r * sin(a0) * cos(phi1),
                r * sin(phi1) + cylinderHalf
            );

            glm::vec3 p3(
                r * cos(a1) * cos(phi1),
                r * sin(a1) * cos(phi1),
                r * sin(phi1) + cylinderHalf
            );

            verts.push_back({p0,{u0,v0}, glm::normalize(p0 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,cylinderHalf))});

            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p3,{u1,v1}, glm::normalize(p3 - glm::vec3(0,0,cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,cylinderHalf))});
        }
    }

    // ===============================
    // BOTTOM HEMISPHERE
    // ===============================

    for (int j = 0; j < stacks; j++)
    {
        float v0 = float(j) / stacks;
        float v1 = float(j+1) / stacks;

        float phi0 = v0 * PI * 0.5f;
        float phi1 = v1 * PI * 0.5f;

        for (int i = 0; i < slices; i++)
        {
            float u0 = float(i) / slices;
            float u1 = float(i+1) / slices;

            float a0 = u0 * 2 * PI;
            float a1 = u1 * 2 * PI;

            glm::vec3 p0(
                r * cos(a0) * cos(phi0),
                r * sin(a0) * cos(phi0),
                -r * sin(phi0) - cylinderHalf
            );

            glm::vec3 p1(
                r * cos(a1) * cos(phi0),
                r * sin(a1) * cos(phi0),
                -r * sin(phi0) - cylinderHalf
            );

            glm::vec3 p2(
                r * cos(a0) * cos(phi1),
                r * sin(a0) * cos(phi1),
                -r * sin(phi1) - cylinderHalf
            );

            glm::vec3 p3(
                r * cos(a1) * cos(phi1),
                r * sin(a1) * cos(phi1),
                -r * sin(phi1) - cylinderHalf
            );

            verts.push_back({p0,{u0,v0}, glm::normalize(p0 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,-cylinderHalf))});

            verts.push_back({p1,{u1,v0}, glm::normalize(p1 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p2,{u0,v1}, glm::normalize(p2 - glm::vec3(0,0,-cylinderHalf))});
            verts.push_back({p3,{u1,v1}, glm::normalize(p3 - glm::vec3(0,0,-cylinderHalf))});
        }
    }

    capsuleVertCount = (int)verts.size();

    glGenVertexArrays(1,&capsuleVAO);
    glGenBuffers(1,&capsuleVBO);

    glBindVertexArray(capsuleVAO);
    glBindBuffer(GL_ARRAY_BUFFER,capsuleVBO);

    glBufferData(GL_ARRAY_BUFFER,
        verts.size()*sizeof(V),
        verts.data(),
        GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,normal));
}

// =====================================================
// Player
// =====================================================

Player::Player()
{
    loadModel(PLAYER_GLB_PATH);
    reset();
}

void Player::reset()
{
    // pos = {0,0,50};
    // debug test
    // pos = {1,5,2};
    // debug test 2 for the ctf map
    pos = {1,5,30};
    vel = {0,0,0};
    dashVel = {0,0};
    onGround = false;

    // put this here so idk? mar 7 2026
    jumpHeldPrev = false;
    airJumpsLeft = 1;
    // dashCharges = DASH_MAX_CHARGES;
    groundReturnCharges = GROUND_RETURN_MAX_CHARGES;

    freezeTimer = 0.0f;
    freezeActive = false;
    freezeAvailable = true;
    freezeHeldPrev = false;
    freezeHoldSoundPlayed = false;

    previousProceduralVelocity = glm::vec3(0.0f);
    proceduralTime = 0.0f;

    for (BodyPart& part : bodyParts)
    {
        part.pose = ProceduralPose{};
        part.translationSpring = SpringState{};
        part.rotationSpring = SpringState{};
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

    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const tinygltf::Node& gltfNode = model.nodes[i];
        TransformNode& node = nodes[i];
        node.name = gltfNode.name;
        node.localTransform = nodeMatrix(gltfNode);
        restLocalTransforms[i] = node.localTransform;
        node.worldTransform = node.localTransform;
        node.children = gltfNode.children;

        for (int child : gltfNode.children)
            if (child >= 0 && child < (int)nodes.size())
                nodes[child].parent = i;
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

        Mesh bodyMesh;
        appendNodeRenderMesh(model, i, bodyMesh);
        bodyPartMeshes.push_back(bodyMesh);

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

void Player::updateModelWorldTransforms()
{
    glm::mat4 rootWorld =
        glm::translate(glm::mat4(1.0f), pos) *
        glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0,0,1));

    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        if (nodes[i].parent < 0)
            nodes[i].worldTransform = rootWorld * nodes[i].localTransform;
        else
            nodes[i].worldTransform = nodes[nodes[i].parent].worldTransform * nodes[i].localTransform;
    }
}

void Player::updateProceduralAnimation(float dt)
{
    if (nodes.empty() || restLocalTransforms.size() != nodes.size())
        return;

    proceduralTime += dt;

    glm::vec2 planarVel = glm::vec2(vel.x, vel.y) + dashVel;
    float speed = glm::length(planarVel);
    float move01 = std::min(speed / std::max(PHYS.moveSpeed, 0.001f), 1.6f);
    float dash01 = std::min(glm::length(dashVel) / std::max(DASH_IMPULSE, 0.001f), 1.0f);
    glm::vec3 acceleration = (dt > 0.0001f) ? (vel - previousProceduralVelocity) / dt : glm::vec3(0.0f);
    previousProceduralVelocity = vel;

    float phase = proceduralTime * (5.8f + move01 * 5.5f);
    float stride = std::sin(phase);
    float counterStride = std::sin(phase + 3.14159265f);
    float bob = std::abs(std::sin(phase)) * 0.055f * move01;
    float air = onGround ? 0.0f : 1.0f;
    float accelLean = std::clamp(acceleration.z * -0.03f, -8.0f, 8.0f);

    for (BodyPart& part : bodyParts)
    {
        if (part.nodeIndex < 0 || part.nodeIndex >= (int)nodes.size())
            continue;

        ProceduralPose target;

        if (part.name == "leftLeg")
        {
            target.rotationEuler.y = stride * 48.0f * move01 - air * 18.0f;
            target.rotationEuler.x = -air * 8.0f;
            target.translation.z = -bob * 0.35f;
        }
        else if (part.name == "rightLeg")
        {
            target.rotationEuler.y = counterStride * 48.0f * move01 - air * 18.0f;
            target.rotationEuler.x = -air * 8.0f;
            target.translation.z = -bob * 0.35f;
        }
        else if (part.name == "leftArm")
        {
            target.rotationEuler.y = counterStride * 58.0f * move01 + air * 14.0f;
            target.rotationEuler.z = 8.0f * move01;
        }
        else if (part.name == "rightArm")
        {
            target.rotationEuler.y = stride * 58.0f * move01 + air * 14.0f;
            target.rotationEuler.z = -8.0f * move01;
        }
        else if (part.name == "torso")
        {
            target.translation.z = bob;
            target.rotationEuler.x = -6.0f * move01 - 10.0f * dash01 + accelLean;
            target.rotationEuler.z = std::clamp(dashVel.x * -0.18f + dashVel.y * 0.08f, -8.0f, 8.0f);
        }
        else if (part.name == "head")
        {
            target.translation.z = bob * 0.35f;
            target.rotationEuler.x = 3.0f * move01 + 5.0f * dash01 - accelLean * 0.5f;
            target.rotationEuler.z = std::clamp(dashVel.x * 0.08f - dashVel.y * 0.04f, -4.0f, 4.0f);
        }

        if (!onGround)
            target.translation.z += (part.name == "torso" || part.name == "head") ? 0.015f : 0.0f;

        part.pose.translation = springVec3(part.translationSpring, target.translation, 90.0f, 16.0f, dt);
        part.pose.rotationEuler = springVec3(part.rotationSpring, target.rotationEuler, 80.0f, 14.0f, dt);

        nodes[part.nodeIndex].localTransform =
            restLocalTransforms[part.nodeIndex] * poseMatrix(part.pose);
    }

    updateModelWorldTransforms();
}

Capsule Player::getCapsule() const
{
    Capsule c;
    c.r = PLAYER_RADIUS;

    float half = PLAYER_HEIGHT * 0.5f;
    c.a = pos - glm::vec3(0,0,half - c.r);
    c.b = pos + glm::vec3(0,0,half - c.r);

    return c;
}

OBB Player::getOBB() const
{
    OBB b;
    b.center = pos;
    b.halfSize = glm::vec3(PLAYER_WIDTH,PLAYER_DEPTH,PLAYER_HEIGHT) * 0.5f;
    b.orientation = glm::rotate(glm::mat4(1.0f),
                                glm::radians(-yaw),
                                glm::vec3(0,0,1));
    return b;
}

void Player::updateAudio(float dt)
{
    // simple and works but annoying  air jump 
    if (didGroundJump) playEventSound("entity/player/jump",1.0f);
    // if (didAirJump)    playSound("entity/player/doublejump",1.0f);

    // with 0.5 sec wait time from audio.cpp 
    if (didAirJump)
        playAirJumpSound();

    // // testing this so that we stop spamming air jump
    // if (didGroundJump && !jumpHeldPrev)
    //     playSound("entity/player/jump",1.0f);

    // // this one spams so much we attempt fix 1 mar 7 2026 
    // if (didAirJump && !jumpHeldPrev)
    //     playSound("entity/player/doublejump",1.0f);
    
    if (didDash)       playEventSound("entity/player/dash",1.0f);
    if (didLand)       playEventSound("entity/player/land",1.0f);

    glm::vec2 xy = glm::vec2(vel.x,vel.y) + dashVel;
    // if (onGround && glm::length(xy) > 0.1f) {
    // > 0.1f was old, mar 8 2026
    // setting it to be 0.5f so i stop playing the sound so much
    if (onGround && glm::length(xy) > 0.5f) {
        footstepTimer -= dt;
        if (footstepTimer <= 0.0f) {
            playRandomFootstep();
            footstepTimer = 0.35f;
        }
    } else {
        footstepTimer = 0.0f;
    }

    didGroundJump = didAirJump = didDash = didLand = false;
}

void Player::render(unsigned int shader,
                    const glm::mat4& view,
                    const glm::mat4& proj) const
{
    const_cast<Player*>(this)->updateModelWorldTransforms();

    if (modelLoaded && !bodyParts.empty() && bodyPartMeshes.size() == bodyParts.size())
    {
        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
        glUniform1i(glGetUniformLocation(shader,"uUseColor"),0);
        glUniform1i(glGetUniformLocation(shader,"uTex"),0);

        glActiveTexture(GL_TEXTURE0);
        for (size_t i = 0; i < bodyParts.size(); ++i)
        {
            const BodyPart& part = bodyParts[i];
            const Mesh& mesh = bodyPartMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)nodes.size() || mesh.verts.empty())
                continue;

            uploadBodyPartMesh(mesh);

            const glm::mat4& model = nodes[part.nodeIndex].worldTransform;
            glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);

            for (const Mesh::Batch& batch : mesh.batches)
            {
                glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default"));
                glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
            }
        }
        return;
    }

    if (modelLoaded && !renderMesh.verts.empty())
    {
        uploadPlayerMeshIfNeeded(renderMesh);

        glm::mat4 model =
            glm::translate(glm::mat4(1.0f), pos) *
            glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0,0,1));

        glUseProgram(shader);
        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
        glUniform1i(glGetUniformLocation(shader,"uUseColor"),0);
        glUniform1i(glGetUniformLocation(shader,"uTex"),0);

        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(playerVAO);
        for (const Mesh::Batch& batch : renderMesh.batches)
        {
            glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default"));
            glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count);
        }
        return;
    }

    initCapsuleMesh();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
    glUniform1i(glGetUniformLocation(shader,"uUseColor"),0);
    glUniform1i(glGetUniformLocation(shader,"uTex"),0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gTextures.get("greenwirev1"));

    glBindVertexArray(capsuleVAO);
    glDrawArrays(GL_TRIANGLES, 0, capsuleVertCount);
}
