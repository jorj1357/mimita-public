#include "player.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "avatar/character-registry.h"
#include "camera.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
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
bool gAvatarUseModelTransforms = false;
bool gAvatarCanonicalizePlayerModel = false;
// Render-only animated parts declared by the avatar rig, as (partName, nodeName).
// Set by AvatarSystem before loadModel; consumed to build extra render meshes.
std::vector<std::pair<std::string, std::string>> gAvatarRigExtraNodes;

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

// Blender appends a ".NNN" duplicate suffix to datablock names (for example
// "torso.184", "leftLeg.001", "plrOrigin.091"). The engine identifies body
// parts by their base name, so strip a trailing ".<digits>" before matching,
// storing, or looking up a part.
std::string playerBodyPartBaseName(const std::string& name)
{
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= name.size())
        return name;
    for (size_t i = dot + 1; i < name.size(); ++i)
        if (name[i] < '0' || name[i] > '9')
            return name;
    return name.substr(0, dot);
}

bool isPlayerBodyPart(const std::string& name)
{
    const std::string base = playerBodyPartBaseName(name);
    return base == "head" || base == "torso" ||
           base == "leftArm" || base == "rightArm" ||
           base == "leftLeg" || base == "rightLeg";
}

static nlohmann::json vec3Json(const glm::vec3& v)
{
    return {v.x, v.y, v.z};
}

static nlohmann::json mat4Json(const glm::mat4& m)
{
    nlohmann::json out = nlohmann::json::array();
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            out.push_back(m[col][row]);
    return out;
}

static std::string modelSourceSignature(const std::string& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    const auto stamp = std::filesystem::last_write_time(path, ec).time_since_epoch().count();
    const std::string input = path + ":" + std::to_string(size) + ":" + std::to_string(stamp);
    return std::to_string(std::hash<std::string>{}(input));
}

static glm::vec3 readAccessorVec3(const tinygltf::Model& model,
                                  const tinygltf::Accessor& accessor,
                                  size_t index)
{
    const unsigned char* base = accessorPtr(model, accessor);
    const size_t stride = accessorStride(model, accessor);
    glm::vec3 value(0.0f);
    std::memcpy(&value.x, base + index * stride + sizeof(float) * 0, sizeof(float));
    std::memcpy(&value.y, base + index * stride + sizeof(float) * 1, sizeof(float));
    std::memcpy(&value.z, base + index * stride + sizeof(float) * 2, sizeof(float));
    return value;
}

static void writeAccessorVec3(const tinygltf::Model& model,
                              const tinygltf::Accessor& accessor,
                              size_t index,
                              const glm::vec3& value)
{
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    tinygltf::Buffer& buffer = const_cast<tinygltf::Buffer&>(model.buffers[view.buffer]);
    unsigned char* base = buffer.data.data() + view.byteOffset + accessor.byteOffset;
    const size_t stride = accessorStride(model, accessor);
    unsigned char* dst = base + index * stride;
    std::memcpy(dst + sizeof(float) * 0, &value.x, sizeof(float));
    std::memcpy(dst + sizeof(float) * 1, &value.y, sizeof(float));
    std::memcpy(dst + sizeof(float) * 2, &value.z, sizeof(float));
}

static bool accessorFiniteAndBounds(const tinygltf::Model& model,
                                    const tinygltf::Accessor& accessor,
                                    glm::vec3& outMin,
                                    glm::vec3& outMax)
{
    if (accessor.type != TINYGLTF_TYPE_VEC3 ||
        accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.count == 0 || accessor.bufferView < 0 ||
        accessor.bufferView >= (int)model.bufferViews.size())
        return false;

    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < accessor.count; ++i) {
        const glm::vec3 p = readAccessorVec3(model, accessor, i);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return false;
        outMin = glm::min(outMin, p);
        outMax = glm::max(outMax, p);
    }
    return true;
}

// Convert a rigid multipart custom model into the same bind-frame convention
// used by the default player GLB. The visible rest pose is preserved by moving
// the inverse correction into mesh-local vertices. This is intentionally
// player-only and runs before the immutable model cache is published.
static bool normalizePlayerModelTransforms(tinygltf::Model& model,
                                            const std::string& resolvedPath)
{
    const std::string signature = modelSourceSignature(resolvedPath);
    int rootIndex = -1;
    std::unordered_map<std::string, int> partIndices;
    for (int i = 0; i < (int)model.nodes.size(); ++i) {
        const std::string name = playerBodyPartBaseName(model.nodes[i].name);
        if (name == "plrOrigin")
            rootIndex = rootIndex < 0 ? i : -2;
        if (isPlayerBodyPart(name))
            partIndices[name] = i;
    }

    const char* required[] = {"head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"};
    if (rootIndex < 0 || rootIndex == -2 || partIndices.size() != 6) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "avatar.model.normalized-node", signature,
            "canonicalization rejected: root or required parts invalid", 0,
            {{"model_path", resolvedPath}, {"result", "rejected"},
             {"reason", "root_or_required_parts"}},
            __FILE__, __LINE__, __FUNCTION__);
        return false;
    }

    std::vector<int> parents(model.nodes.size(), -1);
    for (int i = 0; i < (int)model.nodes.size(); ++i)
        for (int child : model.nodes[i].children)
            if (child >= 0 && child < (int)parents.size())
                parents[child] = i;

    std::vector<int> visiting(model.nodes.size(), 0);
    std::vector<glm::mat4> worlds(model.nodes.size(), glm::mat4(1.0f));
    std::function<bool(int)> computeWorld = [&](int index) {
        if (index < 0 || index >= (int)model.nodes.size()) return false;
        if (visiting[index] == 2) return true;
        if (visiting[index] == 1) return false;
        visiting[index] = 1;
        const int parent = parents[index];
        worlds[index] = parent >= 0
            ? worlds[parent] * nodeMatrix(model.nodes[index])
            : nodeMatrix(model.nodes[index]);
        if (parent >= 0 && !computeWorld(parent)) return false;
        if (parent >= 0)
            worlds[index] = worlds[parent] * nodeMatrix(model.nodes[index]);
        visiting[index] = 2;
        return true;
    };
    for (int i = 0; i < (int)model.nodes.size(); ++i)
        if (!computeWorld(i))
            return false;

    for (const char* requiredName : required) {
        const int index = partIndices[requiredName];
        if (parents[index] != rootIndex) {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Avatar, StructuredLevel::Important,
                "avatar.model.normalized-node", signature,
                "canonicalization rejected: body part is not a direct root child", 0,
                {{"model_path", resolvedPath}, {"part", requiredName},
                 {"result", "rejected"}, {"reason", "non_direct_parent"}},
                __FILE__, __LINE__, __FUNCTION__);
            return false;
        }
    }

    const glm::quat canonicalRotation = glm::angleAxis(
        glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    std::unordered_set<int> transformedMeshes;
    for (const char* requiredName : required) {
        const int index = partIndices[requiredName];
        const tinygltf::Node& rawNode = model.nodes[index];
        const glm::mat4 rawWorld = worlds[index];
        const glm::vec3 rawPosition = glm::vec3(rawWorld[3]);
        const int meshIndex = rawNode.mesh;
        if (meshIndex < 0 || meshIndex >= (int)model.meshes.size())
            return false;
        if (!transformedMeshes.insert(meshIndex).second)
            return false;

        glm::vec3 rawBoundsMin(0.0f), rawBoundsMax(0.0f);
        size_t rawVertexCount = 0;
        bool rawBoundsValid = false;
        for (const tinygltf::Primitive& primitive : model.meshes[meshIndex].primitives) {
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) return false;
            const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
            rawBoundsValid = accessorFiniteAndBounds(
                model, posAccessor, rawBoundsMin, rawBoundsMax);
            rawVertexCount += posAccessor.count;
            if (!rawBoundsValid) return false;
        }

        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "avatar.model.raw-node", signature,
            "raw custom player model node", 0,
            {{"model_path", resolvedPath}, {"part", requiredName},
             {"node_index", index}, {"parent_index", parents[index]},
             {"mesh_index", meshIndex}, {"world_matrix", mat4Json(rawWorld)},
             {"world_position", vec3Json(rawPosition)},
             {"local_matrix", mat4Json(nodeMatrix(rawNode))},
             {"vertex_count", rawVertexCount},
             {"bounds_min", vec3Json(rawBoundsMin)},
             {"bounds_max", vec3Json(rawBoundsMax)},
             {"finite_vertices", rawBoundsValid}},
            __FILE__, __LINE__, __FUNCTION__);

        const glm::mat4 canonicalWorld =
            glm::translate(glm::mat4(1.0f), rawPosition) *
            glm::mat4_cast(canonicalRotation);
        const glm::mat4 meshCorrection = glm::inverse(canonicalWorld) * rawWorld;
        const glm::mat3 normalCorrection = glm::transpose(glm::inverse(glm::mat3(meshCorrection)));

        tinygltf::Mesh& mesh = model.meshes[meshIndex];
        for (tinygltf::Primitive& primitive : mesh.primitives) {
            auto posIt = primitive.attributes.find("POSITION");
            if (posIt == primitive.attributes.end()) return false;
            const tinygltf::Accessor& posAccessor = model.accessors[posIt->second];
            glm::vec3 mn, mx;
            if (!accessorFiniteAndBounds(model, posAccessor, mn, mx)) return false;
            for (size_t v = 0; v < posAccessor.count; ++v) {
                const glm::vec3 p = glm::vec3(meshCorrection * glm::vec4(
                    readAccessorVec3(model, posAccessor, v), 1.0f));
                writeAccessorVec3(model, posAccessor, v, p);
            }
            auto normalIt = primitive.attributes.find("NORMAL");
            if (normalIt != primitive.attributes.end()) {
                const tinygltf::Accessor& normalAccessor = model.accessors[normalIt->second];
                for (size_t v = 0; v < normalAccessor.count; ++v) {
                    glm::vec3 n = normalCorrection * readAccessorVec3(model, normalAccessor, v);
                    if (glm::dot(n, n) > 0.000001f) n = glm::normalize(n);
                    writeAccessorVec3(model, normalAccessor, v, n);
                }
            }
        }

        tinygltf::Node& normalizedNode = model.nodes[index];
        normalizedNode.matrix.clear();
        normalizedNode.translation = {rawPosition.x, rawPosition.y, rawPosition.z};
        normalizedNode.rotation = {canonicalRotation.x, canonicalRotation.y,
                                   canonicalRotation.z, canonicalRotation.w};
        normalizedNode.scale = {1.0, 1.0, 1.0};

        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "avatar.model.normalized-node", signature,
            "custom player model node normalized", 0,
            {{"model_path", resolvedPath}, {"part", requiredName},
             {"node_index", index}, {"result", "normalized"},
             {"source_world_matrix", mat4Json(rawWorld)},
             {"output_world_matrix", mat4Json(canonicalWorld)},
             {"mesh_correction", mat4Json(meshCorrection)},
             {"canonical_rotation", {canonicalRotation.x, canonicalRotation.y,
                                      canonicalRotation.z, canonicalRotation.w}},
             {"pivot", vec3Json(rawPosition)}, {"scale_baked", true}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    tinygltf::Node& root = model.nodes[rootIndex];
    root.matrix.clear();
    root.translation = {0.0, 0.0, 0.0};
    root.rotation = {0.0, 0.0, 0.0, 1.0};
    root.scale = {1.0, 1.0, 1.0};

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        "avatar.model.normalized-node", signature,
        "custom player model canonicalization complete", 0,
        {{"model_path", resolvedPath}, {"result", "success"},
         {"root_index", rootIndex}, {"root_reset_identity", true},
         {"part_count", 6}, {"canonical_axis", "default_player_bind_frame"}},
        __FILE__, __LINE__, __FUNCTION__);
    return true;
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
    std::vector<PhysicalBodyPart> extraParts;
    std::vector<Mesh> extraPartMeshes;
    PerfectPoseSkeleton perfectPose;
    bool valid = false;
};

std::unordered_map<std::string, CachedPlayerModel>& playerModelCache()
{
    static std::unordered_map<std::string, CachedPlayerModel> cache;
    return cache;
}

static std::string playerModelCacheKey(const std::string& resolvedPath)
{
    std::string key = resolvedPath +
        (gAvatarCanonicalizePlayerModel ? "|player_canonical" : "|authored");
    // Extra render parts depend on the avatar rig, so include it in the key.
    for (const auto& [partName, nodeName] : gAvatarRigExtraNodes)
        key += "|" + partName + ":" + nodeName;
    return key;
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
    player.physicalBody.extraParts = cached.extraParts;
    player.physicalBody.extraPartMeshes = cached.extraPartMeshes;
    player.modelLoaded = !player.renderMesh.verts.empty();
}

void appendNodeRenderMesh(const tinygltf::Model& model, int nodeIndex, Mesh& out);

// Load every remaining mesh node (not already a physical or rig part) as a
// render-only animated part. This makes an avatar with arbitrary node names
// (e.g. "Cone.042", "Sphere.055") a real, animated, rendered body instead of
// silently falling back to the default model. Names use the base node name and
// are de-duplicated.
static void appendGenericRenderParts(
        const tinygltf::Model& model,
        const std::vector<TransformNode>& nodes,
        const std::vector<PhysicalBodyPart>& physical,
        std::vector<PhysicalBodyPart>& extras,
        std::vector<Mesh>& extraMeshes)
{
    std::unordered_set<int> used;
    for (const auto& p : physical)
        if (p.nodeIndex >= 0) used.insert(p.nodeIndex);
    for (const auto& p : extras)
        if (p.nodeIndex >= 0) used.insert(p.nodeIndex);

    std::unordered_map<std::string, int> nameCount;
    for (int i = 0; i < (int)nodes.size(); ++i) {
        if (used.count(i))
            continue;
        const tinygltf::Node& node = model.nodes[i];
        if (node.mesh < 0 || node.mesh >= (int)model.meshes.size())
            continue;
        Mesh mesh;
        appendNodeRenderMesh(model, i, mesh);
        if (mesh.verts.empty())
            continue;

        std::string base = playerBodyPartBaseName(nodes[i].name);
        if (base.empty())
            base = "part";
        int& c = nameCount[base];
        const std::string name = (c == 0) ? base : base + "_" + std::to_string(c);
        ++c;

        PhysicalBodyPart part;
        part.name = name;
        part.nodeIndex = i;
        extras.push_back(std::move(part));
        extraMeshes.push_back(std::move(mesh));
    }
}

// Build render-only parts (wings/stinger/tail) declared by the avatar rig.
// Each extra node is looked up by base name, given a render mesh and a
// nodeIndex, and animated by the same clip runtime as physical parts.
static void buildExtraRenderPartsFromModel(
        const tinygltf::Model& model,
        const std::vector<std::pair<std::string, std::string>>& extraNodes,
        std::vector<PhysicalBodyPart>& outParts,
        std::vector<Mesh>& outMeshes,
        const std::vector<TransformNode>& nodes)
{
    outParts.clear();
    outMeshes.clear();
    if (extraNodes.empty())
        return;

    for (const auto& [partName, nodeName] : extraNodes) {
        // Prefer an exact node-name match (rigs may reference distinct duplicate
        // nodes such as "Cone.042".."Cone.059"); fall back to a base-name match
        // so a Blender ".NNN" suffix is still tolerated.
        int idx = -1;
        for (int i = 0; i < (int)nodes.size(); ++i) {
            if (nodes[i].name == nodeName) { idx = i; break; }
        }
        if (idx < 0) {
            const std::string base = playerBodyPartBaseName(nodeName);
            for (int i = 0; i < (int)nodes.size(); ++i) {
                if (playerBodyPartBaseName(nodes[i].name) == base) { idx = i; break; }
            }
        }
        if (idx < 0) {
            Debug::warn(Debug::Category::GLB,
                "[PLAYER GLB] rig part '%s' node '%s' not found\n",
                partName.c_str(), nodeName.c_str());
            continue;
        }
        Mesh mesh;
        appendNodeRenderMesh(model, idx, mesh);
        if (mesh.verts.empty())
            continue;
        PhysicalBodyPart part;
        part.name = partName;
        part.nodeIndex = idx;
        outParts.push_back(std::move(part));
        outMeshes.push_back(std::move(mesh));
    }

    nlohmann::json resolved = nlohmann::json::array();
    for (const auto& p : outParts)
        resolved.push_back({{"name", p.name}, {"node_index", p.nodeIndex}});
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Avatar, StructuredLevel::Important,
        "anim.rig.parts", "rig",
        "render-only rig parts resolved", 0,
        {{"declared", extraNodes.size()}, {"resolved", outParts.size()},
         {"parts", resolved}},
        __FILE__, __LINE__, __FUNCTION__);
}

// Loads the legacy config/bodyparts.json and avatar overrides for each
// matching body part node. Custom authored-model avatars bypass this layer.
static void applyBodypartConfigOverrides(
    std::vector<glm::mat4>& restLocalTransforms,
    const std::vector<TransformNode>& nodes,
    bool useModelTransforms = false)
{
    if (useModelTransforms) {
        gAvatarBodypartOverrides = nullptr;
        return;
    }
    std::ifstream file("config/bodyparts.json");
    if (!file.is_open()) {
        Debug::log(Debug::Category::General,
            "[BODYPART] config/bodyparts.json not found, using GLB defaults\n");
        return;
    }
    nlohmann::json j;
    try { j = nlohmann::json::parse(file, nullptr, true, true); } catch (...) {
        Debug::warn(Debug::Category::General,
            "[BODYPART] Failed to parse config/bodyparts.json\n");
        return;
    }

    int overrides = 0;
    for (int i = 0; i < (int)nodes.size(); ++i) {
        const std::string name = playerBodyPartBaseName(nodes[i].name);
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
            const std::string name = playerBodyPartBaseName(nodes[i].name);
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

// ── Shared CPU model builders ─────────────────────────────────────
// These build the skeleton and per-part collision triangles from an already
// parsed GLB with no GL calls. They are used by Player::loadModel (after the
// GL render mesh is loaded) and by Player::loadModelColliders (headless actors).
static void buildNodeHierarchyFromModel(const tinygltf::Model& model, Player& player)
{
    player.nodes.clear();
    player.nodes.resize(model.nodes.size());
    player.restLocalTransforms.clear();
    player.restLocalTransforms.resize(model.nodes.size(), glm::mat4(1.0f));
    player.bodyColliders.clear();
    player.bodyParts.clear();
    player.bodyPartMeshes.clear();
    player.perfectPoseSkeleton.nodes.clear();
    player.perfectPoseSkeleton.nodes.resize(model.nodes.size());
    player.perfectPoseSkeleton.restLocalTransforms.clear();
    player.perfectPoseSkeleton.restLocalTransforms.resize(model.nodes.size(), glm::mat4(1.0f));
    player.physicalBody.parts.clear();
    player.physicalBody.partMeshes.clear();

    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const tinygltf::Node& gltfNode = model.nodes[i];
        TransformNode& node = player.nodes[i];
        node.name = gltfNode.name;
        node.localTransform = nodeMatrix(gltfNode);
        player.restLocalTransforms[i] = node.localTransform;
        node.worldTransform = node.localTransform;
        node.children = gltfNode.children;

        TransformNode& poseNode = player.perfectPoseSkeleton.nodes[i];
        poseNode.name = gltfNode.name;
        poseNode.localTransform = node.localTransform;
        poseNode.worldTransform = node.worldTransform;
        poseNode.children = node.children;
        player.perfectPoseSkeleton.restLocalTransforms[i] = node.localTransform;

        for (int child : gltfNode.children)
            if (child >= 0 && child < (int)player.nodes.size())
            {
                player.nodes[child].parent = i;
                player.perfectPoseSkeleton.nodes[child].parent = i;
            }
    }
}

// After applyBodypartConfigOverrides mutates restLocalTransforms, push the
// result into the pose skeleton and legacy nodes so world transforms match.
static void syncPoseSkeletonFromRestLocal(Player& player)
{
    for (int i = 0; i < (int)player.restLocalTransforms.size(); ++i) {
        player.perfectPoseSkeleton.restLocalTransforms[i] = player.restLocalTransforms[i];
        player.nodes[i].localTransform = player.restLocalTransforms[i];
        player.perfectPoseSkeleton.nodes[i].localTransform = player.restLocalTransforms[i];
    }
}

// Node-local collision triangles and render meshes per body part. The triangles
// stay node-local; part.worldTransform (set by updateModelWorldTransforms)
// places them in the world for both the renderer and the collision owner.
// buildRenderMeshes must be false on a headless actor: appendNodeRenderMesh
// resolves a GL texture and requires a context.
static void buildBodyPartCollidersFromModel(const tinygltf::Model& model, Player& player,
                                            bool buildRenderMeshes)
{
    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const std::string name = playerBodyPartBaseName(player.nodes[i].name);
        if (!isPlayerBodyPart(name))
            continue;

        Collider collider;
        collider.name = name;
        appendNodeCollider(model, i, collider);
        player.bodyColliders.push_back(collider);

        BodyPart part;
        part.name = name;
        part.nodeIndex = i;
        part.collider = collider;
        player.bodyParts.push_back(part);

        PhysicalBodyPart physicalPart;
        physicalPart.name = name;
        physicalPart.nodeIndex = i;
        physicalPart.collider = collider;
        player.physicalBody.parts.push_back(physicalPart);

        size_t renderVerts = 0;
        if (buildRenderMeshes)
        {
            Mesh bodyMesh;
            appendNodeRenderMesh(model, i, bodyMesh);
            renderVerts = bodyMesh.verts.size();
            player.bodyPartMeshes.push_back(bodyMesh);
            player.physicalBody.partMeshes.push_back(bodyMesh);
        }

        Debug::log(
            Debug::Category::GLB,
            "[PLAYER GLB] body collider=%s localTriangles=%zu localVerts=%zu localMin=(%.2f %.2f %.2f) localMax=(%.2f %.2f %.2f)\n",
            collider.name.c_str(),
            collider.triangles.size(),
            renderVerts,
            collider.localMin.x, collider.localMin.y, collider.localMin.z,
            collider.localMax.x, collider.localMax.y, collider.localMax.z
        );
    }

    if (buildRenderMeshes) {
        buildExtraRenderPartsFromModel(model, gAvatarRigExtraNodes,
                                       player.physicalBody.extraParts,
                                       player.physicalBody.extraPartMeshes,
                                       player.nodes);
        // Only generalize when the model has no canonical body or declares a
        // rig, so existing six-part avatars are unchanged.
        if (player.physicalBody.parts.empty() || !gAvatarRigExtraNodes.empty())
            appendGenericRenderParts(model, player.nodes, player.physicalBody.parts,
                                     player.physicalBody.extraParts,
                                     player.physicalBody.extraPartMeshes);
    }
}

bool Player::loadModel(const char* path)
{
    const std::string resolvedPath = resolveAssetPath(path);
    auto& cache = playerModelCache();
    const std::string cacheKey = playerModelCacheKey(resolvedPath);

    // Fast path: reuse the parsed immutable GLB data. The cache stores the
    // pristine skeleton as read from the file (pre-override); per-avatar
    // body-part overrides are applied fresh on this player's copy, so shared
    // cache entries never leak mutable per-player state.
    auto cacheIt = cache.find(cacheKey);
    if (cacheIt != cache.end() && cacheIt->second.valid)
    {
        const CachedPlayerModel& cached = cacheIt->second;
        copyCachedModelIntoPlayer(cached, *this);

        // Apply body part config overrides from config/bodyparts.json + avatar overrides
        applyBodypartConfigOverrides(restLocalTransforms, nodes, gAvatarUseModelTransforms);
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

    if (gAvatarCanonicalizePlayerModel &&
        !normalizePlayerModelTransforms(model, resolvedPath)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "avatar.model.normalized-node", modelSourceSignature(resolvedPath),
            "canonicalization rejected; preserving authored model", 0,
            {{"model_path", resolvedPath}, {"result", "fallback_authored"}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    buildNodeHierarchyFromModel(model, *this);

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
    applyBodypartConfigOverrides(restLocalTransforms, nodes, gAvatarUseModelTransforms);
    syncPoseSkeletonFromRestLocal(*this);

    buildBodyPartCollidersFromModel(model, *this, /*buildRenderMeshes=*/true);

    // Publish the full immutable parse result into the cache.
    cached.bodyColliders = bodyColliders;
    cached.bodyParts = bodyParts;
    cached.bodyPartMeshes = bodyPartMeshes;
    cached.extraParts = physicalBody.extraParts;
    cached.extraPartMeshes = physicalBody.extraPartMeshes;
    cache[cacheKey] = std::move(cached);
    printf("[AVATAR CACHE] model=%s result=miss (skeleton)\n", resolvedPath.c_str());

    updateModelWorldTransforms();
    printf("[PLAYER GLB] hierarchy nodes=%zu bodyColliders=%zu root=plrOrigin expected\n",
           nodes.size(), bodyColliders.size());
    return modelLoaded;
}

bool Player::loadModelColliders(const char* path)
{
    const std::string resolvedPath = resolveAssetPath(path);
    auto& cache = playerModelCache();
    const std::string cacheKey = playerModelCacheKey(resolvedPath);

    // Reuse an already-parsed model when a client has cached it. The cache
    // holds the pristine skeleton; overrides are re-applied per actor.
    auto cacheIt = cache.find(cacheKey);
    if (cacheIt != cache.end() && cacheIt->second.valid)
    {
        copyCachedModelIntoPlayer(cacheIt->second, *this);
        applyBodypartConfigOverrides(restLocalTransforms, nodes, gAvatarUseModelTransforms);
        syncPoseSkeletonFromRestLocal(*this);
        updateModelWorldTransforms();
        return !physicalBody.parts.empty();
    }

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string err;
    std::string warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, resolvedPath))
    {
        Debug::warn(Debug::Category::GLB,
            "[PLAYER GLB HEADLESS] failed to load %s: %s\n",
            resolvedPath.c_str(), err.c_str());
        return false;
    }

    if (gAvatarCanonicalizePlayerModel &&
        !normalizePlayerModelTransforms(model, resolvedPath)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "avatar.model.normalized-node", modelSourceSignature(resolvedPath),
            "canonicalization rejected; preserving authored collider model", 0,
            {{"model_path", resolvedPath}, {"result", "fallback_authored"}},
            __FILE__, __LINE__, __FUNCTION__);
    }

    buildNodeHierarchyFromModel(model, *this);
    applyBodypartConfigOverrides(restLocalTransforms, nodes, gAvatarUseModelTransforms);
    syncPoseSkeletonFromRestLocal(*this);
    buildBodyPartCollidersFromModel(model, *this, /*buildRenderMeshes=*/false);
    updateModelWorldTransforms();

    // Do not publish into the shared cache: this actor has no GL render mesh,
    // and a cache entry with an empty render mesh would break later client loads.
    return !physicalBody.parts.empty();
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
        const std::string name = playerBodyPartBaseName(d->nodes[i].name);
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

    buildExtraRenderPartsFromModel(model, d->rigExtraNodes,
                                   d->physicalBodyData.extraParts,
                                   d->physicalBodyData.extraPartMeshes,
                                   d->nodes);
    if (d->physicalBodyData.parts.empty() || !d->rigExtraNodes.empty())
        appendGenericRenderParts(model, d->nodes, d->physicalBodyData.parts,
                                 d->physicalBodyData.extraParts,
                                 d->physicalBodyData.extraPartMeshes);
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

    if (d->canonicalizePlayerModel &&
        !normalizePlayerModelTransforms(model, d->resolvedPath)) {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Avatar, StructuredLevel::Important,
            "avatar.model.normalized-node", modelSourceSignature(d->resolvedPath),
            "canonicalization rejected; preserving authored async model", 0,
            {{"model_path", d->resolvedPath}, {"result", "fallback_authored"}},
            __FILE__, __LINE__, __FUNCTION__);
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
    data->useModelTransforms = gAvatarUseModelTransforms;
    data->canonicalizePlayerModel = gAvatarCanonicalizePlayerModel;
    data->rigExtraNodes = gAvatarRigExtraNodes;

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
    for (Mesh& bpm : physicalBody.extraPartMeshes) {
        for (auto& batch : bpm.batches) {
            if (tidx < (int)texIds.size()) {
                batch.texture = texIds[tidx++];
            }
        }
    }

    // Restore bodypart overrides from saved state (in case global was overwritten)
    if (!d->bodypartOverrides.is_null() && !d->useModelTransforms)
        gAvatarBodypartOverrides = d->bodypartOverrides;
    gAvatarUseModelTransforms = d->useModelTransforms;

    // Apply bodypart config overrides
    applyBodypartConfigOverrides(restLocalTransforms, nodes, gAvatarUseModelTransforms);
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
