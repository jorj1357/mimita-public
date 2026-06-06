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
#include "debug/gl-debug.h"
#include "effects/effect-part.h"

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
    MIMITA_GL_CLEAR_STAGE("uploadBodyPartMesh");
    if (!bodyPartVAO) MIMITA_GL_CALL(glGenVertexArrays(1, &bodyPartVAO));
    if (!bodyPartVBO) MIMITA_GL_CALL(glGenBuffers(1, &bodyPartVBO));

    MIMITA_GL_CALL(glBindVertexArray(bodyPartVAO));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, bodyPartVBO));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_DYNAMIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));

    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));
}

glm::mat4 poseMatrix(const ProceduralPose& pose)
{
    glm::mat4 m = glm::translate(glm::mat4(1.0f), pose.translation);
    m = glm::rotate(m, glm::radians(pose.rotationEuler.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(pose.rotationEuler.z), glm::vec3(0, 0, 1));
    return m;
}

glm::quat yawRotation(float yawDegrees)
{
    return glm::angleAxis(glm::radians(yawDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
}

glm::mat4 transformMatrix(const glm::vec3& position, const glm::quat& rotation)
{
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
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

    MIMITA_GL_CLEAR_STAGE("uploadPlayerMeshIfNeeded");
    if (!playerVAO) MIMITA_GL_CALL(glGenVertexArrays(1, &playerVAO));
    if (!playerVBO) MIMITA_GL_CALL(glGenBuffers(1, &playerVBO));

    MIMITA_GL_CALL(glBindVertexArray(playerVAO));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, playerVBO));
    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER, mesh.verts.size() * sizeof(Vertex), mesh.verts.data(), GL_STATIC_DRAW));

    MIMITA_GL_CALL(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(0));

    MIMITA_GL_CALL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(1));

    MIMITA_GL_CALL(glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal)));
    MIMITA_GL_CALL(glEnableVertexAttribArray(2));

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

    MIMITA_GL_CLEAR_STAGE("initCapsuleMesh");
    MIMITA_GL_CALL(glGenVertexArrays(1,&capsuleVAO));
    MIMITA_GL_CALL(glGenBuffers(1,&capsuleVBO));

    MIMITA_GL_CALL(glBindVertexArray(capsuleVAO));
    MIMITA_GL_CALL(glBindBuffer(GL_ARRAY_BUFFER,capsuleVBO));

    MIMITA_GL_CALL(glBufferData(GL_ARRAY_BUFFER,
        verts.size()*sizeof(V),
        verts.data(),
        GL_STATIC_DRAW));

    MIMITA_GL_CALL(glEnableVertexAttribArray(0));
    MIMITA_GL_CALL(glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0));

    MIMITA_GL_CALL(glEnableVertexAttribArray(1));
    MIMITA_GL_CALL(glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv)));

    MIMITA_GL_CALL(glEnableVertexAttribArray(2));
    MIMITA_GL_CALL(glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,normal)));
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
    // pos = {1,5,30};
    pos = {1,5,60};
    vel = {0,0,0};
    dashVel = {0,0};
    externalImpulse = {0,0,0};
    dashLockedDirection = {0,0};
    onGround = false;

    // put this here so idk? mar 7 2026
    jumpHeldPrev = false;
    moveHeldPrev = false;
    dashHeldPrev = false;
    jumpIntentTimer = 0.0f;
    coyoteTimer = 0.0f;
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

    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        part.pose = ProceduralPose{};
        part.translationSpring = SpringState{};
        part.rotationSpring = SpringState{};
    }

    syncLegacyStateToLayers();
    updateModelWorldTransforms();
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

void Player::syncLegacyStateToLayers()
{
    origin.position = pos;
    origin.rotation = yawRotation(yaw);

    movementCapsule.position = origin.position;
    movementCapsule.rotation = origin.rotation;
    movementCapsule.velocity = vel;
    movementCapsule.dashVelocity = dashVel;
    movementCapsule.radius = PLAYER_RADIUS;
    movementCapsule.height = PLAYER_HEIGHT;
    movementCapsule.onGround = onGround;
}

void Player::syncLayersToLegacyState()
{
    pos = origin.position;
    vel = movementCapsule.velocity;
    dashVel = movementCapsule.dashVelocity;
    onGround = movementCapsule.onGround;
}

void Player::updateModelWorldTransforms()
{
    syncLegacyStateToLayers();

    glm::mat4 rootWorld = transformMatrix(movementCapsule.position, movementCapsule.rotation);

    for (int i = 0; i < (int)perfectPoseSkeleton.nodes.size(); ++i)
    {
        TransformNode& poseNode = perfectPoseSkeleton.nodes[i];
        if (poseNode.parent < 0)
            poseNode.worldTransform = rootWorld * poseNode.localTransform;
        else
            poseNode.worldTransform = perfectPoseSkeleton.nodes[poseNode.parent].worldTransform * poseNode.localTransform;

        if (i < (int)nodes.size())
        {
            nodes[i].localTransform = poseNode.localTransform;
            nodes[i].worldTransform = poseNode.worldTransform;
        }
    }

    for (PhysicalBodyPart& part : physicalBody.parts)
    {
        if (part.nodeIndex >= 0 && part.nodeIndex < (int)perfectPoseSkeleton.nodes.size())
            part.worldTransform = perfectPoseSkeleton.nodes[part.nodeIndex].worldTransform;
    }
}

void Player::updateProceduralAnimation(float dt, const glm::vec3& camForward, const glm::vec3& camPos)
{
    if (perfectPoseSkeleton.nodes.empty() ||
        perfectPoseSkeleton.restLocalTransforms.size() != perfectPoseSkeleton.nodes.size())
        return;

    // Store aim data for weapon positioning
    if (glm::length(camForward) > 0.001f) {
        aimDirection = glm::normalize(camForward);
        aimPosition = camPos;
        hasAimData = true;
    }

    syncLegacyStateToLayers();

    proceduralTime += dt;
    weaponSwayTime += dt;

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

    // Weapon sway (applied to arms when weapon equipped)
    bool weaponEquipped = (equippedSlot == 1);
    float swayAmount = weaponEquipped ? 0.15f + move01 * 0.1f : 0.0f;
    float swayPhase = weaponSwayTime * 8.0f;
    float swayX = std::sin(swayPhase) * swayAmount;
    float swayY = std::cos(swayPhase * 1.3f) * swayAmount * 0.6f;
    float swayZ = std::sin(swayPhase * 0.7f) * swayAmount * 0.4f;

    // Upper body aim rotation (torso + arms rotate toward aim direction)
    float aimYaw = 0.0f;
    float aimPitch = 0.0f;
    if (hasAimData && weaponEquipped) {
        glm::vec3 flatAim = glm::normalize(glm::vec3(aimDirection.x, aimDirection.y, 0.0f));
        glm::vec3 flatForward = glm::normalize(glm::vec3(movementCapsule.rotation * glm::vec3(0,1,0)));
        // Only yaw in XY plane
        aimYaw = std::atan2(flatAim.y, flatAim.x) - std::atan2(flatForward.y, flatForward.x);
        // Normalize to -PI..PI
        while (aimYaw > 3.14159265f) aimYaw -= 2.0f * 3.14159265f;
        while (aimYaw < -3.14159265f) aimYaw += 2.0f * 3.14159265f;
        aimYaw = std::clamp(aimYaw, -1.0f, 1.0f); // Limit to ~57 degrees
        
        // Pitch for up/down aim
        aimPitch = std::asin(std::clamp(aimDirection.z, -1.0f, 1.0f));
        aimPitch = std::clamp(aimPitch, -0.8f, 0.8f); // Limit pitch
    }

    for (PhysicalBodyPart& part : physicalBody.parts)
{
    if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size())
        continue;

    ProceduralPose target;

        if (part.name == "leftLeg")
        {
            // legs already work correctly with local Z swing
            // target.rotationEuler.y = stride * 48.0f * move01 - air * 18.0f;

            target.rotationEuler.z = stride * 48.0f * move01 - air * 18.0f;

            // slight air tuck
            target.rotationEuler.x = -air * 8.0f;

            // tiny vertical compression while walking
            target.translation.z = -bob * 0.35f;
        }
        else if (part.name == "rightLeg")
        {
            // target.rotationEuler.y = counterStride * 48.0f * move01 - air * 18.0f;

            target.rotationEuler.z = counterStride * 48.0f * move01 - air * 18.0f;

            target.rotationEuler.x = -air * 8.0f;

            target.translation.z = -bob * 0.35f;
        }
        else if (part.name == "leftArm")
        {
            // IMPORTANT:
            // arm local axes are different from legs
            // in this rig X is the proper swing axis

            // old tests:
            // target.rotationEuler.y = counterStride * 58.0f * move01 + air * 14.0f;
            // target.rotationEuler.z = counterStride * 58.0f * move01 + air * 14.0f;
            // target.rotationEuler.x = counterStride * 108.0f * move01 + air * 14.0f;
            // target.rotationEuler.y = counterStride * 108.0f * move01 + air * 14.0f;

            // forward/back arm swing
            // target.rotationEuler.x =
            // 6 4  2026
            // attemtp 2 lalala 
            // ok this rotates like the shoulder like wagging ur finger left right is what it does
            // attempt 3  z
            // target.rotationEuler.y =
            target.rotationEuler.z =
                counterStride * 58.0f * move01 +
                air * 14.0f;

            // tiny outward shoulder angle
            target.rotationEuler.z += 8.0f * move01;

            // Weapon hold pose (left hand supporting)
            if (weaponEquipped) {
                // Raise arm to hold weapon
                target.rotationEuler.z += -45.0f; // Raise up
                target.rotationEuler.x += -20.0f; // Bring forward
                target.rotationEuler.y += 15.0f;  // Rotate inward
                target.translation.z += 0.15f;    // Lift slightly
                
                // Weapon sway on left arm (supporting hand)
                target.rotationEuler.z += swayZ * 0.5f;
                target.rotationEuler.x += swayX * 0.3f;
                target.rotationEuler.y += swayY * 0.3f;
            }
        }
        else if (part.name == "rightArm")
        {
            // old tests:
            // target.rotationEuler.y = stride * 58.0f * move01 + air * 14.0f;
            // target.rotationEuler.z = stride * 58.0f * move01 + air * 14.0f;
            // target.rotationEuler.x = stride * 108.0f * move01 + air * 14.0f;

            // x = proper arm swing axis on this rig
            // y = twisting shoulder
            // z = side lean / flare

            // target.rotationEuler.y = stride * 108.0f * move01 + air * 14.0f;

            // forward/back arm swing
            // target.rotationEuler.x =
            // test 2  
            // 6 4 2026
            // bc x swings left right across the torso 
            // target.rotationEuler.y =
            target.rotationEuler.z =
                stride * 58.0f * move01 +
                air * 14.0f;

            // tiny outward shoulder angle
            target.rotationEuler.z += -8.0f * move01;

            // Weapon hold pose (right hand - primary grip)
            if (weaponEquipped) {
                // Raise arm to hold weapon forward
                target.rotationEuler.z += -55.0f; // Raise up
                target.rotationEuler.x += -25.0f; // Bring forward
                target.rotationEuler.y += -10.0f; // Rotate outward slightly
                target.translation.z += 0.18f;    // Lift slightly
                target.translation.x += 0.08f;    // Offset to side
                
                // Weapon sway on right arm (trigger hand)
                target.rotationEuler.z += swayZ;
                target.rotationEuler.x += swayX * 0.5f;
                target.rotationEuler.y += swayY * 0.5f;
            }
        }
        else if (part.name == "torso")
        {
            target.translation.z = bob;

            // old:
            // target.rotationEuler.x = -6.0f * move01 - 10.0f * dash01 + accelLean;

            // torso forward lean
            // target.rotationEuler.x =
            target.rotationEuler.z =
                -6.0f * move01 -
                // -60.0f * move01 -
                10.0f * dash01 +
                accelLean;

            // torso side tilt from dash
            target.rotationEuler.z +=
                std::clamp(
                    dashVel.x * -0.18f +
                    dashVel.y * 0.08f,
                    -8.0f,
                    8.0f
                );

            // Upper body aim rotation (yaw toward aim direction)
            if (weaponEquipped && hasAimData) {
                // Convert aimYaw from radians to degrees for rotationEuler
                target.rotationEuler.y += aimYaw * 57.29578f * 0.6f; // 60% of aim yaw applied to torso
                target.rotationEuler.x += aimPitch * 57.29578f * 0.3f; // 30% of pitch to torso
            }
        }
        else if (part.name == "head")
        {
            target.translation.z = bob * 0.35f;

            // old:
            // target.rotationEuler.x = 3.0f * move01 + 5.0f * dash01 - accelLean * 0.5f;

            // slight counterbalance to torso
            // target.rotationEuler.x =
            target.rotationEuler.z =
                3.0f * move01 +
                // 30.0f * move01 +
                5.0f * dash01 -
                accelLean * 0.5f;

            // subtle side stabilization
            target.rotationEuler.z +=
                std::clamp(
                    dashVel.x * 0.08f -
                    dashVel.y * 0.04f,
                    -4.0f,
                    4.0f
                );
        }

        if (!onGround)
        {
            target.translation.z +=
                (part.name == "torso" || part.name == "head")
                ? 0.015f
                : 0.0f;
        }

        part.pose.translation =
            springVec3(
                part.translationSpring,
                target.translation,
                90.0f,
                16.0f,
                dt
            );

        part.pose.rotationEuler =
            springVec3(
                part.rotationSpring,
                target.rotationEuler,
                80.0f,
                14.0f,
                dt
            );

        perfectPoseSkeleton.nodes[part.nodeIndex].localTransform =
            perfectPoseSkeleton.restLocalTransforms[part.nodeIndex] *
            poseMatrix(part.pose);

        for (BodyPart& legacyPart : bodyParts)
        {
            if (legacyPart.nodeIndex != part.nodeIndex)
                continue;
            legacyPart.pose = part.pose;
            legacyPart.translationSpring = part.translationSpring;
            legacyPart.rotationSpring = part.rotationSpring;
            break;
        }
    }
    

    updateModelWorldTransforms();
}

Capsule Player::getCapsule() const
{
    Capsule c;
    c.r = movementCapsule.radius > 0.0f ? movementCapsule.radius : PLAYER_RADIUS;

    float height = movementCapsule.height > 0.0f ? movementCapsule.height : PLAYER_HEIGHT;
    float half = height * 0.5f;
    glm::vec3 center = movementCapsule.position;
    if (glm::length(center - pos) > 0.0001f)
        center = pos;
    c.a = center - glm::vec3(0,0,half - c.r);
    c.b = center + glm::vec3(0,0,half - c.r);

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
    if (didGroundJump) playWorldSound("entity/player/jump", pos, 1.0f, 1.0f, 28.0f);

    if (didAirJump)
        playAirJumpSound();

    if (didDash) {
        playWorldSound("entity/player/dash", pos, 1.0f, 1.0f, 36.0f);
        // Spawn dash effect at player position
        EffectPartSystem::instance().spawnDash(pos);
    }

    // Only trigger land sound on stable air->ground transition
    // Use stableOnGround to avoid flickering from collision jitter
    if (!wasStableGroundedLastFrame && stableOnGround)
        playWorldSound("entity/player/land", pos, 1.0f, 1.0f, 32.0f);
    wasStableGroundedLastFrame = stableOnGround;

    glm::vec2 xy = glm::vec2(vel.x,vel.y) + dashVel;
    if (stableOnGround && glm::length(xy) > 0.5f) {
        footstepTimer -= dt;
        if (footstepTimer <= 0.0f) {
            playWorldSound("entity/player/walk" + std::to_string(1 + rand() % 4), pos, 0.8f, 1.0f, 22.0f);
            // Spawn footstep effect at feet position
            Capsule cap = getCapsule();
            glm::vec3 footPos = cap.a;
            footPos.z -= cap.r; // bottom of capsule
            EffectPartSystem::instance().spawnFootstep(footPos);
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

    if (modelLoaded && !physicalBody.parts.empty() && physicalBody.partMeshes.size() == physicalBody.parts.size())
    {
        MIMITA_GL_CLEAR_STAGE("Player::render body parts");
        MIMITA_GL_CALL(glUseProgram(shader));
        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
        glUniform1i(glGetUniformLocation(shader,"uUseColor"),0);
        glUniform1i(glGetUniformLocation(shader,"uTex"),0);

        glActiveTexture(GL_TEXTURE0);
        for (size_t i = 0; i < physicalBody.parts.size(); ++i)
        {
            const PhysicalBodyPart& part = physicalBody.parts[i];
            const Mesh& mesh = physicalBody.partMeshes[i];
            if (part.nodeIndex < 0 || part.nodeIndex >= (int)perfectPoseSkeleton.nodes.size() || mesh.verts.empty())
                continue;

            uploadBodyPartMesh(mesh);

            const glm::mat4& model = part.worldTransform;
            glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);

            for (const Mesh::Batch& batch : mesh.batches)
            {
                MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default")));
                MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count));
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

        MIMITA_GL_CLEAR_STAGE("Player::render mesh");
        MIMITA_GL_CALL(glUseProgram(shader));
        glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
        glUniform1i(glGetUniformLocation(shader,"uUseColor"),0);
        glUniform1i(glGetUniformLocation(shader,"uTex"),0);

        MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE0));
        MIMITA_GL_CALL(glBindVertexArray(playerVAO));
        for (const Mesh::Batch& batch : renderMesh.batches)
        {
            MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, batch.texture ? batch.texture : gTextures.get("default")));
            MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, (GLint)batch.first, (GLsizei)batch.count));
        }
        return;
    }

    initCapsuleMesh();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos);

    MIMITA_GL_CLEAR_STAGE("Player::render capsule");
    MIMITA_GL_CALL(glUseProgram(shader));
    glUniformMatrix4fv(glGetUniformLocation(shader,"view"),1,0,&view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"projection"),1,0,&proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"model"),1,0,&model[0][0]);
    glUniform1i(glGetUniformLocation(shader,"uUseColor"),0);
    glUniform1i(glGetUniformLocation(shader,"uTex"),0);

    MIMITA_GL_CALL(glActiveTexture(GL_TEXTURE0));
    MIMITA_GL_CALL(glBindTexture(GL_TEXTURE_2D, gTextures.get("greenwirev1")));

    MIMITA_GL_CALL(glBindVertexArray(capsuleVAO));
    MIMITA_GL_CALL(glDrawArrays(GL_TRIANGLES, 0, capsuleVertCount));
}

void Player::takeDamage(int damage, const glm::vec3& knockbackDir, float knockbackForce)
{
    if (damage <= 0) return;
    
    int oldHp = currentHp;
    currentHp = std::max(0, currentHp - damage);
    int actualDamage = oldHp - currentHp;
    
    if (actualDamage <= 0) return;
    
    // Play hurt sound with volume/pitch based on damage
    float hurt01 = std::clamp(actualDamage / 100.0f, 0.0f, 1.0f);
    float volume = 0.3f + hurt01 * 0.7f;      // 0.3 - 1.0
    float pitch = 1.1f - hurt01 * 0.2f;       // 1.1 - 0.9
    
    playWorldSound("player_hurt", pos, volume, pitch, 35.0f);
    
    // Apply knockback
    if (knockbackForce > 0.0f && glm::length(knockbackDir) > 0.001f) {
        vel += glm::normalize(knockbackDir) * knockbackForce;
        vel.z += knockbackForce * 0.2f; // Slight upward knockback
    }
    
    // Spawn blood effect at player position
    EffectPartSystem::instance().spawnDamage(pos, username, actualDamage);
    EffectPartSystem::instance().spawnStickyBlood(pos, glm::vec3(0,0,1), std::clamp(actualDamage / 100.0f, 0.1f, 1.0f), 0);
    
    if (DebugConfig::DEBUG_COMMANDS) {
        Debug::log(Debug::Category::General, "[PLAYER HURT] damage=%d hp=%d/%d vol=%.2f pitch=%.2f\n",
                   actualDamage, currentHp, maxHp, volume, pitch);
    }
}
