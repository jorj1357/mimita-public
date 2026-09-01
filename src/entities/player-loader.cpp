#include "player.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "avatar/character-registry.h"
#include "camera.h"
#include "debug/debug-log.h"
#include "map/map_loader.h"
#include "physics/config.h"
#include "tinygltf/tiny_gltf.h"
#include <nlohmann/json.hpp>
#include "utils/path_utils.h"
#include "world/texture-store.h"

// Global avatar bodypart overrides, set by AvatarSystem before loadModel.
// Used by applyBodypartConfigOverrides to layer per-avatar overrides on
// top of the global config/bodyparts.json defaults.
nlohmann::json gAvatarBodypartOverrides;

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

} // anonymous namespace

static GLuint getOpaqueWhiteTexture() {
    static GLuint tex = 0;
    if (tex) return tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    unsigned char white[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    return tex;
}

bool isPlayerBodyPart(const std::string& name)
{
    return name == "head" || name == "torso" ||
           name == "leftArm" || name == "rightArm" ||
            name == "leftLeg" || name == "rightLeg";
}

// ── Immutable GLB parse cache ─────────────────────────────────────
// Parsing a player GLB (tinygltf file IO + hierarchy + colliders +
// part meshes) is expensive and identical for every replica of the same
// model. Cache the immutable parsed result per resolved path so creating
// a remote player replica reuses CPU/GPU-ready data instead of reparsing.
// Mutable per-player state (animation time, health, transform, runtime
// weapon state, interpolation buffer) stays on the Player, never shared.
struct CachedPlayerModel
{
    Mesh renderMesh;
    std::vector<TransformNode> nodes;
    std::vector<glm::mat4> restLocalTransforms;
    std::vector<Collider> bodyColliders;
    std::vector<BodyPart> bodyParts;
    std::vector<Mesh> bodyPartMeshes;
    PerfectPoseSkeleton perfectPose;
    bool valid = false;
};

std::unordered_map<std::string, CachedPlayerModel>& playerModelCache()
{
    static std::unordered_map<std::string, CachedPlayerModel> cache;
    return cache;
}

void clearPlayerModelCache()
{
    playerModelCache().clear();
}

// Copies the immutable parsed model data into a Player, then rebuilds the
// physical body with FRESH collider references so mutable per-player pose,
// spring, and world-transform state is never shared across replicas.
void copyCachedModelIntoPlayer(const CachedPlayerModel& cached, Player& player)
{
    player.renderMesh = cached.renderMesh;
    player.nodes = cached.nodes;
    player.restLocalTransforms = cached.restLocalTransforms;
    player.bodyColliders = cached.bodyColliders;
    player.bodyParts = cached.bodyParts;
    player.bodyPartMeshes = cached.bodyPartMeshes;
    player.perfectPoseSkeleton = cached.perfectPose;
    player.physicalBody.parts.clear();
    player.physicalBody.partMeshes.clear();
    for (const BodyPart& part : cached.bodyParts)
    {
        PhysicalBodyPart physicalPart;
        physicalPart.name = part.name;
        physicalPart.nodeIndex = part.nodeIndex;
        physicalPart.collider = part.collider;
        player.physicalBody.parts.push_back(physicalPart);
    }
    player.physicalBody.partMeshes = cached.bodyPartMeshes;
    player.modelLoaded = !player.renderMesh.verts.empty();
}

// Loads config/bodyparts.json and overrides the GLB's rest-pose
// transforms for each matching body part node.
static void applyBodypartConfigOverrides(
    std::vector<glm::mat4>& restLocalTransforms,
    const std::vector<TransformNode>& nodes)
{
    std::ifstream file("config/bodyparts.json");
    if (!file.is_open()) {
        Debug::log(Debug::Category::General,
            "[BODYPART] config/bodyparts.json not found, using GLB defaults\n");
        return;
    }
    nlohmann::json j;
    try { file >> j; } catch (...) {
        Debug::warn(Debug::Category::General,
            "[BODYPART] Failed to parse config/bodyparts.json\n");
        return;
    }

    int overrides = 0;
    for (int i = 0; i < (int)nodes.size(); ++i) {
        const std::string& name = nodes[i].name;
        if (!j.contains(name)) continue;
        if (!isPlayerBodyPart(name)) continue;

        auto& part = j[name];
        float tX = 0, tY = 0, tZ = 0;
        float rX = 0, rY = 0, rZ = 0;
        float sX = 1, sY = 1, sZ = 1;

        if (part.contains("offset") && part["offset"].is_array() && part["offset"].size() >= 3) {
            tX = part["offset"][0].get<float>();
            tY = part["offset"][1].get<float>();
            tZ = part["offset"][2].get<float>();
        }
        if (part.contains("rotation") && part["rotation"].is_array() && part["rotation"].size() >= 3) {
            rX = part["rotation"][0].get<float>();
            rY = part["rotation"][1].get<float>();
            rZ = part["rotation"][2].get<float>();
        }
        if (part.contains("scale") && part["scale"].is_array() && part["scale"].size() >= 3) {
            sX = part["scale"][0].get<float>();
            sY = part["scale"][1].get<float>();
            sZ = part["scale"][2].get<float>();
        }

        // Build transform: T * Rz * Ry * Rx * S
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(tX, tY, tZ));
        if (rZ != 0) m = glm::rotate(m, glm::radians(rZ), glm::vec3(0, 0, 1));
        if (rY != 0) m = glm::rotate(m, glm::radians(rY), glm::vec3(0, 1, 0));
        if (rX != 0) m = glm::rotate(m, glm::radians(rX), glm::vec3(1, 0, 0));
        m = glm::scale(m, glm::vec3(sX, sY, sZ));

        restLocalTransforms[i] = m;
        overrides++;
        Debug::log(Debug::Category::General,
            "[BODYPART] Override '%s': offset=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f) scale=(%.2f,%.2f,%.2f)\n",
            name.c_str(), tX, tY, tZ, rX, rY, rZ, sX, sY, sZ);
    }
    printf("[BODYPART] Applied %d body part overrides from config/bodyparts.json\n", overrides);

    // Apply per-avatar overrides (set via gAvatarBodypartOverrides)
    if (!gAvatarBodypartOverrides.is_null() && gAvatarBodypartOverrides.is_object()) {
        int avOverrides = 0;
        for (int i = 0; i < (int)nodes.size(); ++i) {
            const std::string& name = nodes[i].name;
            if (!gAvatarBodypartOverrides.contains(name)) continue;
            if (!isPlayerBodyPart(name)) continue;

            auto& part = gAvatarBodypartOverrides[name];
            float tX = 0, tY = 0, tZ = 0;
            float rX = 0, rY = 0, rZ = 0;
            float sX = 1, sY = 1, sZ = 1;

            if (part.contains("offset") && part["offset"].is_array() && part["offset"].size() >= 3) {
                tX = part["offset"][0].get<float>();
                tY = part["offset"][1].get<float>();
                tZ = part["offset"][2].get<float>();
            }
            if (part.contains("rotation") && part["rotation"].is_array() && part["rotation"].size() >= 3) {
                rX = part["rotation"][0].get<float>();
                rY = part["rotation"][1].get<float>();
                rZ = part["rotation"][2].get<float>();
            }
            if (part.contains("scale") && part["scale"].is_array() && part["scale"].size() >= 3) {
                sX = part["scale"][0].get<float>();
                sY = part["scale"][1].get<float>();
                sZ = part["scale"][2].get<float>();
            }

            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(tX, tY, tZ));
            if (rZ != 0) m = glm::rotate(m, glm::radians(rZ), glm::vec3(0, 0, 1));
            if (rY != 0) m = glm::rotate(m, glm::radians(rY), glm::vec3(0, 1, 0));
            if (rX != 0) m = glm::rotate(m, glm::radians(rX), glm::vec3(1, 0, 0));
            m = glm::scale(m, glm::vec3(sX, sY, sZ));

            restLocalTransforms[i] = m;
            avOverrides++;
            Debug::log(Debug::Category::General,
                "[BODYPART] Avatar override '%s': offset=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f) scale=(%.2f,%.2f,%.2f)\n",
                name.c_str(), tX, tY, tZ, rX, rY, rZ, sX, sY, sZ);
        }
        if (avOverrides > 0)
            printf("[BODYPART] Applied %d avatar body part overrides\n", avOverrides);
    }
    // Clear the global overrides after consuming them
    gAvatarBodypartOverrides = nullptr;
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
        batch.texture = getOpaqueWhiteTexture();
        batch.first = out.verts.size();
        // Read doubleSided from GLTF material if available
        if (primitive.material >= 0 && primitive.material < (int)model.materials.size())
            batch.doubleSided = model.materials[primitive.material].doubleSided;

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
    const std::string resolvedPath = resolveAssetPath(path);
    auto& cache = playerModelCache();

    // Fast path: reuse the parsed immutable GLB data. The cache stores the
    // pristine skeleton as read from the file (pre-override); per-avatar
    // body-part overrides are applied fresh on this player's copy, so shared
    // cache entries never leak mutable per-player state.
    auto cacheIt = cache.find(resolvedPath);
    if (cacheIt != cache.end() && cacheIt->second.valid)
    {
        const CachedPlayerModel& cached = cacheIt->second;
        copyCachedModelIntoPlayer(cached, *this);

        // Apply body part config overrides from config/bodyparts.json + avatar overrides
        applyBodypartConfigOverrides(restLocalTransforms, nodes);
        for (int i = 0; i < (int)restLocalTransforms.size(); ++i) {
            perfectPoseSkeleton.restLocalTransforms[i] = restLocalTransforms[i];
            nodes[i].localTransform = restLocalTransforms[i];
            perfectPoseSkeleton.nodes[i].localTransform = restLocalTransforms[i];
        }

        updateModelWorldTransforms();
        printf("[AVATAR CACHE] model=%s result=hit\n", resolvedPath.c_str());
        return modelLoaded;
    }

    renderMesh = loadGLB(path);
    modelLoaded = !renderMesh.verts.empty();

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
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

    // ── Snapshot pristine (pre-override) skeleton + mesh data ─────────
    // Body-part overrides mutate restLocalTransforms/nodes per avatar, so
    // they must be re-applied on every load, not baked into the cache.
    CachedPlayerModel cached;
    cached.renderMesh = renderMesh;
    cached.nodes = nodes;
    cached.restLocalTransforms = restLocalTransforms;
    cached.perfectPose = perfectPoseSkeleton;
    cached.valid = true;

    // Apply body part config overrides from config/bodyparts.json + avatar overrides
    applyBodypartConfigOverrides(restLocalTransforms, nodes);
    // Sync perfectPoseSkeleton and node localTransforms
    for (int i = 0; i < (int)restLocalTransforms.size(); ++i) {
        perfectPoseSkeleton.restLocalTransforms[i] = restLocalTransforms[i];
        nodes[i].localTransform = restLocalTransforms[i];
        perfectPoseSkeleton.nodes[i].localTransform = restLocalTransforms[i];
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

    // Publish the full immutable parse result into the cache.
    cached.bodyColliders = bodyColliders;
    cached.bodyParts = bodyParts;
    cached.bodyPartMeshes = bodyPartMeshes;
    cache[resolvedPath] = std::move(cached);
    printf("[AVATAR CACHE] model=%s result=miss (skeleton)\n", resolvedPath.c_str());

    updateModelWorldTransforms();
    printf("[PLAYER GLB] hierarchy nodes=%zu bodyColliders=%zu root=plrOrigin expected\n",
           nodes.size(), bodyColliders.size());
    return modelLoaded;
}

bool Player::loadCharacter(const std::string& characterName)
{
    const CharacterManifest* manifest = CharacterRegistry::instance().get(characterName);
    if (!manifest)
    {
        printf("[PLAYER] character '%s' not found in registry, trying DefaultGuy\n", characterName.c_str());
        manifest = CharacterRegistry::instance().get("DefaultGuy");
        if (!manifest)
        {
            printf("[PLAYER] DefaultGuy not found either, using hardcoded path\n");
            return loadModel("assets/entity/player/default/mimita-char-no-animations-v4.glb");
        }
    }

    std::string glbPath = "Characters/" + characterName + "/" + manifest->model;
    printf("[PLAYER] loading character '%s' from %s\n", characterName.c_str(), glbPath.c_str());
    bool ok = loadModel(glbPath.c_str());
    if (!ok)
    {
        printf("[PLAYER] failed to load character GLB, trying DefaultGuy\n");
        const CharacterManifest* fallback = CharacterRegistry::instance().get("DefaultGuy");
        if (fallback && fallback->name != characterName)
        {
            std::string fallbackPath = "Characters/" + fallback->name + "/" + fallback->model;
            return loadModel(fallbackPath.c_str());
        }
        return loadModel("assets/entity/player/default/mimita-char-no-animations-v4.glb");
    }

    mCharacterName = characterName;

    // Update physics capsule from manifest
    PLAYER_RADIUS = manifest->capsule.radius;
    PLAYER_HEIGHT = manifest->capsule.height;

    printf("[PLAYER] character '%s' loaded: capsule=(%.2f, %.2f) camera=(%.2f, %.2f, %.2f)\n",
           characterName.c_str(),
           manifest->capsule.radius, manifest->capsule.height,
           manifest->camera.distance, manifest->camera.height, manifest->camera.shoulderOffset);
    return true;
}

void Player::ensureCharacterLoaded()
{
    if (modelLoaded || mLazyLoadRequested)
        return;
    mLazyLoadRequested = true;
    printf("[PLAYER] lazy-loading character '%s'\n", mCharacterName.c_str());
    loadCharacter(mCharacterName);
}

// ── Threaded model load ──────────────────────────────────────────

static void buildPlayerDataFromModel(PendingPlayerModel* d, const tinygltf::Model& model)
{
    d->nodes.resize(model.nodes.size());
    d->restLocalTransforms.resize(model.nodes.size(), glm::mat4(1.0f));
    d->perfectPose.nodes.clear();
    d->perfectPose.nodes.resize(model.nodes.size());
    d->perfectPose.restLocalTransforms.clear();
    d->perfectPose.restLocalTransforms.resize(model.nodes.size(), glm::mat4(1.0f));

    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const tinygltf::Node& gltfNode = model.nodes[i];
        TransformNode& node = d->nodes[i];
        node.name = gltfNode.name;
        node.localTransform = nodeMatrix(gltfNode);
        d->restLocalTransforms[i] = node.localTransform;
        node.worldTransform = node.localTransform;
        node.children = gltfNode.children;

        TransformNode& poseNode = d->perfectPose.nodes[i];
        poseNode.name = gltfNode.name;
        poseNode.localTransform = node.localTransform;
        poseNode.worldTransform = node.worldTransform;
        poseNode.children = node.children;
        d->perfectPose.restLocalTransforms[i] = node.localTransform;

        for (int child : gltfNode.children)
            if (child >= 0 && child < (int)d->nodes.size())
            {
                d->nodes[child].parent = i;
                d->perfectPose.nodes[child].parent = i;
            }
    }

    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const std::string& name = d->nodes[i].name;
        if (!isPlayerBodyPart(name))
            continue;

        Collider collider;
        collider.name = name;
        appendNodeCollider(model, i, collider);
        d->bodyColliders.push_back(collider);

        BodyPart part;
        part.name = name;
        part.nodeIndex = i;
        part.collider = collider;
        d->bodyParts.push_back(part);

        PhysicalBodyPart physicalPart;
        physicalPart.name = name;
        physicalPart.nodeIndex = i;
        physicalPart.collider = collider;
        d->physicalBodyData.parts.push_back(physicalPart);

        Mesh bodyMesh;
        appendNodeRenderMesh(model, i, bodyMesh);
        d->bodyPartMeshes.push_back(bodyMesh);
        d->physicalBodyData.partMeshes.push_back(bodyMesh);
    }
}

static void modelLoadThreadFunc(PendingPlayerModel* d)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err, warn;

    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, d->resolvedPath);
    if (!warn.empty()) printf("[PLAYER GLB WARNING] %s\n", warn.c_str());
    if (!err.empty()) printf("[PLAYER GLB ERROR] %s\n", err.c_str());
    if (!ok) {
        printf("[PLAYER GLB ERROR] failed async load %s\n", d->path.c_str());
        d->ready = true;
        return;
    }

    d->loadOk = true;
    d->glbDir = std::filesystem::path(d->resolvedPath).parent_path().string();
    d->images = std::move(model.images);
    d->imageCount = (int)d->images.size();

    buildPlayerDataFromModel(d, model);
    d->ready = true;
}

void Player::requestModelLoad(const std::string& filepath)
{
    if (modelLoaded) return;
    if (mPendingModel) return;

    auto data = std::make_shared<PendingPlayerModel>();
    data->path = filepath;
    data->resolvedPath = resolveAssetPath(filepath);
    if (!gAvatarBodypartOverrides.is_null())
        data->bodypartOverrides = gAvatarBodypartOverrides;

    mPendingModel = data;
    PendingPlayerModel* raw = mPendingModel.get();

    std::thread t(modelLoadThreadFunc, raw);
    t.detach();
}

static GLuint uploadPlayerTexture(const tinygltf::Image& image)
{
    if (image.image.empty() || image.width <= 0 || image.height <= 0)
        return 0;
    if (image.component < 1 || image.component > 4)
        return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum srcFormat = GL_RGBA;
    if (image.component == 1) srcFormat = GL_RED;
    else if (image.component == 2) srcFormat = GL_RG;
    else if (image.component == 3) srcFormat = GL_RGB;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
                 srcFormat, GL_UNSIGNED_BYTE, image.image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    return tex;
}

void Player::finalizeModelIfReady()
{
    if (!mPendingModel || modelLoaded) return;
    if (!mPendingModel->ready.load()) return;

    PendingPlayerModel* d = mPendingModel.get();
    if (!d->loadOk) {
        mPendingModel.reset();
        return;
    }

    // Upload textures to GL (must be on main thread)
    std::vector<GLuint> texIds;
    texIds.reserve(d->images.size());
    for (const auto& img : d->images) {
        GLuint t = uploadPlayerTexture(img);
        texIds.push_back(t);
    }

    // Build combined renderMesh from body part meshes
    {
        Mesh combined;
        size_t offset = 0;
        for (const auto& bpm : d->bodyPartMeshes) {
            combined.verts.insert(combined.verts.end(), bpm.verts.begin(), bpm.verts.end());
            for (const auto& batch : bpm.batches) {
                Mesh::Batch cb = batch;
                cb.first += offset;
                combined.batches.push_back(cb);
            }
            offset += bpm.verts.size();
        }
        renderMesh = std::move(combined);
    }

    // Assign all data to Player
    nodes = std::move(d->nodes);
    restLocalTransforms = std::move(d->restLocalTransforms);
    bodyColliders = std::move(d->bodyColliders);
    bodyParts = std::move(d->bodyParts);
    bodyPartMeshes = std::move(d->bodyPartMeshes);
    perfectPoseSkeleton = std::move(d->perfectPose);
    physicalBody = std::move(d->physicalBodyData);

    // Assign texture IDs to body part mesh batches (overwrite any existing)
    int tidx = 0;
    for (Mesh& bpm : bodyPartMeshes) {
        for (auto& batch : bpm.batches) {
            if (tidx < (int)texIds.size()) {
                batch.texture = texIds[tidx++];
            }
        }
    }
    for (Mesh& bpm : physicalBody.partMeshes) {
        for (auto& batch : bpm.batches) {
            if (tidx < (int)texIds.size()) {
                batch.texture = texIds[tidx++];
            }
        }
    }

    // Restore bodypart overrides from saved state (in case global was overwritten)
    if (!d->bodypartOverrides.is_null())
        gAvatarBodypartOverrides = d->bodypartOverrides;

    // Apply bodypart config overrides
    applyBodypartConfigOverrides(restLocalTransforms, nodes);
    for (int i = 0; i < (int)restLocalTransforms.size(); ++i) {
        perfectPoseSkeleton.restLocalTransforms[i] = restLocalTransforms[i];
        nodes[i].localTransform = restLocalTransforms[i];
        perfectPoseSkeleton.nodes[i].localTransform = restLocalTransforms[i];
    }

    updateModelWorldTransforms();
    modelLoaded = true;

    // Avatar application can happen before this async model finishes. Complete
    // the per-player atlas, body overrides, and cosmetics at the ready boundary
    // so joining never requires avatar.reload.
    if (avatarInstance)
        AvatarSystem::instance().finalizeAvatarForPlayer(*this);

    printf("[PLAYER] async model loaded: path=%s nodes=%zu bodyParts=%zu textures=%d\n",
           d->path.c_str(), nodes.size(), bodyParts.size(), (int)texIds.size());
    mPendingModel.reset();
}
