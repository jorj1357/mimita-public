// 09 14 2026
/* purpose
* movement.main: the single local-player movement step, owned as a hot generic
* runtime system. It self-registers through HotPackageBuilder, so this file can
* be added, edited, renamed, split, deleted, or replaced live without a new EXE
* slot. It reads the local player's components, computes a movement step, asks
* the kernel for a capsule-vs-world solve (physics.moveCapsule), and publishes
* the result through the movement-override capability (kernel applies it and
* skips the built-in step).
* It is the ONLY movement path: the kernel's built-in step no longer runs.
* Presets live in the hot C++ registry (hot-movement-presets.h): source,
* default, heavy, retrograd_fast, counterstrike. The global active preset is a
* C++ constant; per-actor presets come from the generic ActorProfileState
* component. Config JSON is reference/comparison data, never the owner.
* Does NOT own entity storage, collision, rendering, or authority.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"
#include "hot-reload/hot-movement-fired.h"
#include "hot-reload/hot-presentation.h"
#include "hot-reload/packages/collision/collision-log.h"
#include "hot-reload/hot-movement-policy.h"
#include "hot-reload/hot-movement-preset-log.h"
#include "hot-reload/hot-movement-presets.h"
#include "hot-reload/hot-package.h"
#include "hot-reload/hot-pose.h"
#include "hot-reload/packages/collision/collision-abi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

// movement.tuning: cold callers (server, NPC, prediction setup, validation)
// request a preset's tuning by id. The hot handler explicitly selects either
// the C++ table or the JSON preset and fills the same POD tuning envelope.
void MIMITA_GAME_CALL onMovementTuning(void* host, const GameEventV1* event)
{
    auto* t = event ? static_cast<GameMovementTuningV1*>(event->payload) : nullptr;
    if (!t)
        return;
    const auto id = t->presetId < MimitaHotMovement::kMovementPresetCount
                        ? static_cast<MimitaHotMovement::MovementPresetId>(t->presetId)
                        : MimitaHotMovement::kActiveMovementPreset;
    const MimitaHotMovement::MovementPreset& preset =
        MimitaHotMovement::getMovementPreset(id);
    std::string jsonPreset;
    const auto source = MimitaHotMovement::movementBehaviorSourceFromJson(&jsonPreset);
    GameMovementTuningV1 selected = preset.tuning;
    if (source == MimitaHotMovement::MovementBehaviorSource::Json) {
        GameMovementTuningV1 jsonTuning{};
        // Load the JSON for the REQUESTED preset, not the globally selected one,
        // so per-actor/role presets resolve correctly.
        if (MimitaHotMovement::loadJsonMovementPreset(preset.name, jsonTuning))
            selected = jsonTuning;
    }
    *t = selected;
    t->presetId = static_cast<std::uint32_t>(id);
    t->handled = 1u;
    t->reserved = 0u;

    static MimitaHotMovement::MovementPresetTuningLogState sTuningLog;
    const std::uint64_t tick = event->tick;
    MimitaHotMovement::movementPresetLogTuning(host, sTuningLog, id, tick, tick);

    static const char* lastLoggedMode = nullptr;
    if (lastLoggedMode != preset.name) {
        lastLoggedMode = preset.name;
        std::printf("[MOVEMENT TUNING] source=%s preset=%s authority=shared-hot-movement\n",
                    MimitaHotMovement::movementBehaviorSourceName(source),
                    source == MimitaHotMovement::MovementBehaviorSource::Json && !jsonPreset.empty()
                        ? jsonPreset.c_str() : preset.name);
    }
}

// Reads the per-actor preset from the generic ActorProfileState component
// (movementPresetHash = gameHash(preset name)). Absent/unknown -> active preset.
MimitaHotMovement::MovementPresetId actorMovementPreset(GameplayContextV1* ctx,
                                                        std::uint64_t entity,
                                                        bool* outFromProfile = nullptr)
{
    if (outFromProfile)
        *outFromProfile = false;
    if (!ctx->dynamicReadComponent)
        return MimitaHotMovement::kActiveMovementPreset;
    struct ActorProfileStateV1 {
        std::uint64_t movementPresetHash;
        std::uint64_t behaviorProfileHash;
        std::uint32_t flags;
        std::uint32_t reserved;
    };
    ActorProfileStateV1 profile{};
    if (!ctx->dynamicReadComponent(ctx->host, entity, gameHash("ActorProfileState"),
                                   &profile, sizeof(profile)) ||
        profile.movementPresetHash == 0u)
        return MimitaHotMovement::kActiveMovementPreset;
    if (outFromProfile)
        *outFromProfile = true;
    return MimitaHotMovement::movementPresetIdFromHash(profile.movementPresetHash);
}

// Captured each tick so terminal commands (host == nullptr) can reach shared
// state, matching the editor module's command pattern.
GameSharedStateV1* gShared = nullptr;

GameSharedStateV1* sharedState(GameplayContextV1* ctx)
{
    if (!ctx || !ctx->permanentStorage ||
        ctx->permanentStorageSize < sizeof(GameSharedStateV1))
        return nullptr;
    GameSharedStateV1* shared =
        reinterpret_cast<GameSharedStateV1*>(ctx->permanentStorage);
    return shared->magic == GAME_SHARED_MAGIC ? shared : nullptr;
}

using PhysicsMoveFn = void (MIMITA_GAME_CALL *)(void*, MovementStateV1*, float, std::uint32_t);
using EffectSpawnFn = void (MIMITA_GAME_CALL *)(void*, const GameEffectSpawnV1*);

// Weapon collision shape from config/weaponcollisions.json, keyed by
// gameHash(weaponId). A shape picker selects the mode: the actual weapon
// triangles (default; resolved elsewhere), a single sphere, an oriented capsule
// (also used for cylinder / elongated-sphere modes), multiple spheres, or a box
// approximation. Live-editable: re-read when the file mtime changes.
struct WeaponShapeV1 {
    enum class Mode { Triangles, Sphere, Capsule, Spheres, Box };
    Mode mode = Mode::Triangles;
    float radius = 0.0f;
    float start[3] = {0.0f, 0.0f, 0.0f};
    float end[3] = {0.0f, 0.0f, 0.0f};
    float extents[3] = {0.0f, 0.0f, 0.0f};
    struct Sphere {
        float pos[3];
        float radius;
    };
    Sphere spheres[8];
    int sphereCount = 0;
    // Local-space triangle mesh for the `triangles` mode. Collision uses the
    // actual mesh vertices (sampled as small spheres) transformed by the hot
    // attachment pose, so the real weapon geometry drives world contact.
    float triangles[8][3][3];
    int triangleCount = 0;
};

WeaponShapeV1::Mode weaponShapeModeFromName(const std::string& name)
{
    if (name == "sphere") return WeaponShapeV1::Mode::Sphere;
    if (name == "capsule") return WeaponShapeV1::Mode::Capsule;
    if (name == "cylinder") return WeaponShapeV1::Mode::Capsule;
    if (name == "elongated_sphere") return WeaponShapeV1::Mode::Capsule;
    if (name == "capsules") return WeaponShapeV1::Mode::Capsule;
    if (name == "spheres") return WeaponShapeV1::Mode::Spheres;
    if (name == "box") return WeaponShapeV1::Mode::Box;
    return WeaponShapeV1::Mode::Triangles;
}

bool weaponShapeFor(std::uint64_t toolKey, WeaponShapeV1& out)
{
    struct Cache {
        std::uint64_t write = 0;
        bool loaded = false;
        std::unordered_map<std::uint64_t, WeaponShapeV1> map;
    };
    static Cache cache;

    std::error_code ec;
    const auto ft = std::filesystem::last_write_time(
        "config/weaponcollisions.json", ec);
    const std::uint64_t write =
        ec ? 0ull : static_cast<std::uint64_t>(ft.time_since_epoch().count());
    if (!cache.loaded || write != cache.write) {
        cache.map.clear();
        cache.loaded = true;
        cache.write = write;
        std::ifstream file("config/weaponcollisions.json");
        if (file) {
            try {
                const auto root =
                    nlohmann::json::parse(file, nullptr, true, true);
                auto readVec3 = [](const nlohmann::json& v, float outv[3]) {
                    if (v.is_array())
                        for (int k = 0; k < 3 && k < (int)v.size(); ++k)
                            outv[k] = v[k].get<float>();
                };
                auto readCapsule = [&](const nlohmann::json& cap,
                                       WeaponShapeV1& shape) {
                    if (!cap.is_object())
                        return false;
                    if (cap.contains("enabled") && cap["enabled"].is_boolean() &&
                        !cap["enabled"].get<bool>())
                        return false;
                    const float r = cap.value("radius", 0.0f);
                    if (r <= 0.0f)
                        return false;
                    shape.mode = WeaponShapeV1::Mode::Capsule;
                    shape.radius = r;
                    readVec3(cap.value("start", nlohmann::json::array()),
                             shape.start);
                    readVec3(cap.value("end", nlohmann::json::array()),
                             shape.end);
                    return true;
                };
                for (auto it = root.begin(); it != root.end(); ++it) {
                    if (!it.value().is_object())
                        continue;
                    const auto& entry = it.value();
                    if (entry.contains("enabled") &&
                        entry["enabled"].is_boolean() &&
                        !entry["enabled"].get<bool>())
                        continue;

                    WeaponShapeV1 shape{};
                    const std::string modeName = entry.value("shape", "");
                    shape.mode = weaponShapeModeFromName(modeName);
                    bool ok = false;
                    // Explicit triangle mesh (the default `triangles` mode).
                    if (entry.contains("triangles") &&
                        entry["triangles"].is_array()) {
                        shape.mode = WeaponShapeV1::Mode::Triangles;
                        for (const auto& tri : entry["triangles"]) {
                            if (shape.triangleCount >= 8)
                                break;
                            if (!tri.is_array() || tri.size() < 3)
                                continue;
                            bool okTri = true;
                            for (int v = 0; v < 3 && okTri; ++v) {
                                if (!tri[v].is_array() || tri[v].size() < 3) {
                                    okTri = false;
                                    break;
                                }
                                for (int k = 0; k < 3; ++k)
                                    shape.triangles[shape.triangleCount][v][k] =
                                        tri[v][k].get<float>();
                            }
                            if (okTri)
                                ++shape.triangleCount;
                        }
                        if (shape.triangleCount > 0) {
                            shape.radius = entry.value("radius", 0.03f);
                            ok = true;
                        }
                    }
                    if (shape.mode == WeaponShapeV1::Mode::Triangles) {
                        // Infer from the legacy fields when no explicit shape.
                        if (entry.contains("spheres"))
                            shape.mode = WeaponShapeV1::Mode::Spheres;
                        else if (entry.contains("capsules"))
                            shape.mode = WeaponShapeV1::Mode::Capsule;
                        else if (entry.value("source", "capsule") == "capsule")
                            shape.mode = WeaponShapeV1::Mode::Capsule;
                    }
                    switch (shape.mode) {
                    case WeaponShapeV1::Mode::Capsule:
                        if (entry.contains("capsule"))
                            ok = readCapsule(entry["capsule"], shape);
                        if (!ok && entry.contains("capsules") &&
                            entry["capsules"].is_array() &&
                            !entry["capsules"].empty())
                            ok = readCapsule(entry["capsules"][0], shape);
                        break;
                    case WeaponShapeV1::Mode::Sphere: {
                        const float r = entry.value("radius", 0.0f);
                        if (r > 0.0f) {
                            shape.radius = r;
                            readVec3(entry.value("center",
                                                 nlohmann::json::array()),
                                     shape.start);
                            shape.end[0] = shape.start[0];
                            shape.end[1] = shape.start[1];
                            shape.end[2] = shape.start[2];
                            ok = true;
                        }
                        break;
                    }
                    case WeaponShapeV1::Mode::Spheres:
                        if (entry.contains("spheres") &&
                            entry["spheres"].is_array()) {
                            for (const auto& sp : entry["spheres"]) {
                                if (shape.sphereCount >= 8)
                                    break;
                                WeaponShapeV1::Sphere s{};
                                s.radius = sp.value("radius", 0.0f);
                                readVec3(sp.value("position",
                                                  nlohmann::json::array()),
                                         s.pos);
                                if (s.radius > 0.0f)
                                    shape.spheres[shape.sphereCount++] = s;
                            }
                            ok = shape.sphereCount > 0;
                        }
                        break;
                    case WeaponShapeV1::Mode::Box: {
                        const float r = entry.value("radius", 0.0f);
                        readVec3(entry.value("extents",
                                             nlohmann::json::array()),
                                 shape.extents);
                        shape.radius = r > 0.0f ? r : 0.1f;
                        ok = shape.extents[0] > 0.0f || shape.extents[1] > 0.0f ||
                             shape.extents[2] > 0.0f;
                        break;
                    }
                    default:
                        break;
                    }
                    if (ok)
                        cache.map[gameHash(it.key().c_str())] = shape;
                }
            } catch (...) {
            }
        }
    }

    const auto found = cache.map.find(toolKey);
    if (found == cache.map.end())
        return false;
    out = found->second;
    return true;
}

// Which per-limb source the collider builder used this tick (diagnostic).
const char* g_limbSource = "none";

// The player collider list: the movement/smoothing capsule plus the head,
// torso, arms, and legs resolved from the skeleton. The caller supplies the
// generic shape description; the collision package owns the solve.
void buildPlayerCollision(
    GameplayContextV1* ctx, MovementStateV1* st, float dt, std::uint64_t entity,
    std::uint64_t tick, HotCollisionPackage::CollisionSolveV1& q)
{
    using namespace HotCollisionPackage;
    q = CollisionSolveV1{};
    q.entityId = entity;
    q.tick = tick;
    q.dt = dt;
    q.yaw = st->yaw;
    q.sizeScale = st->sizeScale > 0.0f ? st->sizeScale : 1.0f;
    q.mask = COLLISION_MASK_WORLD;
    q.flags = COLLISION_SOLVE_SPAWN_IMPACTS;
    // Identity/timing for live records. The local human is a player; the frame
    // and client tick come from the kernel context when available.
    q.actorKind = HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER;
    q.frame = ctx->tick;
    q.clientTick = ctx->tick;
    q.serverTick = 0;
    for (int i = 0; i < 3; ++i) {
        q.position[i] = st->position[i];
        q.velocity[i] = st->velocity[i];
    }

    CollisionColliderV1& capsule = q.colliders[q.colliderCount++];
    capsule.partId = COLLISION_PART_CAPSULE;
    capsule.shape = COLLISION_SHAPE_CAPSULE;
    capsule.policyId = COLLISION_POLICY_CAPSULE;
    capsule.flags = COLLISION_COLLIDER_HELPER;
    capsule.radius = st->radius;
    capsule.halfHeight = st->halfHeight;
    for (int i = 0; i < 3; ++i)
        capsule.position[i] = st->position[i];

    // Weapon collider: the equipped tool's resolved hot attachment position
    // becomes a body-authoritative collider so the weapon cannot pass through
    // surfaces and participates in root correction. Shape data will move to the
    // weapon collision config; a conservative sphere is the first slice.
    if (ctx->dynamicReadComponent &&
        q.colliderCount < COLLISION_MAX_COLLIDERS) {
        HotToolClaimV1 claim{};
        if (ctx->dynamicReadComponent(ctx->host, entity,
                                      HOT_TOOL_CLAIM_COMPONENT, &claim,
                                      sizeof(claim)) &&
            claim.toolEntity != 0u) {
            HotAttachmentStateV1 att{};
            if (ctx->dynamicReadComponent(ctx->host, claim.toolEntity,
                                          HOT_ATTACHMENT_COMPONENT, &att,
                                          sizeof(att)) &&
                att.resolved != 0u) {
                // Weapon collision shape picker from config: triangles (fallback
                // approximation), sphere, oriented capsule (also cylinder /
                // elongated sphere), multiple spheres, or a box approximation.
                // All shapes are placed by the hot attachment pose so the
                // collider follows the visible weapon.
                const float s = q.sizeScale;
                const glm::quat rot(att.worldRotation[3], att.worldRotation[0],
                                    att.worldRotation[1], att.worldRotation[2]);
                const glm::vec3 origin(att.worldPosition[0], att.worldPosition[1],
                                       att.worldPosition[2]);
                auto toWorld = [&](const float local[3]) {
                    return origin + rot * glm::vec3(local[0] * s, local[1] * s,
                                                    local[2] * s);
                };
                auto addWeaponSphere = [&](const glm::vec3& p, float r) {
                    if (q.colliderCount >= COLLISION_MAX_COLLIDERS)
                        return;
                    CollisionColliderV1& c = q.colliders[q.colliderCount++];
                    c.partId = COLLISION_PART_WEAPON;
                    c.shape = COLLISION_SHAPE_SPHERE;
                    c.policyId = COLLISION_POLICY_WEAPON;
                    c.flags = COLLISION_COLLIDER_BODY_AUTHORITATIVE;
                    c.radius = r * s;
                    c.position[0] = p.x;
                    c.position[1] = p.y;
                    c.position[2] = p.z;
                };
                auto addWeaponCapsule = [&](const glm::vec3& a, const glm::vec3& b,
                                            float r) {
                    if (q.colliderCount >= COLLISION_MAX_COLLIDERS)
                        return;
                    CollisionColliderV1& c = q.colliders[q.colliderCount++];
                    c.partId = COLLISION_PART_WEAPON;
                    c.shape = COLLISION_SHAPE_CAPSULE;
                    c.policyId = COLLISION_POLICY_WEAPON;
                    c.flags = COLLISION_COLLIDER_BODY_AUTHORITATIVE |
                              COLLISION_COLLIDER_ORIENTED_CAPSULE;
                    c.radius = r * s;
                    c.position[0] = a.x;
                    c.position[1] = a.y;
                    c.position[2] = a.z;
                    c.endPosition[0] = b.x;
                    c.endPosition[1] = b.y;
                    c.endPosition[2] = b.z;
                };

                WeaponShapeV1 shape{};
                if (weaponShapeFor(claim.toolKey, shape)) {
                    switch (shape.mode) {
                    case WeaponShapeV1::Mode::Capsule:
                        addWeaponCapsule(toWorld(shape.start), toWorld(shape.end),
                                         shape.radius);
                        break;
                    case WeaponShapeV1::Mode::Sphere:
                        addWeaponSphere(toWorld(shape.start), shape.radius);
                        break;
                    case WeaponShapeV1::Mode::Spheres:
                        for (int k = 0; k < shape.sphereCount; ++k)
                            addWeaponSphere(toWorld(shape.spheres[k].pos),
                                            shape.spheres[k].radius);
                        break;
                    case WeaponShapeV1::Mode::Box: {
                        // Approximate a box with two spheres along its longest axis.
                        int axis = 0;
                        for (int k = 1; k < 3; ++k)
                            if (shape.extents[k] > shape.extents[axis])
                                axis = k;
                        float a[3] = {0.0f, 0.0f, 0.0f};
                        float b[3] = {0.0f, 0.0f, 0.0f};
                        a[axis] = -shape.extents[axis];
                        b[axis] = shape.extents[axis];
                        addWeaponSphere(toWorld(a), shape.radius);
                        addWeaponSphere(toWorld(b), shape.radius);
                        break;
                    }
                    case WeaponShapeV1::Mode::Triangles:
                        if (shape.triangleCount > 0) {
                            // Vertex-sampled mesh collision: each triangle
                            // vertex becomes a small sphere collider, so the
                            // real weapon geometry blocks the world.
                            for (int t = 0; t < shape.triangleCount; ++t)
                                for (int v = 0; v < 3; ++v)
                                    addWeaponSphere(
                                        toWorld(shape.triangles[t][v]),
                                        shape.radius);
                        } else {
                            addWeaponSphere(origin, 0.18f);
                        }
                        break;
                    default:
                        addWeaponSphere(origin, 0.18f);
                        break;
                    }
                } else {
                    addWeaponSphere(origin, 0.18f);
                }
            }
        }
    }

    if (!ctx->resolveCapability ||
        q.colliderCount >= COLLISION_MAX_COLLIDERS)
        return;

    // ── afad20a per-limb source: the animated physical body ────────────────
    // Use Player::physicalBody.parts (the exact body the renderer draws) and
    // each part's real collider AABB. The full world matrix is used because the
    // part transform carries model scale, so the collider centre must be
    // transformed by the matrix, not by rotation alone. Each limb becomes an
    // oriented capsule spanning the whole AABB (not a single small sphere), so
    // arms/legs/torso cannot slip into geometry.
    auto bodyFn = reinterpret_cast<GameBodyPartsFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_BODY_PARTS));
    if (bodyFn) {
        GameBodyPartsV1 bp{};
        bp.entity = entity;
        if (bodyFn(ctx->host, &bp) && bp.valid != 0u && bp.count > 0u &&
            bp.parts[0].space == 1u) {
            g_limbSource = "body.parts";
            auto partIdForHash = [](std::uint64_t h) -> std::uint32_t {
                if (h == gameHash("head")) return COLLISION_PART_HEAD;
                if (h == gameHash("torso")) return COLLISION_PART_TORSO;
                if (h == gameHash("leftArm")) return COLLISION_PART_LEFT_ARM;
                if (h == gameHash("rightArm")) return COLLISION_PART_RIGHT_ARM;
                if (h == gameHash("leftLeg")) return COLLISION_PART_LEFT_LEG;
                if (h == gameHash("rightLeg")) return COLLISION_PART_RIGHT_LEG;
                return COLLISION_PART_TORSO;
            };
            const std::uint32_t n = std::min(
                bp.count, static_cast<std::uint32_t>(GAME_MAX_BODY_PARTS));
            for (std::uint32_t i = 0; i < n; ++i) {
                if (q.colliderCount >= COLLISION_MAX_COLLIDERS)
                    break;
                const GameBodyPartV1& part = bp.parts[i];
                if (part.space != 1u)
                    continue;
                glm::mat4 wm(1.0f), pwm(1.0f);
                for (int c = 0; c < 4; ++c)
                    for (int r = 0; r < 4; ++r) {
                        wm[c][r] = part.worldMatrix[c * 4 + r];
                        pwm[c][r] = part.previousWorldMatrix[c * 4 + r];
                    }
                const glm::vec3 mn(part.boundsMin[0], part.boundsMin[1],
                                   part.boundsMin[2]);
                const glm::vec3 mx(part.boundsMax[0], part.boundsMax[1],
                                   part.boundsMax[2]);
                const glm::vec3 localCenter = (mn + mx) * 0.5f;
                const glm::vec3 extents = (mx - mn) * 0.5f;
                // Model scale lives in the matrix; apply it to the radius too.
                float scale = glm::length(glm::vec3(wm[0]));
                if (!(scale > 1e-6f))
                    scale = 1.0f;
                // Dominant (longest) local axis becomes the capsule axis.
                int axis = 0;
                if (extents[1] > extents[axis]) axis = 1;
                if (extents[2] > extents[axis]) axis = 2;
                glm::vec3 axisDir(0.0f);
                axisDir[axis] = 1.0f;
                const float halfLen = extents[axis];
                const float r1 = extents[(axis + 1) % 3];
                const float r2 = extents[(axis + 2) % 3];
                const float localRadius = std::max(r1, r2);
                float radius = localRadius * scale;
                if (!(radius > 1e-4f))
                    radius = 0.05f * scale;
                radius = std::min(radius, 0.6f);
                // Inset the capsule endpoints by the radius so the capsule's
                // lowest point matches the AABB's lowest point. Without this the
                // rounded caps extend `radius` below the AABB and the settle
                // lifts the visible body by that amount (the "idle float").
                const float localSegHalf = std::max(0.0f, halfLen - localRadius);
                const glm::vec3 a = glm::vec3(
                    wm * glm::vec4(localCenter - axisDir * localSegHalf, 1.0f));
                const glm::vec3 b = glm::vec3(
                    wm * glm::vec4(localCenter + axisDir * localSegHalf, 1.0f));
                const glm::vec3 pa = glm::vec3(
                    pwm * glm::vec4(localCenter - axisDir * localSegHalf, 1.0f));
                const glm::vec3 pb = glm::vec3(
                    pwm * glm::vec4(localCenter + axisDir * localSegHalf, 1.0f));
                CollisionColliderV1& c = q.colliders[q.colliderCount++];
                c.partId = partIdForHash(part.part);
                c.policyId = COLLISION_POLICY_BODY;
                c.flags = COLLISION_COLLIDER_BODY_AUTHORITATIVE;
                c.radius = radius;
                if (glm::length(b - a) > 1e-3f) {
                    c.shape = COLLISION_SHAPE_CAPSULE;
                    c.flags |= COLLISION_COLLIDER_ORIENTED_CAPSULE;
                    c.position[0] = a.x; c.position[1] = a.y; c.position[2] = a.z;
                    c.endPosition[0] = b.x;
                    c.endPosition[1] = b.y;
                    c.endPosition[2] = b.z;
                    const glm::vec3 sweep = (a + b) * 0.5f - (pa + pb) * 0.5f;
                    c.velocity[0] = sweep.x;
                    c.velocity[1] = sweep.y;
                    c.velocity[2] = sweep.z;
                } else {
                    c.shape = COLLISION_SHAPE_SPHERE;
                    c.position[0] = a.x; c.position[1] = a.y; c.position[2] = a.z;
                    const glm::vec3 sweep = a - pa;
                    c.velocity[0] = sweep.x;
                    c.velocity[1] = sweep.y;
                    c.velocity[2] = sweep.z;
                }
            }
            return;
        }
    }

    // ── Fallback: socket + mesh bounds (remote/NPC or unloaded model) ──────
    g_limbSource = "socket";
    auto rawFn = reinterpret_cast<GameSocketRawFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SOCKET_RAW));
    if (!rawFn)
        return;
    auto boundsFn = reinterpret_cast<GameMeshPartBoundsFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_MESH_PART_BOUNDS));

    static const struct {
        std::uint32_t part;
        const char* name;
        float radius;
        float halfLength;
    }
        kParts[] = {
            {COLLISION_PART_HEAD, "head", 0.30f, 0.12f},
            {COLLISION_PART_TORSO, "torso", 0.42f, 0.30f},
            {COLLISION_PART_LEFT_ARM, "leftArm", 0.17f, 0.24f},
            {COLLISION_PART_RIGHT_ARM, "rightArm", 0.17f, 0.24f},
            {COLLISION_PART_LEFT_LEG, "leftLeg", 0.20f, 0.32f},
            {COLLISION_PART_RIGHT_LEG, "rightLeg", 0.20f, 0.32f},
        };
    const float s = q.sizeScale;
    glm::mat4 root = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(st->position[0], st->position[1], st->position[2]));
    root *= glm::rotate(glm::mat4(1.0f), glm::radians(st->yaw),
                        glm::vec3(0.0f, 0.0f, 1.0f));
    for (const auto& p : kParts) {
        if (q.colliderCount + 3u > COLLISION_MAX_COLLIDERS)
            break;
        GameSocketRawV1 r{};
        r.entity = entity;
        r.socket = gameHash(p.name);
        if (!rawFn(ctx->host, &r) || !r.valid)
            continue;
        const glm::vec3 local(r.position[0] * s, r.position[1] * s,
                              r.position[2] * s);
        const glm::vec3 center = glm::vec3(root * glm::vec4(local, 1.0f));
        const glm::quat socketRotation(r.rotation[3], r.rotation[0],
                                       r.rotation[1], r.rotation[2]);
        // afad20a used the exact mesh-node AABB, not a guessed socket length.
        // The bounds capability is read-only asset data; all sampling policy
        // remains here in the hot DLL.
        glm::vec3 localMin(-p.halfLength * p.radius,
                           -p.radius, -p.radius);
        glm::vec3 localMax(p.halfLength * p.radius,
                           p.radius, p.radius);
        bool exactBounds = false;
        if (boundsFn) {
            GameMeshPartBoundsV1 b{};
            b.entity = entity;
            b.part = r.socket;
            exactBounds = boundsFn(ctx->host, &b) && b.valid != 0;
            if (exactBounds) {
                for (int k = 0; k < 3; ++k) {
                    localMin[k] = b.boundsMin[k];
                    localMax[k] = b.boundsMax[k];
                }
            }
        }
        const glm::vec3 localCenter = (localMin + localMax) * 0.5f;
        const glm::vec3 localExtents = (localMax - localMin) * 0.5f;
        glm::vec3 axisDir = localExtents;
        float axisLen = glm::length(axisDir);
        if (axisLen > 0.001f) axisDir /= axisLen;
        else { axisDir = glm::vec3(0, 0, 1); axisLen = p.halfLength; }
        const glm::vec3 worldCenter = glm::vec3(root * glm::vec4(
            glm::vec3(r.position[0], r.position[1], r.position[2]) +
            socketRotation * (localCenter * s), 1.0f));
        const glm::vec3 axis = glm::vec3(root * glm::vec4(
            socketRotation * (axisDir * axisLen * s), 0.0f));
        float radius = exactBounds
            ? std::max(0.035f, std::min(localExtents.x, localExtents.y) * 1.5f * s)
            : p.radius * s;
        radius = std::min(radius, 0.35f * s);
        for (int sample = 0; sample < 3; ++sample) {
            const float t = static_cast<float>(sample) * 0.5f - 0.5f;
            const glm::vec3 world = worldCenter + axis * t;
            CollisionColliderV1& c = q.colliders[q.colliderCount++];
            c.partId = p.part;
            c.shape = COLLISION_SHAPE_SPHERE;
            c.policyId = COLLISION_POLICY_BODY;
            c.flags = COLLISION_COLLIDER_BODY_AUTHORITATIVE;
            c.radius = radius;
            c.position[0] = world.x;
            c.position[1] = world.y;
            c.position[2] = world.z;
        }
    }
}

// ── Live collision diagnostics (events.jsonl) ───────────────────────────────
// One throttled record per second for the branch that did NOT solve, so a
// missing capability or a declined solve is visible while the game runs.
struct MovementBranchLog {
    float sinceLogSeconds = 0.0f;
};
MovementBranchLog& movementBranchLog()
{
    static MovementBranchLog b;
    return b;
}

void logMovementBranch(GameplayContextV1* ctx, const char* why,
                       const MovementStateV1* st, std::uint64_t entity,
                       std::uint64_t tick)
{
    MovementBranchLog& b = movementBranchLog();
    b.sinceLogSeconds += 1.0f / 60.0f;
    if (b.sinceLogSeconds < 1.0f)
        return;
    char msg[224];
    std::snprintf(msg, sizeof(msg),
                  "branch=%s entity=%llu pos=(%.2f %.2f %.2f) vz=%.2f "
                  "hasCapability=%d",
                  why, (unsigned long long)entity, st->position[0],
                  st->position[1], st->position[2], st->velocity[2],
                  (int)(ctx && ctx->resolveCapability != nullptr));
    HotCollisionPackage::collisionLogFull(
        ctx, 3u, "COLLISION", "movement.collision", msg, why, entity, entity,
        HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER, tick, 0, tick, tick);
    b.sinceLogSeconds = 0.0f;
}

void playActionSound(GameplayContextV1* ctx, std::uint64_t owner,
                     const float pos[3], const char* sound, float volume,
                     float pitch);

// afad20a applied the current pose before updating model transforms and
// collecting body samples. Re-publish the last hot pose at the collision
// boundary so the kernel updates its model-node transforms before the raw
// socket/bounds capabilities are queried. When no stored pose exists yet, an
// empty pose is still applied so `Player::updateModelWorldTransforms()` runs
// and the animated physical body (Player::physicalBody.parts) is populated for
// the per-limb collision source instead of staying stale/empty.
void applyStoredPoseBeforeCollision(GameplayContextV1* ctx,
                                    std::uint64_t entity)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    GameSkeletonPoseV1 pose{};
    pose.entity = entity;
    if (ctx->dynamicReadComponent) {
        HotPoseStateV1 state{};
        if (ctx->dynamicReadComponent(ctx->host, entity,
                                      HOT_POSE_STATE_COMPONENT, &state,
                                      sizeof(state))) {
            const std::uint32_t count = std::min(
                state.count, static_cast<std::uint32_t>(HOT_POSE_MAX_PARTS));
            pose.count = count;
            pose.flags = state.version;
            for (std::uint32_t i = 0; i < count; ++i) {
                pose.parts[i].part = state.part[i];
                for (int k = 0; k < 3; ++k) {
                    pose.parts[i].translation[k] = state.translation[i][k];
                    pose.parts[i].rotationEuler[k] = state.rotationEuler[i][k];
                }
            }
        }
    }
    auto apply = reinterpret_cast<GameSkeletonApplyFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_SKELETON_APPLY));
    if (apply)
        apply(ctx->host, &pose);
}

// Collision is owned by exactly one system: the collision package
// (`collision.main`). There is no second in-DLL solver; if the package is not
// available the actor keeps its plain-integrated velocity for one tick rather
// than being mutated by a competing owner.
void resolveCollisions(GameplayContextV1* ctx, MovementStateV1* st, float dt,
                       std::uint64_t entity, std::uint64_t tick)
{
    using namespace HotCollisionPackage;
    // `st->grounded` was set by the caller to the previous tick's value before
    // this call; capture it so the land sound fires only on the airborne ->
    // grounded transition (each landing), not every grounded tick.
    const bool wasGrounded = st->grounded != 0u;
    GameCollisionSolveFn fn = nullptr;
    if (ctx->resolveCapability)
        fn = reinterpret_cast<GameCollisionSolveFn>(
            ctx->resolveCapability(ctx->host, GAME_CAP_COLLISION));
    if (!fn) {
        for (int i = 0; i < 3; ++i)
            st->position[i] += st->velocity[i] * dt;
        st->grounded = 0;
        st->collided = 0;
        // The package capability is missing: no collision owner at all.
        logMovementBranch(ctx, "no_capability", st, entity, tick);
        return;
    }
    CollisionSolveV1 q;
    applyStoredPoseBeforeCollision(ctx, entity);
    buildPlayerCollision(ctx, st, dt, entity, tick, q);
    // The collision package resolves capabilities from the gameplay context, so
    // it receives `ctx` (the context), not `ctx->host` (the opaque kernel host).
    fn(ctx, &q);
    if (!q.handled) {
        // Package could not solve (for example the world is not bound yet):
        // integrate plainly and state the result explicitly, so grounded is
        // never left undefined and no second owner invents a result.
        for (int i = 0; i < 3; ++i)
            st->position[i] += st->velocity[i] * dt;
        st->grounded = 0;
        st->collided = 0;
        logMovementBranch(ctx, "declined", st, entity, tick);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        st->position[i] = q.outPosition[i];
        st->velocity[i] = q.outVelocity[i];
    }
    st->grounded = q.grounded;
    // afad20a universal reset: touching anything on the actor (capsule, limb,
    // weapon, tool) resets abilities. Any world/body contact this tick, the
    // sticky world-contact flag, or a returned contact all qualify.
    st->collided = (q.worldContact || q.bodyContact || q.contactCount > 0u)
                       ? 1u : 0u;

    // Contact consumer: collision.main is the source of truth for impacts. Use
    // the returned world contacts to play a throttled impact/land sound. Spark
    // and decal impacts are already produced by the package through
    // COLLISION_SOLVE_SPAWN_IMPACTS, so this adds only the audio consumer.
    {
        float maxIncoming = 0.0f;
        for (std::uint32_t h = 0;
             h < q.contactCount &&
             h < HotCollisionPackage::COLLISION_MAX_CONTACTS;
             ++h) {
            if (q.contacts[h].targetKind != 0u)
                continue;  // world contacts only
            if (q.contacts[h].incomingSpeed > maxIncoming)
                maxIncoming = q.contacts[h].incomingSpeed;
        }
        // afad20a landing: play the grounded sound only on the airborne ->
        // grounded transition (each landing), with no cooldown. This runs once
        // per tick, so it is naturally capped at one sound per tick; volume
        // scales with the impact speed.
        if (!wasGrounded && q.grounded) {
            const float volume = std::clamp(maxIncoming / 30.0f, 0.2f, 1.0f);
            playActionSound(ctx, entity, st->position, "entity/player/land",
                            volume, 1.0f);
        }
    }

    // Throttled branch record: proves which owner ran and the collider count
    // that was actually sent (capsule + resolved body parts), plus the actor
    // identity and the frame/client/server tick.
    MovementBranchLog& b = movementBranchLog();
    b.sinceLogSeconds += dt;
    if (b.sinceLogSeconds >= 1.0f) {
        char msg[320];
        std::uint32_t limbHits = 0;
        const CollisionColliderV1* firstLimb = nullptr;
        for (std::uint32_t i = 0; i < q.colliderCount &&
                                i < HotCollisionPackage::COLLISION_MAX_COLLIDERS;
             ++i) {
            const std::uint32_t part = q.colliders[i].partId;
            if (part >= COLLISION_PART_HEAD && part <= COLLISION_PART_RIGHT_LEG &&
                !firstLimb)
                firstLimb = &q.colliders[i];
        }
        for (std::uint32_t i = 0; i < q.contactCount &&
                                i < HotCollisionPackage::COLLISION_MAX_CONTACTS;
             ++i) {
            const std::uint32_t part = q.contacts[i].sourcePart;
            if (part >= COLLISION_PART_HEAD && part <= COLLISION_PART_RIGHT_LEG)
                ++limbHits;
        }
        std::snprintf(msg, sizeof(msg),
                      "branch=solved limbSrc=%s cols=%u limbHits=%u contacts=%u "
                      "grounded=%u wc=%u bc=%u bounced=%u limb0=(%.2f %.2f %.2f r=%.3f) "
                      "pos=(%.2f %.2f %.2f)",
                      g_limbSource, q.colliderCount, limbHits, q.contactCount,
                      q.grounded, q.worldContact, q.bodyContact, q.bounced,
                      firstLimb ? firstLimb->position[0] : 0.0f,
                      firstLimb ? firstLimb->position[1] : 0.0f,
                      firstLimb ? firstLimb->position[2] : 0.0f,
                      firstLimb ? firstLimb->radius : 0.0f,
                      q.outPosition[0], q.outPosition[1], q.outPosition[2]);
        HotCollisionPackage::collisionLogFull(
            ctx, 2u, "COLLISION", "movement.collision", msg,
            q.grounded ? "grounded" : ((q.worldContact || q.bodyContact)
                                           ? "contact"
                                           : "no_contact"),
            entity, entity,
            HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER, ctx->tick, 0,
            ctx->tick, tick);
        b.sinceLogSeconds = 0.0f;
    }
}

// Effects are one generic spawn descriptor resolved by id; movement just emits
// named kinds and the kernel maps them to pooled emitters.
void spawnEffect(GameplayContextV1* ctx, std::uint64_t kind, const float pos[3],
                 const float dir[3], float scale, float lifetime)
{
    if (!ctx->resolveCapability)
        return;
    auto fn = reinterpret_cast<EffectSpawnFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_EFFECT_SPAWN));
    if (!fn)
        return;
    GameEffectSpawnV1 d{};
    d.kind = kind;
    d.scale = scale > 0.0f ? scale : 1.0f;
    d.lifetime = lifetime;
    d.color[0] = 1.0f; d.color[1] = 1.0f; d.color[2] = 1.0f; d.color[3] = 1.0f;
    for (int i = 0; i < 3; ++i) {
        d.position[i] = pos[i];
        d.direction[i] = dir ? dir[i] : 0.0f;
    }
    fn(ctx->host, &d);
}

// Actions do not call feature-specific audio functions.  They publish one
// generic audio command and the kernel resolves the logical sound name through
// the existing audio player.  Keeping this at the shared movement owner makes
// dash/down-dash presentation identical for local prediction and hot actors.
void playActionSound(GameplayContextV1* ctx, std::uint64_t owner,
                     const float pos[3], const char* sound, float volume,
                     float pitch)
{
    if (!ctx || !ctx->resolveCapability || !sound || !sound[0])
        return;
    auto fn = reinterpret_cast<GameAudioPlayFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_AUDIO_PLAY));
    if (!fn)
        return;
    GameAudioCommandV1 command{};
    std::snprintf(command.sound, sizeof(command.sound), "%s", sound);
    command.position[0] = pos[0];
    command.position[1] = pos[1];
    command.position[2] = pos[2];
    command.volume = volume;
    command.pitch = pitch;
    command.maxDistance = 50.0f;
    command.spatial = 1u;
    command.action = 0u;
    command.ownerEntity = owner;
    command.slotId = 0u;
    command.op = GAME_AUDIO_PLAY_ONESHOT;
    command.loop = 0u;
    fn(ctx->host, &command);
}

void logAction(GameplayContextV1* ctx, std::uint64_t actor,
               std::uint32_t tick, const char* action, std::uint64_t actionId,
               bool availableAfter, const float position[3],
               const float velocity[3])
{
    if (!ctx || !ctx->resolveCapability || !action)
        return;
    auto fn = reinterpret_cast<GameLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return;
    GameLogEventV1 event{};
    event.level = 2u;
    event.simulationTick = tick;
    event.entityId = actor;
    event.actorId = actor;
    event.actorKind = 1u;
    event.frame = tick;
    std::snprintf(event.category, sizeof(event.category), "MOVEMENT");
    std::snprintf(event.name, sizeof(event.name), "movement.action");
    std::snprintf(event.message, sizeof(event.message),
                  "action=%s action_id=%llu available_after=%u pos=(%.3f %.3f %.3f) velocity=(%.3f %.3f %.3f)",
                  action, static_cast<unsigned long long>(actionId),
                  availableAfter ? 1u : 0u, position[0], position[1],
                  position[2], velocity[0], velocity[1], velocity[2]);
    std::snprintf(event.result, sizeof(event.result), "accepted");
    fn(ctx->host, &event);
}

// ── One generation-tracked movement tuning snapshot ─────────────────────────
// JSON is the active preset source when config/movement.json says so. A parse
// failure keeps the last valid JSON generation active; if none exists it falls
// back to the compiled C++ preset. Resolved once per tick and reported.
struct ResolvedTuning {
    GameMovementTuningV1 tuning{};
    MimitaHotMovement::MovementBehaviorSource source =
        MimitaHotMovement::MovementBehaviorSource::Cpp;
    std::string presetName;
    std::uint64_t presetHash = 0;
};

ResolvedTuning resolveTuning(MimitaHotMovement::MovementPresetId id)
{
    const MimitaHotMovement::MovementPreset& preset =
        MimitaHotMovement::getMovementPreset(id);
    ResolvedTuning r;
    r.presetName = preset.name;
    r.tuning = preset.tuning;

    std::string jsonPreset;
    r.source = MimitaHotMovement::movementBehaviorSourceFromJson(&jsonPreset);
    if (r.source == MimitaHotMovement::MovementBehaviorSource::Json) {
        const std::string name = preset.name;
        static std::string sLastGoodName;
        static GameMovementTuningV1 sLastGood{};
        static bool sHasLastGood = false;
        static std::uint64_t sLastStamp = 0;

        std::error_code ec;
        const auto ft = std::filesystem::last_write_time(
            MimitaHotMovement::movementPresetJsonPath(name), ec);
        std::uint64_t stamp =
            ec ? 0ull : static_cast<std::uint64_t>(ft.time_since_epoch().count());

        if (sHasLastGood && sLastGoodName == name && stamp != 0 && stamp == sLastStamp) {
            r.tuning = sLastGood;
            r.presetName = name;
        } else {
            GameMovementTuningV1 jsonTuning{};
            if (MimitaHotMovement::loadJsonMovementPreset(name, jsonTuning)) {
                sLastGood = jsonTuning;
                sHasLastGood = true;
                sLastGoodName = name;
                sLastStamp = stamp;
                r.tuning = jsonTuning;
                r.presetName = name;
            } else if (sHasLastGood && sLastGoodName == name) {
                // Invalid JSON: keep the previous valid generation active.
                r.tuning = sLastGood;
                r.presetName = name;
            }
        }
    }
    r.presetHash = gameHash(r.presetName.c_str());
    return r;
}

void logMovementSnapshot(GameplayContextV1* ctx, std::uint64_t entity,
                         std::uint64_t tick, const ResolvedTuning& r)
{
    if (!ctx || !ctx->resolveCapability)
        return;
    static float sinceLog = 1.0f;  // emit the first snapshot immediately
    sinceLog += 1.0f / 60.0f;
    if (sinceLog < 1.0f)
        return;
    sinceLog = 0.0f;
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "source=%s preset=%s generation=%llu preset_hash=%llu "
                  "simulation_tick=%llu",
                  MimitaHotMovement::movementBehaviorSourceName(r.source),
                  r.presetName.c_str(),
                  static_cast<unsigned long long>(ctx->generation),
                  static_cast<unsigned long long>(r.presetHash),
                  static_cast<unsigned long long>(tick));
    auto fn = reinterpret_cast<GameLogEventFn>(
        ctx->resolveCapability(ctx->host, GAME_CAP_LOG_EVENT));
    if (!fn)
        return;
    GameLogEventV1 event{};
    event.level = 2u;
    event.simulationTick = tick;
    event.entityId = entity;
    event.actorId = entity;
    event.actorKind = 1u;
    event.frame = tick;
    std::snprintf(event.category, sizeof(event.category), "MOVEMENT");
    std::snprintf(event.name, sizeof(event.name), "movement.snapshot");
    std::snprintf(event.message, sizeof(event.message), "%s", msg);
    std::snprintf(event.result, sizeof(event.result), "%s",
                  MimitaHotMovement::movementBehaviorSourceName(r.source));
    fn(ctx->host, &event);
}

void MIMITA_GAME_CALL movementMainTick(void* host, std::uint64_t tick, float dt)
{
    GameplayContextV1* ctx = static_cast<GameplayContextV1*>(host);
    if (!ctx || ctx->structSize < sizeof(GameplayContextV1))
        return;
    if (!ctx->readComponent || !ctx->writeComponent ||
        !ctx->requestMovementOverride)
        return;

    GameSharedStateV1* shared = sharedState(ctx);
    if (!shared || shared->localPlayerEntity == 0)
        return;
    gShared = shared;
    const bool createMode = (shared->modeFlags & GAME_MODE_FLAG_CREATION) != 0;

    const std::uint64_t e = shared->localPlayerEntity;

    GameTransformComponentV1 tf{};
    GameVelocityComponentV1 vl{};
    GameMovementIntentComponentV1 mi{};
    GameMovementRuntimeStateComponentV1 rs{};
    GameBodyComponentV1 body{};
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &tf, sizeof(tf)))
        return;
    ctx->readComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &vl, sizeof(vl));
    ctx->readComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_INTENT, &mi, sizeof(mi));
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_RUNTIME_STATE,
                            &rs, sizeof(rs)) ||
        rs.version != MOVEMENT_RUNTIME_STATE_VERSION) {
        rs = GameMovementRuntimeStateComponentV1{};
        rs.version = MOVEMENT_RUNTIME_STATE_VERSION;
        rs.airJumpsLeft = 1;
        rs.jumpAirJumpArmed = 1;
        rs.dashAvailable = 1;
        rs.downDashAvailable = 1;
        rs.freezeAvailable = 1;
    }
    if (!ctx->readComponent(ctx->host, e, GAME_COMPONENT_BODY, &body, sizeof(body))) {
        body.radius = 0.4f;
        body.height = 1.8f;
        body.sizeScale = 1.0f;
    }

    const float yaw = tf.yaw;

    MovementStateV1 st{};
    st.position[0] = tf.position[0];
    st.position[1] = tf.position[1];
    st.position[2] = tf.position[2];
    st.yaw = yaw;
    st.radius = body.radius > 0.0f ? body.radius : 0.4f;
    st.halfHeight = body.height > 0.0f ? body.height * 0.5f : 0.9f;
    st.sizeScale = body.sizeScale;

    bool presetFromProfile = false;
    const MimitaHotMovement::MovementPresetId presetId =
        actorMovementPreset(ctx, e, &presetFromProfile);
    const ResolvedTuning resolved = resolveTuning(presetId);
    const GameMovementTuningV1& m = resolved.tuning;
    MimitaHotMovement::movementPresetLogActor(
        ctx, e, MimitaHotMovement::MOVEMENT_LOG_ACTOR_PLAYER, presetId,
        presetFromProfile ? "actor-profile" : "active", tick, tick);
    logMovementSnapshot(ctx, e, tick, resolved);

    if (createMode) {
        // Free-fly / noclip: camera-relative movement, no gravity/collision.
        GameAimIntentComponentV1 aim{};
        ctx->readComponent(ctx->host, e, GAME_COMPONENT_AIM_INTENT, &aim, sizeof(aim));
        float f[3] = {aim.direction[0], aim.direction[1], aim.direction[2]};
        float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
        if (fl < 1e-4f) { f[0] = 0.0f; f[1] = 1.0f; f[2] = 0.0f; fl = 1.0f; }
        f[0] /= fl; f[1] /= fl; f[2] /= fl;
        float r[3] = {f[1], -f[0], 0.0f};
        float rl = std::sqrt(r[0] * r[0] + r[1] * r[1]);
        if (rl < 1e-4f) { r[0] = 1.0f; r[1] = 0.0f; }
        else { r[0] /= rl; r[1] /= rl; }
        const float vertical = (mi.jump ? 1.0f : 0.0f) - (mi.freeze ? 1.0f : 0.0f);
        const float speed = m.freeFlySpeed;
        st.velocity[0] = (r[0] * mi.moveX + f[0] * mi.moveY) * speed;
        st.velocity[1] = (r[1] * mi.moveX + f[1] * mi.moveY) * speed;
        st.velocity[2] = vertical * speed;
        st.position[0] += st.velocity[0] * dt;
        st.position[1] += st.velocity[1] * dt;
        st.position[2] += st.velocity[2] * dt;
        st.grounded = 0;
    } else {
        float vx = vl.linear[0];
        float vy = vl.linear[1];
        float vz = vl.linear[2];

        // movement intent is already camera-relative WORLD XY (input-poll.cpp),
        // so it must not be rotated by yaw again.
        const float wishX = mi.moveX;
        const float wishY = mi.moveY;
        const float wishLen = std::sqrt(wishX * wishX + wishY * wishY);
        const bool hasWish = mi.pressed && wishLen > 1e-4f;
        const float wishDirX = hasWish ? wishX / wishLen : 0.0f;
        const float wishDirY = hasWish ? wishY / wishLen : 0.0f;

        rs.dashCooldownSeconds = std::max(0.0f, rs.dashCooldownSeconds - dt);
        const bool freezeNow = mi.freeze != 0;
        const bool freezeEdge = freezeNow && !rs.freezePreviously;
        const bool dashEdge = mi.dash != 0 && !rs.dashHeldPreviously;
        const bool downDashEdge = mi.downDash != 0 && !rs.downDashHeldPreviously;
        const bool jumpEdge = mi.jump != 0 && !rs.jumpHeldPreviously;

        // Edge-only input trace for the two touch-reset abilities. The logger
        // adds UTC wall_time and monotonic t; frame is the fixed simulation
        // frame visible at the hot boundary (ctx->tick).
        const bool dashReleased = mi.dash == 0 && rs.dashHeldPreviously != 0;
        const bool downDashReleased =
            mi.downDash == 0 && rs.downDashHeldPreviously != 0;
        if (dashEdge || dashReleased || downDashEdge || downDashReleased) {
            const char* action = downDashEdge      ? "q_down"
                                 : downDashReleased ? "q_up"
                                 : dashEdge          ? "shift_down"
                                                      : "shift_up";
            char inputMessage[256];
            std::snprintf(
                inputMessage, sizeof(inputMessage),
                "action=%s q=%u shift=%u q_edge_down=%u q_edge_up=%u "
                "shift_edge_down=%u shift_edge_up=%u frame=%llu tick=%llu "
                "dash_available=%u down_dash_available=%u",
                action, mi.downDash ? 1u : 0u, mi.dash ? 1u : 0u,
                downDashEdge ? 1u : 0u, downDashReleased ? 1u : 0u,
                dashEdge ? 1u : 0u, dashReleased ? 1u : 0u,
                static_cast<unsigned long long>(ctx->tick),
                static_cast<unsigned long long>(tick),
                rs.dashAvailable, rs.downDashAvailable);
            HotCollisionPackage::collisionLogFull(
                ctx, 2u, "MOVEMENT", "movement.input_edge", inputMessage,
                action, e, e, HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER,
                ctx->tick, 0, tick, tick);
        }

        bool didDash = false;
        bool didDownDash = false;
        const bool dashAvailableBefore = rs.dashAvailable != 0u;
        const bool downDashAvailableBefore = rs.downDashAvailable != 0u;
        float dashDirX = 0.0f;
        float dashDirY = 0.0f;

        // ── afad20a PRE-collision: gravity -> freeze -> down-dash ─────────
        // GRAVITY (pre): the ONE shared hot gravity policy. physics.move no
        // longer applies it. Grounded actors rest on the ground; applying
        // gravity every tick would make the solver micro-bounce forever.
        if (!freezeNow && !rs.grounded) {
            GameGravityV1 gv{};
            gv.velocityZ = vz;
            gv.gravityZ = -static_cast<float>(m.gravityMagnitude);
            gv.maximumFallSpeed = m.maxFallSpeed;
            gv.dt = dt;
            float outZ = vz;
            MimitaHotMovement::gravity(gv, outZ);
            vz = outZ;
        }

        // FREEZE (pre): the ONE shared hot freeze policy.
        GameFreezePolicyV1 fp{};
        fp.velocity[0] = vx;
        fp.velocity[1] = vy;
        fp.velocity[2] = vz;
        fp.dt = dt;
        fp.durationSeconds = m.freezeDurationSeconds;
        fp.freezePressed = freezeEdge ? 1u : 0u;
        fp.freezeHeld = freezeNow ? 1u : 0u;
        fp.freezeHeldPreviously = rs.freezePreviously ? 1u : 0u;
        fp.freezeEnabled = m.freezeEnabled;
        fp.freezeActive = rs.freezeActive;
        fp.freezeAvailable = rs.freezeAvailable;
        fp.freezeTimerSeconds = rs.freezeTimerSeconds;
        fp.movementModel =
            (m.walkMode == MimitaHotMovement::kWalkModeV206) ? 1u : 0u;
        MimitaHotMovement::freezePolicy(fp);
        rs.freezeActive = fp.outFreezeActive;
        rs.freezeAvailable = fp.outFreezeAvailable;
        rs.freezeTimerSeconds = fp.outFreezeTimerSeconds;
        vx = fp.outVelocity[0];
        vy = fp.outVelocity[1];
        vz = fp.outVelocity[2];

        // DOWN-DASH (pre): same shared dash policy, dash edge suppressed.
        bool didGroundJump = false;
        bool didAirJump = false;
        {
            const float yawRad = yaw * 0.01745329252f;
            GameDashPolicyV1 dd{};
            dd.velocity[0] = vx;
            dd.velocity[1] = vy;
            dd.velocity[2] = vz;
            dd.moveAxes[0] = hasWish ? wishDirX : 0.0f;
            dd.moveAxes[1] = hasWish ? wishDirY : 0.0f;
            dd.cameraForward[0] = std::cos(yawRad);
            dd.cameraForward[1] = std::sin(yawRad);
            dd.groundDashImpulse = m.groundDashImpulse;
            dd.airDashImpulse = m.airDashImpulse;
            dd.downDashVerticalSpeed = m.downDashSpeed;
            dd.dashPressed = 0u;
            dd.downDashPressed = (downDashEdge && rs.downDashAvailable) ? 1u : 0u;
            dd.grounded = rs.grounded ? 1u : 0u;
            dd.dashAvailable = rs.dashAvailable ? 1u : 0u;
            dd.downDashAvailable = rs.downDashAvailable ? 1u : 0u;
            dd.dashEnabled = m.dashEnabled;
            dd.downDashEnabled = m.downDashEnabled;
            MimitaHotMovement::dashPolicy(dd);
            vx = dd.outVelocity[0];
            vy = dd.outVelocity[1];
            vz = dd.outVelocity[2];
            if (dd.outDidDownDash) {
                rs.downDashAvailable = dd.outDownDashAvailable;
                didDownDash = true;
            }
        }

        // afad20a freeze suppression: the collision/integration velocity is
        // scaled by the pass-through curve, while the stored velocity is
        // reconciled back after the solve so momentum is preserved and returns
        // when the freeze ends (mirrors physics-mini's collision velocity view).
        const bool frozenNow =
            fp.outFreezeActive != 0u &&
            m.walkMode != MimitaHotMovement::kWalkModeV206;
        const float freezePass = frozenNow
            ? MimitaHotMovement::freezePassThrough(rs.freezeTimerSeconds,
                                                   m.freezeDurationSeconds,
                                                   m.freezeCurveExponent)
            : 1.0f;
        const float storedVx = vx;
        const float storedVy = vy;
        const float storedVz = vz;
        st.velocity[0] = vx * freezePass;
        st.velocity[1] = vy * freezePass;
        st.velocity[2] = vz * freezePass;
        // Gravity is owned by the hot gravity policy above; tell the generic
        // capsule solver not to add its own (negative = already integrated).
        st.gravityScale = -1.0f;
        st.grounded = rs.grounded;
        resolveCollisions(ctx, &st, dt, e, tick);
        rs.grounded = st.grounded;
        if (frozenNow) {
            if (freezePass > MimitaHotMovement::kFreezeReconcileMinPassThrough) {
                vx = st.velocity[0] / freezePass;
                vy = st.velocity[1] / freezePass;
                vz = st.velocity[2] / freezePass;
            } else {
                vx = storedVx;
                vy = storedVy;
                vz = storedVz;
            }
        }

        // ── afad20a POST-collision: reset, then walk -> dash -> jump ──────
        // The contact reset must run before the ability edges are consumed so a
        // grounded player can dash/down-dash again on the same fresh press.
        // afad20a synthesizes a Ground contact whenever the actor is on the
        // ground even if no explicit contact was returned, so grounded alone
        // qualifies; a limb/weapon contact qualifies without being grounded.
        const bool contactNow = (st.collided != 0) || (st.grounded != 0);
        if (contactNow)
            MimitaHotMovement::restoreTouchAbilities(rs);
        if (contactNow)
            rs.reserved[GAME_MOVEMENT_STAMP_FLAGS] |= 2u;
        else
            rs.reserved[GAME_MOVEMENT_STAMP_FLAGS] &= ~2u;

        if (!frozenNow) {
            vx = st.velocity[0];
            vy = st.velocity[1];
            vz = st.velocity[2];
        }

        if (fp.outFreezeActive == 0u) {
            // SPEED: one shared hot speed policy derives the effective max speed
            // (size-scale aware), the same implementation the server uses.
            GameSpeedPolicyV1 sp{};
            sp.baseMaxSpeed = m.walkSpeed;
            sp.baseFallbackSpeed = m.walkSpeed;
            sp.sizeScale = body.sizeScale;
            sp.sizeExponent = 0.0f;
            sp.speedLimit = m.speedLimitEnabled ? m.speedLimit : 0.0f;
            sp.speedLimitFixed = (m.speedLimitMode == 1u) ? 1u : 0u;
            sp.airMaxWishspeed = m.airMaxWishspeed;
            sp.rawWishSpeed = m.walkSpeed;
            float optMax = m.walkSpeed;
            float optWish = m.walkSpeed;
            MimitaHotMovement::speedPolicy(sp, optMax, optWish);
            const float speed = optMax;
            if (rs.grounded && !mi.jump) {
                // GROUND: the ONE shared hot ground-move policy.
                const bool v206Walk =
                    m.walkMode == MimitaHotMovement::kWalkModeV206;
                GameGroundMoveV1 g{};
                g.velocity[0] = vx;
                g.velocity[1] = vy;
                g.wishDir[0] = wishDirX;
                g.wishDir[1] = wishDirY;
                g.wishSpeed = speed;
                g.groundAcceleration = m.groundAcceleration;
                g.frictionAmount = (m.walkMode == MimitaHotMovement::kWalkModeSource)
                                       ? m.groundFriction
                                       : m.groundFrictionAmount;
                g.stopspeed = v206Walk ? 0.0f : m.stopspeed;
                g.dt = dt;
                g.hasInput = hasWish ? 1u : 0u;
                g.movementModel = v206Walk ? 1u : 0u;
                float out[2] = {vx, vy};
                MimitaHotMovement::groundMove(g, out);
                vx = out[0];
                vy = out[1];
                // afad20a landing vertical snap (applySourceGround): a small
                // vertical velocity while grounded is zeroed, so the collision
                // bounce cannot make a resting actor oscillate. Real impacts
                // (down-dash, hard fall) exceed velocityClipEpsilon and bounce.
                if (m.groundSnap && std::fabs(vz) <= m.velocityClipEpsilon)
                    vz = 0.0f;
            } else if (hasWish) {
                // AIR: the ONE shared hot air-acceleration policy.
                GameAirAccelerateV1 air{};
                air.velocity[0] = vx;
                air.velocity[1] = vy;
                air.wishDir[0] = wishDirX;
                air.wishDir[1] = wishDirY;
                air.wishSpeed = m.sourceAirAccelerateBugCompatible ? speed : optWish;
                air.wishspd = optWish;
                air.maxSpeed = speed;
                air.airAcceleration = m.airAcceleration;
                air.surfaceFriction = m.surfaceFriction;
                air.airSpeedGainMultiplier = m.airSpeedGainMultiplier;
                air.dt = dt;
                air.currentSpeed = vx * wishDirX + vy * wishDirY;
                air.blendedAddSpeed = air.wishspd - air.currentSpeed;
                air.movementModel =
                    m.sourceAirAccelerateBugCompatible ? 1u : 0u;
                float out[2] = {vx, vy};
                MimitaHotMovement::airAccelerate(air, out);
                vx = out[0];
                vy = out[1];
            }

            // DASH (post-collision, additive, edge-triggered). No cooldown
            // timer: a fresh press plus availability is the only gate.
            {
                const float inVx = vx;
                const float inVy = vy;
                const float yawRad = yaw * 0.01745329252f;
                GameDashPolicyV1 dp{};
                dp.velocity[0] = vx;
                dp.velocity[1] = vy;
                dp.velocity[2] = vz;
                dp.moveAxes[0] = hasWish ? wishDirX : 0.0f;
                dp.moveAxes[1] = hasWish ? wishDirY : 0.0f;
                dp.cameraForward[0] = std::cos(yawRad);
                dp.cameraForward[1] = std::sin(yawRad);
                dp.groundDashImpulse = m.groundDashImpulse;
                dp.airDashImpulse = m.airDashImpulse;
                dp.downDashVerticalSpeed = m.downDashSpeed;
                dp.dashPressed = (dashEdge && rs.dashAvailable) ? 1u : 0u;
                dp.downDashPressed = 0u;
                dp.grounded = rs.grounded ? 1u : 0u;
                dp.dashAvailable = rs.dashAvailable ? 1u : 0u;
                dp.downDashAvailable = rs.downDashAvailable ? 1u : 0u;
                dp.dashEnabled = m.dashEnabled;
                dp.downDashEnabled = m.downDashEnabled;
                dp.dashMovementTicks = rs.dashMovementTicks;
                MimitaHotMovement::dashPolicy(dp);
                vx = dp.outVelocity[0];
                vy = dp.outVelocity[1];
                vz = dp.outVelocity[2];
                if (dp.outDidDash) {
                    rs.dashAvailable = dp.outDashAvailable;
                    rs.dashCooldownSeconds = 0.0f;
                    didDash = true;
                    const float ddx = dp.outVelocity[0] - inVx;
                    const float ddy = dp.outVelocity[1] - inVy;
                    const float dl = std::sqrt(ddx * ddx + ddy * ddy);
                    dashDirX = dl > 1e-4f ? ddx / dl : 0.0f;
                    dashDirY = dl > 1e-4f ? ddy / dl : 0.0f;
                }
            }
        }

        // JUMP (post): the ONE shared hot jump policy. Freeze does not suppress
        // jump velocity.
        {
            GameJumpPolicyV1 jp{};
            jp.velocityZ = vz;
            jp.jumpSpeed = m.jumpSpeed;
            jp.dt = dt;
            jp.coyoteSeconds = m.coyoteSeconds;
            jp.jumpBufferSeconds = m.jumpBufferSeconds;
            // Touch anything (ground/wall/ceiling/prop) and the jump is eligible.
            const bool contactLastTick =
                (rs.reserved[GAME_MOVEMENT_STAMP_FLAGS] & 2u) != 0u;
            jp.grounded = (rs.grounded || contactLastTick) ? 1u : 0u;
            jp.jumpPressed = jumpEdge ? 1u : 0u;
            jp.jumpHeld = mi.jump ? 1u : 0u;
            jp.jumpHeldPreviously = rs.jumpHeldPreviously ? 1u : 0u;
            jp.autoBhopEnabled = m.autoBhopEnabled;
            jp.maximumAirJumps = m.maximumAirJumps;
            jp.jumpIntentTimerSeconds = 0.0f;
            jp.coyoteTimerSeconds = 0.0f;
            jp.airJumpsLeft = static_cast<std::int32_t>(rs.airJumpsLeft);
            jp.airJumpArmed = rs.jumpAirJumpArmed ? 1u : 0u;
            jp.airJumpLocked = 0u;
            MimitaHotMovement::jumpPolicy(jp);
            vz = jp.outVelocityZ;
            rs.grounded = jp.outGrounded;
            rs.airJumpsLeft = static_cast<std::uint32_t>(jp.airJumpsLeft);
            rs.jumpAirJumpArmed = jp.airJumpArmed;
            didGroundJump = jp.outDidGroundJump != 0u;
            didAirJump = jp.outDidAirJump != 0u;
        }

        if (vz < -m.maxFallSpeed)
            vz = -m.maxFallSpeed;

        st.velocity[0] = vx;
        st.velocity[1] = vy;
        st.velocity[2] = vz;

        // Apply the configured horizontal speed limit after all additive
        // abilities and collision response.
        if (m.speedLimitEnabled && m.speedLimit > 0.0f) {
            GameSpeedClampV1 clamp{};
            clamp.velocity[0] = st.velocity[0];
            clamp.velocity[1] = st.velocity[1];
            clamp.speedLimit = m.speedLimit;
            clamp.enabled = 1u;
            MimitaHotMovement::speedClamp(clamp);
            st.velocity[0] = clamp.outVelocity[0];
            st.velocity[1] = clamp.outVelocity[1];
        }

        // Airborne ticks with movement held (diagnostic / actor parity).
        if (!st.grounded && mi.pressed) {
            if (rs.dashMovementTicks < 99u)
                ++rs.dashMovementTicks;
        } else {
            rs.dashMovementTicks = 0u;
        }

        // Prove the ability-reset decision directly in events.jsonl. The
        // collision package separately records worldContact; tick/entity IDs
        // join both records without adding a second collision owner. Log only a
        // real availability transition or a fired down-dash, not every grounded
        // tick (grounded resets every tick now).
        if (didDownDash ||
            downDashAvailableBefore != (rs.downDashAvailable != 0u)) {
            char contactMessage[320];
            std::snprintf(
                contactMessage, sizeof(contactMessage),
                "touch=%u grounded=%u collided=%u ability_dash_before=%u "
                "ability_dash_after=%u ability_down_dash_before=%u "
                "ability_down_dash_after=%u restored=%u down_dash_fired=%u "
                "pos=(%.3f %.3f %.3f) velocity=(%.3f %.3f %.3f)",
                contactNow ? 1u : 0u, st.grounded, st.collided,
                dashAvailableBefore ? 1u : 0u,
                rs.dashAvailable ? 1u : 0u,
                downDashAvailableBefore ? 1u : 0u,
                rs.downDashAvailable ? 1u : 0u,
                (!downDashAvailableBefore && rs.downDashAvailable) ? 1u : 0u,
                didDownDash ? 1u : 0u, st.position[0], st.position[1],
                st.position[2], st.velocity[0], st.velocity[1], st.velocity[2]);
            HotCollisionPackage::collisionLogFull(
                ctx, 2u, "MOVEMENT", "movement.contact_ability", contactMessage,
                contactNow ? "contact" : "no_contact", e, e,
                HotCollisionPackage::COLLISION_LOG_ACTOR_PLAYER, tick, 0, tick,
                tick);
        }

        if (didDash) {
            const float dir[3] = {dashDirX, dashDirY, 0.0f};
            spawnEffect(ctx, gameHash("effect.dash"), st.position, dir, st.sizeScale, 0.0f);
            const std::uint64_t actionId =
                (static_cast<std::uint64_t>(tick) << 32u) ^ e ^ gameHash("dash");
            playActionSound(ctx, e, st.position, "entity/player/dash", 1.0f, 1.0f);
            logAction(ctx, e, tick, "dash", actionId,
                      rs.dashAvailable != 0u, st.position, st.velocity);
        }
        if (didDownDash) {
            spawnEffect(ctx, gameHash("effect.downDash"), st.position, nullptr, st.sizeScale, 0.0f);
            const std::uint64_t actionId =
                (static_cast<std::uint64_t>(tick) << 32u) ^ e ^ gameHash("down_dash");
            playActionSound(ctx, e, st.position, "entity/player/dash", 1.0f, 0.82f);
            logAction(ctx, e, tick, "down_dash", actionId,
                      rs.downDashAvailable != 0u, st.position, st.velocity);
        }
        if (didGroundJump || didAirJump) {
            const std::uint64_t effectId = didAirJump
                ? gameHash("effect.airJump") : gameHash("effect.groundJump");
            spawnEffect(ctx, effectId, st.position, nullptr, st.sizeScale, 0.0f);
            playActionSound(ctx, e, st.position,
                            didAirJump ? "entity/player/doublejump"
                                       : "entity/player/jump",
                            1.0f, 1.0f);
        }
        if (freezeEdge)
            spawnEffect(ctx, gameHash("effect.freeze"), st.position, nullptr, st.sizeScale, 0.0f);
        else if (freezeNow)
            spawnEffect(ctx, gameHash("effect.freezeTrail"), st.position, nullptr, st.sizeScale, 0.0f);

        // Publish generic "ability actually fired" facts for the animation system.
        // OR-accumulated until the hot animation policy reads and clears them.
        const std::uint32_t fired =
            (didDash ? HOT_FIRED_DASH : 0u) |
            (didDownDash ? HOT_FIRED_DOWN_DASH : 0u) |
            (freezeEdge ? HOT_FIRED_FREEZE : 0u) |
            (jumpEdge && rs.grounded ? HOT_FIRED_GROUND_JUMP : 0u) |
            (jumpEdge && !rs.grounded ? HOT_FIRED_AIR_JUMP : 0u);
        if (fired != 0u && ctx->dynamicReadComponent &&
            ctx->dynamicWriteComponent) {
            HotMovementFiredV1 acc{};
            ctx->dynamicReadComponent(ctx->host, e, HOT_MOVEMENT_FIRED_COMPONENT,
                                      &acc, sizeof(acc));
            acc.version = HOT_MOVEMENT_FIRED_VERSION;
            acc.flags |= fired;
            ctx->dynamicWriteComponent(ctx->host, e, HOT_MOVEMENT_FIRED_COMPONENT,
                                       &acc, sizeof(acc));
        }
    }

    rs.jumpHeldPreviously = mi.jump;
    rs.dashHeldPreviously = mi.dash;
    rs.downDashHeldPreviously = mi.downDash;
    rs.freezePreviously = mi.freeze;
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_MOVEMENT_RUNTIME_STATE,
                        &rs, sizeof(rs));

    // Publish: kernel applies this transform and skips the built-in step.
    ctx->requestMovementOverride(ctx->host, 1u, st.position, st.velocity, yaw);

    // Keep components coherent for observers and other hot systems.
    GameTransformComponentV1 outTf = tf;
    outTf.position[0] = st.position[0];
    outTf.position[1] = st.position[1];
    outTf.position[2] = st.position[2];
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_TRANSFORM, &outTf, sizeof(outTf));
    GameVelocityComponentV1 outVl = vl;
    outVl.linear[0] = st.velocity[0];
    outVl.linear[1] = st.velocity[1];
    outVl.linear[2] = st.velocity[2];
    ctx->writeComponent(ctx->host, e, GAME_COMPONENT_VELOCITY, &outVl, sizeof(outVl));
}

// ── command: movementmode [name] ────────────────────────────────────
// Lists/reports the hot C++ presets. The global active preset is a C++ constant
// (kActiveMovementPreset); per-actor presets come from ActorProfileState.
void MIMITA_GAME_CALL movementModeCommand(void* /*host*/, const char* args)
{
    if (args && args[0] != '\0') {
        if (!MimitaHotMovement::movementPresetNameExists(args)) {
            std::printf("[MOVEMENT MODE] unknown '%s'\n", args);
            return;
        }
        std::printf("[MOVEMENT MODE] '%s' is valid; global active is '%s' "
                    "(edit kActiveMovementPreset + rebuild DLL to change)\n",
                    args, MimitaHotMovement::getActiveMovementPreset().name);
        return;
    }
    std::printf("[MOVEMENT MODE] active='%s' presets:",
                MimitaHotMovement::getActiveMovementPreset().name);
    for (std::uint32_t i = 0; i < MimitaHotMovement::kMovementPresetCount; ++i)
        std::printf(" %s", MimitaHotMovement::kMovementPresets[i].name);
    std::printf("\n");
}

const MimitaHotPackage::SystemRegistrar s_movementMain{
    {gameHash("movement.main"), GAME_DOMAIN_GAMEPLAY, 0, 0,
     movementMainTick, "movement.main"}};

const MimitaHotPackage::CommandRegistrar s_movementModeCmd{
    {"movementmode", "movementmode [name] - list/report hot C++ movement presets", 0,
     movementModeCommand}};

const MimitaHotPackage::EventRegistrar s_movementTuningRegistration{
    {GAME_EVENT_MOVEMENT_TUNING, 0, 0, onMovementTuning, "movement.tuning"}};

} // namespace

#endif
