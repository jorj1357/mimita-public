// 2026-09-27
/* purpose
* One generic actor-triangle input: collect body parts and the weapon through a
* single representation, and load their triangles from GLB with no GL context.
* Uses tinygltf + glm only, so NPC, headless-server, and replay actors work.
* Does NOT solve or respond; the triangle solver owns that.
* Does NOT render or upload GPU resources.
* Does NOT change the active collision path.
*/

#include "physics/movement/actor-collision-mesh.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "entities/player.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-config.h"
#include "combat/weapon-types.h"
#include "tinygltf/tiny_gltf.h"
#include "utils/path_utils.h"

namespace {

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
    const float* f = reinterpret_cast<const float*>(
        base + index * accessorStride(model, accessor));
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

bool loadGlbCpu(const std::string& path, tinygltf::Model& model)
{
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    return loader.LoadBinaryFromFile(&model, &err, &warn, resolveAssetPath(path));
}

// Appends one node's mesh triangles (node-local if `xform` is identity, world
// if `xform` is the node's accumulated transform) with shift applied.
void appendNodeTriangles(const tinygltf::Model& model, int nodeIndex,
                         const glm::mat4& xform,
                         std::vector<CollisionTriangle>& out)
{
    if (nodeIndex < 0 || nodeIndex >= (int)model.nodes.size())
        return;
    const tinygltf::Node& node = model.nodes[nodeIndex];
    if (node.mesh < 0 || node.mesh >= (int)model.meshes.size())
        return;
    const tinygltf::Mesh& mesh = model.meshes[node.mesh];
    for (const tinygltf::Primitive& prim : mesh.primitives)
    {
        if (prim.mode != TINYGLTF_MODE_TRIANGLES)
            continue;
        auto it = prim.attributes.find("POSITION");
        if (it == prim.attributes.end())
            continue;
        const tinygltf::Accessor& pos = model.accessors[it->second];
        std::vector<glm::vec3> positions;
        if (prim.indices >= 0)
        {
            const tinygltf::Accessor& idx = model.accessors[prim.indices];
            positions.reserve(idx.count);
            for (size_t i = 0; i < idx.count; ++i)
                positions.push_back(readVec3(model, pos, readIndex(model, idx, i)));
        }
        else
        {
            positions.reserve(pos.count);
            for (size_t i = 0; i < pos.count; ++i)
                positions.push_back(readVec3(model, pos, i));
        }
        for (size_t i = 0; i + 2 < positions.size(); i += 3)
        {
            CollisionTriangle tri;
            tri.a = glm::vec3(xform * glm::vec4(positions[i + 0], 1.0f));
            tri.b = glm::vec3(xform * glm::vec4(positions[i + 1], 1.0f));
            tri.c = glm::vec3(xform * glm::vec4(positions[i + 2], 1.0f));
            glm::vec3 n = glm::cross(tri.b - tri.a, tri.c - tri.a);
            const float len = glm::length(n);
            if (len < 0.000001f)
                continue;
            tri.normal = n / len;
            out.push_back(tri);
        }
    }
}

void walkNodeTriangles(const tinygltf::Model& model, int nodeIndex,
                       const glm::mat4& parentWorld,
                       std::vector<CollisionTriangle>& out, int depth)
{
    if (depth > 64 || nodeIndex < 0 || nodeIndex >= (int)model.nodes.size())
        return;
    const tinygltf::Node& node = model.nodes[nodeIndex];
    const glm::mat4 world = parentWorld * nodeMatrix(node);
    appendNodeTriangles(model, nodeIndex, world, out);
    for (int child : node.children)
        walkNodeTriangles(model, child, world, out, depth + 1);
}

// ── Small per-path CPU caches (GLBs are immutable once on disk) ─────
struct CachedBody { std::vector<ActorMeshPart> parts; };
struct CachedWeapon { std::vector<CollisionTriangle> triangles; };

std::unordered_map<std::string, CachedBody>& bodyCache()
{
    static std::unordered_map<std::string, CachedBody> cache;
    return cache;
}

std::unordered_map<std::string, CachedWeapon>& weaponCache()
{
    static std::unordered_map<std::string, CachedWeapon> cache;
    return cache;
}

} // namespace

std::vector<ActorCollisionMesh> collectActorCollisionMeshes(Player& player)
{
    std::vector<ActorCollisionMesh> meshes = collectActorBodyCollisionMeshes(player);

    // Configured weapons use their JSON sphere/capsule representation in the
    // actor solver. Render-mesh triangles remain available as a fallback for
    // tests or weapons without an initialized collider config, but they are
    // too sharp and too expensive for the normal movement path.
    if ((!player.weaponCollisionDebug.valid || player.weaponCollisionDebug.usesJsonMesh) &&
        !player.weaponColliderMesh.empty())
    {
        ActorCollisionMesh mesh;
        mesh.label = "weapon";
        mesh.localTriangles = &player.weaponColliderMesh;
        mesh.previousTransform = player.previousWeaponModelTransform;
        mesh.desiredTransform = player.weaponModelTransform;
        mesh.affectsMovement = true;
        meshes.push_back(mesh);
    }

    // Future held objects / tools append here with the same representation.

    return meshes;
}

std::vector<ActorCollisionMesh> collectActorBodyCollisionMeshes(Player& player)
{
    std::vector<ActorCollisionMesh> meshes;
    meshes.reserve(player.physicalBody.parts.size());

    for (const PhysicalBodyPart& part : player.physicalBody.parts)
    {
        if (part.collider.triangles.empty())
            continue;
        ActorCollisionMesh mesh;
        mesh.label = part.name.c_str();
        mesh.localTriangles = &part.collider.triangles;
        mesh.previousTransform = part.previousWorldTransform;
        mesh.desiredTransform = part.worldTransform;
        mesh.affectsMovement = true;
        meshes.push_back(mesh);
    }

    return meshes;
}

void commitActorCollisionMeshes(Player& player)
{
    player.previousWeaponModelTransform = player.weaponModelTransform;
}

bool loadActorBodyMeshParts(const char* glbPath,
                            std::vector<ActorMeshPart>& out)
{
    out.clear();
    if (!glbPath || !*glbPath)
        return false;

    const std::string resolved = resolveAssetPath(glbPath);
    auto it = bodyCache().find(resolved);
    if (it != bodyCache().end())
    {
        out = it->second.parts;
        return !out.empty();
    }

    tinygltf::Model model;
    if (!loadGlbCpu(glbPath, model))
        return false;

    for (int i = 0; i < (int)model.nodes.size(); ++i)
    {
        const std::string& name = model.nodes[i].name;
        if (!isBodyPartName(name))
            continue;
        ActorMeshPart part;
        part.name = name;
        part.nodeIndex = i;
        appendNodeTriangles(model, i, glm::mat4(1.0f), part.triangles);
        if (part.triangles.empty())
            continue;
        part.localMin = glm::vec3(std::numeric_limits<float>::max());
        part.localMax = glm::vec3(-std::numeric_limits<float>::max());
        for (const CollisionTriangle& t : part.triangles)
            for (const glm::vec3& v : {t.a, t.b, t.c})
            {
                part.localMin = glm::min(part.localMin, v);
                part.localMax = glm::max(part.localMax, v);
            }
        out.push_back(std::move(part));
    }

    bodyCache()[resolved] = CachedBody{out};
    return !out.empty();
}

bool loadActorWeaponTriangles(const char* glbPath,
                              std::vector<CollisionTriangle>& out)
{
    out.clear();
    if (!glbPath || !*glbPath)
        return false;

    const std::string resolved = resolveAssetPath(glbPath);
    auto it = weaponCache().find(resolved);
    if (it != weaponCache().end())
    {
        out = it->second.triangles;
        return !out.empty();
    }

    tinygltf::Model model;
    if (!loadGlbCpu(glbPath, model))
        return false;

    if (!model.scenes.empty())
    {
        const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
        if (sceneIndex >= 0 && sceneIndex < (int)model.scenes.size())
        {
            for (int node : model.scenes[sceneIndex].nodes)
                walkNodeTriangles(model, node, glm::mat4(1.0f), out, 0);
        }
    }
    else
    {
        for (int i = 0; i < (int)model.nodes.size(); ++i)
            appendNodeTriangles(model, i, glm::mat4(1.0f), out);
    }

    weaponCache()[resolved] = CachedWeapon{out};
    return !out.empty();
}

bool ensureActorWeaponColliderMesh(Player& player, const char* glbPath)
{
    if (!glbPath || !*glbPath)
    {
        player.weaponColliderMesh.clear();
        player.weaponColliderMeshPath.clear();
        return false;
    }
    if (player.weaponColliderMeshPath == glbPath && !player.weaponColliderMesh.empty())
        return true;

    std::vector<CollisionTriangle> triangles;
    if (!loadActorWeaponTriangles(glbPath, triangles) || triangles.empty())
        return false;

    player.weaponColliderMesh = std::move(triangles);
    player.weaponColliderMeshPath = glbPath;
    player.previousWeaponModelTransform = player.weaponModelTransform;
    return true;
}

bool ensureActorWeaponColliderMeshFromEquipped(Player& player)
{
    if (player.equippedWeaponId.empty())
    {
        player.weaponColliderMesh.clear();
        player.weaponColliderMeshPath.clear();
        return false;
    }
    if (player.weaponCollisionDebug.usesJsonMesh)
        return !player.weaponColliderMesh.empty();

    // The viewmodel sets weaponModelTransform when a weapon model is active.
    // The identity placeholder (translation ~0) means "no world attachment", so
    // do not place unset weapon triangles at the world origin.
    if (glm::length(glm::vec3(player.weaponModelTransform[3])) < 0.001f)
        return false;

    const WeaponDefinition* def = WeaponRegistry::instance().get(player.equippedWeaponId);
    if (!def)
        return false;

    std::string modelPath = def->modelPath;
    const WeaponViewModelConfig* vmcfg = WeaponConfig::instance().get(def->id);
    if (vmcfg && !vmcfg->modelPath.empty())
        modelPath = vmcfg->modelPath;
    if (modelPath.empty())
        return false;

    return ensureActorWeaponColliderMesh(player, modelPath.c_str());
}

bool ensureActorBodyCollisionMesh(Player& player, const char* glbPath)
{
    if (!player.physicalBody.parts.empty())
        return true;
    if (!glbPath || !*glbPath)
        return false;
    return player.loadModelColliders(glbPath) && !player.physicalBody.parts.empty();
}

bool actorCollisionMeshSelfTest(std::string* outSummary)
{
    constexpr const char* kBodyPath =
        "assets/entity/player/default/mimita-char-no-animations-v4.glb";
    constexpr const char* kWeaponPath =
        "assets/objects/weapons/mimita-revolver-v1.glb";

    std::string report;
    bool ok = true;
    auto check = [&](bool cond, const char* name) {
        report += cond ? "  PASS: " : "  FAIL: ";
        report += name;
        report += "\n";
        if (!cond) ok = false;
    };

    Player actor(false);
    actor.pos = glm::vec3(0.0f, 0.0f, 1.0f);

    const bool bodyOk = ensureActorBodyCollisionMesh(actor, kBodyPath);
    check(bodyOk, "headless actor loads body-part collision triangles");
    check(actor.physicalBody.parts.size() == 6, "actor has 6 body parts");

    const bool weaponOk = ensureActorWeaponColliderMesh(actor, kWeaponPath);
    check(weaponOk, "actor loads weapon render-mesh triangles");

    actor.weaponModelTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.0f, 1.0f));
    commitActorCollisionMeshes(actor);

    {
        std::vector<ActorCollisionMesh> meshes = collectActorCollisionMeshes(actor);
        check(meshes.size() == 7, "collector returns 6 body + 1 weapon mesh");
        bool allTriangles = !meshes.empty();
        int weaponCount = 0;
        for (const ActorCollisionMesh& m : meshes) {
            if (!m.localTriangles || m.localTriangles->empty())
                allTriangles = false;
            if (m.label && std::strcmp(m.label, "weapon") == 0)
                ++weaponCount;
        }
        check(allTriangles, "every collected mesh has triangles");
        check(weaponCount == 1, "weapon uses the same actor-mesh representation");
    }

    // Move the actor and the weapon one tick. Body and weapon desired transforms
    // must differ from their previous (sweep-start) transforms.
    actor.pos += glm::vec3(1.0f, 0.0f, 0.0f);
    actor.updateModelWorldTransforms();
    actor.weaponModelTransform = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 1.0f));

    {
        std::vector<ActorCollisionMesh> meshes = collectActorCollisionMeshes(actor);
        bool bodyMoved = false;
        bool weaponMoved = false;
        for (const ActorCollisionMesh& m : meshes) {
            const glm::vec3 prevT(m.previousTransform[3]);
            const glm::vec3 desiredT(m.desiredTransform[3]);
            if (glm::length(desiredT - prevT) > 0.1f) {
                if (m.label && std::strcmp(m.label, "weapon") == 0)
                    weaponMoved = true;
                else
                    bodyMoved = true;
            }
        }
        check(bodyMoved, "body part desired transform differs from sweep start");
        check(weaponMoved, "weapon desired transform differs from sweep start");
    }

    if (outSummary)
        *outSummary = report;
    return ok;
}
