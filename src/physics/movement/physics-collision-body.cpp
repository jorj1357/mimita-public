#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "physics/config.h"
#include "entities/player.h"
#include "world/world.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "combat/weapon-collision-config.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "ecs/actor-entities.h"
#include "ecs/dynamic-components.h"
#include "hot-reload/hot-presentation.h"

#define BWLOG(...) Debug::logThrottled(Debug::Category::Collision, "bw-investigate", 1.0f, __VA_ARGS__)

// Default skin/margin fallback (used when per-weapon skin is unavailable).
constexpr float DEFAULT_WEAPON_COLLISION_SKIN = 0.04f;

BWInvestigate gBW;

// Hot-owned tools already publish the transform used for rendering through
// AttachmentState. Collision must consume that same resolved transform or the
// visible tool and its collider will diverge whenever the hot body pose moves.
static bool useResolvedHotToolTransform(Player& p,
                                        HotAttachmentStateV1* resolvedOut)
{
    if (resolvedOut)
        *resolvedOut = HotAttachmentStateV1{};
    if (p.equippedWeaponId.empty())
        return false;

    const EntityId actor = Ecs::ensureLocalPlayerEntity();
    if (actor == kInvalidEntityId)
        return false;

    auto& store = MimitaRuntime::DynamicComponentStore::instance();
    HotToolClaimV1 claim{};
    if (!store.read(actor, HOT_TOOL_CLAIM_COMPONENT, &claim, sizeof(claim)) ||
        claim.migrated == 0 || claim.toolEntity == 0)
        return false;

    const std::uint64_t weaponKey = gameHash(p.equippedWeaponId.c_str());
    if (claim.toolKey != weaponKey &&
        (p.runtimeToolId == 0 || claim.toolKey != p.runtimeToolId))
        return false;

    HotAttachmentStateV1 attachment{};
    if (!store.read(static_cast<EntityId>(claim.toolEntity),
                    HOT_ATTACHMENT_COMPONENT, &attachment,
                    sizeof(attachment)) || attachment.resolved == 0)
        return false;
    if (resolvedOut)
        *resolvedOut = attachment;

    glm::quat rotation(
        attachment.worldRotation[3], attachment.worldRotation[0],
        attachment.worldRotation[1], attachment.worldRotation[2]);
    const float qLen = glm::length(rotation);
    if (!std::isfinite(qLen) || qLen < 0.0001f)
        return false;
    rotation = glm::normalize(rotation);

    glm::vec3 scale(
        attachment.worldScale[0], attachment.worldScale[1],
        attachment.worldScale[2]);
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(scale[i]) || scale[i] <= 0.0001f)
            scale[i] = 1.0f;

    glm::vec3 position(
        attachment.worldPosition[0], attachment.worldPosition[1],
        attachment.worldPosition[2]);
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z))
        return false;

    p.weaponCollisionWorld = glm::translate(glm::mat4(1.0f), position) *
                             glm::mat4_cast(rotation) *
                             glm::scale(glm::mat4(1.0f), scale);
    p.weaponModelTransform = p.weaponCollisionWorld;
    return true;
}

static void logWeaponTransformProbe(const Player& p, bool hotTransform,
                                    const HotAttachmentStateV1& attachment,
                                    const Capsule& capsule)
{
    static std::uint64_t lastTick = ~std::uint64_t{0};
    if (p.movementSimulationTick == lastTick || p.equippedWeaponId.empty())
        return;
    lastTick = p.movementSimulationTick;

    const glm::vec3 hotMuzzle(
        attachment.muzzleWorldPosition[0], attachment.muzzleWorldPosition[1],
        attachment.muzzleWorldPosition[2]);
    const glm::vec3 collisionTip = capsule.b;
    const glm::vec3 muzzleDelta = collisionTip - hotMuzzle;

    debug::Event ev;
    ev.category = "WEAPONS";
    ev.name = "weapon.transform_probe";
    ev.level = debug::Level::Info;
    ev.simulationTick = p.movementSimulationTick;
    ev.message = hotTransform ? "hot_attachment_and_collision_transform"
                              : "cold_collision_transform_fallback";
    ev.reason = hotTransform ? "hot_transform_used" : "cold_fallback";
    ev.fields["weapon_id"] = p.equippedWeaponId;
    ev.fields["hot_transform_used"] = hotTransform;
    ev.fields["attachment_resolved"] = attachment.resolved != 0;
    ev.fields["hot_world_position"] = {attachment.worldPosition[0],
                                         attachment.worldPosition[1],
                                         attachment.worldPosition[2]};
    ev.fields["hot_world_rotation"] = {attachment.worldRotation[0],
                                         attachment.worldRotation[1],
                                         attachment.worldRotation[2],
                                         attachment.worldRotation[3]};
    ev.fields["hot_muzzle_world"] = {hotMuzzle.x, hotMuzzle.y, hotMuzzle.z};
    ev.fields["hot_forward"] = {attachment.forward[0], attachment.forward[1],
                                 attachment.forward[2]};
    ev.fields["collision_grip_world"] = {capsule.a.x, capsule.a.y, capsule.a.z};
    ev.fields["collision_tip_world"] = {collisionTip.x, collisionTip.y,
                                         collisionTip.z};
    ev.fields["collision_radius"] = capsule.r;
    ev.fields["tip_minus_hot_muzzle"] = {muzzleDelta.x, muzzleDelta.y,
                                          muzzleDelta.z};
    ev.aggregationKey = "weapon.transform_probe." + p.equippedWeaponId;
    MIMITA_EVENT(ev);
}

static bool computeBodyPartCenter(
    const glm::mat4& xform,
    const Collider& collider,
    glm::vec3& outCenter,
    float& outRadius
) {
    glm::vec3 localCenter = (collider.localMin + collider.localMax) * 0.5f;
    glm::vec3 localExtents = (collider.localMax - collider.localMin) * 0.5f;

    outCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
    outRadius = std::max({localExtents.x, localExtents.y, localExtents.z, BODY_SAMPLE_RADIUS});
    outRadius = std::min(outRadius, 0.35f);
    return true;
}

void recomputeWeaponCapsule(Player& p)
{
    // The weaponCollisionWorld transform is needed by applyCollisionConfig to
    // convert JSON local-space offsets to world-space collision spheres.
    // Compute it from the right arm attachment transform.
    bool foundArm = false;
    for (const PhysicalBodyPart& part : p.physicalBody.parts)
    {
        if (part.name != "rightArm")
            continue;
        foundArm = true;
        p.weaponCollisionWorld = part.worldTransform * p.weaponLocalToArm;
        break;
    }

    if (!foundArm) {
        p.weaponCollisionWorld = glm::mat4(1.0f);
        p.weaponCollisionDebug.fromJsonConfig = false;
        p.weaponCollisionDebug.capsuleMode = false;
        p.weaponCollisionDebug.valid = false;
        p.weaponCollisionDebug.spheres.clear();
        p.weaponCollisionDebug.capsule.enabled = false;
        p.weaponCollisionDebug.capsules.clear();
        p.collision.hasWeaponCollisionCapsule = false;
        p.weaponModelTransform = glm::mat4(1.0f);
        return;
    }

    // Hot-owned tools publish the same resolved transform that the renderer
    // uses. If unavailable, preserve the cold arm/local-transform fallback.
    HotAttachmentStateV1 hotAttachment{};
    const bool hotTransform = useResolvedHotToolTransform(p, &hotAttachment);

    // Apply JSON config — this is the ONLY source of weapon collision data.
    WeaponCollisionJsonConfig::instance().applyCollisionConfig(p);

    // Capsule mode (default): rebuild the world-space weapon collision capsule
    // from the local capsule shape (model-derived or config-overridden) via the
    // weapon's world transform, so the collision matches the rendered weapon.
    if (p.weaponCollisionDebug.capsuleMode) {
        Capsule cap;
        cap.a = glm::vec3(p.weaponCollisionWorld * glm::vec4(p.weaponGripLocal, 1.0f));
        cap.b = glm::vec3(p.weaponCollisionWorld * glm::vec4(p.weaponMuzzleLocal, 1.0f));
        cap.r = p.weaponRadiusLocal;
        if (glm::length(cap.b - cap.a) > 0.001f && cap.r > 0.001f) {
            p.weaponCollisionCapsule = cap;
            p.collision.hasWeaponCollisionCapsule = true;
            p.weaponCollisionDebug.valid = true;
        }
        logWeaponTransformProbe(p, hotTransform, hotAttachment,
                                p.weaponCollisionCapsule);
        return;
    }

    if (!p.weaponCollisionDebug.fromJsonConfig) {
        // No JSON config for this weapon: no weapon collision.
        // This is intentional — weapon collision is fully data-driven.
        p.weaponCollisionDebug.valid = false;
        p.weaponCollisionDebug.spheres.clear();
        p.weaponCollisionDebug.capsule.enabled = false;
    }

    // JSON-sphere/capsule mode has no early capsule return, so emit the same
    // transform probe for that path as well.
    logWeaponTransformProbe(p, hotTransform, hotAttachment,
                            p.weaponCollisionCapsule);
}

// Generate sphere samples for configurable weapon colliders from JSON collision config.
// Uses fixed 5 samples per collider along the dominant axis.
// Collect sphere samples from all body parts + weapon for contact testing.
// Simple capsule-only: 5 samples along weapon grip→tip axis + body part spheres.
std::vector<BodyWeaponSphere> collectBodyWeaponSpheres(Player& p)
{
    auto t0 = std::chrono::steady_clock::now();
    std::vector<BodyWeaponSphere> spheres;
    spheres.reserve(p.physicalBody.parts.size() + 64);

    glm::vec3 rootMove = p.vel * 0.0f;

    // 1. Body part spheres (one per part — no redundancy)
    {
        auto tb0 = std::chrono::steady_clock::now();
        int partCount = 0;
        for (const PhysicalBodyPart& part : p.physicalBody.parts)
        {
            glm::vec3 center;
            float radius;
            if (!computeBodyPartCenter(part.worldTransform, part.collider, center, radius))
                continue;
            ++partCount;

            glm::vec3 prevCenter;
            computeBodyPartCenter(part.previousWorldTransform, part.collider, prevCenter, radius);
            glm::vec3 sweepDelta = center - prevCenter;

            spheres.push_back({center, radius, part.name.c_str(), sweepDelta});
        }
        auto tb1 = std::chrono::steady_clock::now();
        gBW.bodyPartSphereCount = partCount;
        gBW.bodyPartSpheresMs = std::chrono::duration<float, std::milli>(tb1 - tb0).count();
    }

    // 2. Weapon collision spheres — JSON only, no fallback.
    {
        auto tw0 = std::chrono::steady_clock::now();
        if (p.weaponCollisionDebug.fromJsonConfig && p.weaponCollisionDebug.valid)
        {
            const auto& wcd = p.weaponCollisionDebug;
            gBW.weaponCapsuleSphereCount = (int)wcd.spheres.size();
            for (const auto& ds : wcd.spheres)
            {
                if (!ds.collidesWithWorld) continue;
                spheres.push_back({ds.currentCenter, ds.radius, ds.name.c_str(), ds.sweepDelta});
            }
        }
        auto tw1 = std::chrono::steady_clock::now();
        gBW.weaponCapsuleSpheresMs = std::chrono::duration<float, std::milli>(tw1 - tw0).count();
    }

    gBW.sphereCount = (int)spheres.size();
    auto t1 = std::chrono::steady_clock::now();
    gBW.collectSpheresMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    return spheres;
}

// Collect world-triangle contacts from all body/weapon spheres.
// Returns RecoveryContacts compatible with the batched solver.
// Uses a single union broadphase gather for all spheres instead of per-sphere
// uncached gathers. This eliminates ~250 heap allocations and ~250 spatial hash
// queries per frame (6 substeps × 3 passes × ~15 spheres).
std::vector<RecoveryContact> collectBodyWeaponContacts(
    const Player& p,
    const World& world,
    const std::vector<BodyWeaponSphere>& spheres
) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<RecoveryContact> contacts;
    constexpr float SLIDE_SLOP = 0.002f;

    // Use per-weapon collision skin from config, fall back to default
    float skin = p.weaponCollisionDebug.valid
        ? p.weaponCollisionDebug.collisionSkin
        : DEFAULT_WEAPON_COLLISION_SKIN;

    gBW.candidateCount = 0;
    int totalTests = 0;
    int sweepHits = 0;

    if (spheres.empty()) return contacts;

    // Build a union AABB encompassing all body/weapon spheres. This is the key
    // optimization: instead of doing N separate uncached broadphase queries (one
    // per sphere), we do ONE query for the union region and then filter per sphere.
    // The union AABB is tight — it only covers the actual sphere positions + radii.
    AABB unionBounds;
    {
        const auto& first = spheres[0];
        float r0 = first.radius + skin;
        unionBounds.min = first.center - glm::vec3(r0);
        unionBounds.max = first.center + glm::vec3(r0);
        for (size_t i = 1; i < spheres.size(); ++i) {
            const auto& s = spheres[i];
            float r = s.radius + skin;
            unionBounds.min = glm::min(unionBounds.min, s.center - glm::vec3(r));
            unionBounds.max = glm::max(unionBounds.max, s.center + glm::vec3(r));
        }
    }

    // Single broadphase query for the union region
    std::vector<int> unionCandidates;
    appendChunkTrianglesForAABB(world, unionBounds, 0.0f, unionCandidates, "bwUnionGather");
    gBW.candidateCount = (int)unionCandidates.size();

    // Per-sphere contact testing against the union candidates
    for (const BodyWeaponSphere& bs : spheres)
    {
        for (int triIdx : unionCandidates)
        {
            ++totalTests;
            if (triIdx < 0 || triIdx >= (int)world.collisionMesh.triangles.size())
                continue;

            const CollisionTriangle& tri = world.collisionMesh.triangles[triIdx];

            // Sweep test: skip for static spheres (no movement delta)
            // to avoid paying for sweepSphereTriangle when it always early-outs.
            if (glm::dot(bs.sweepDelta, bs.sweepDelta) > 0.000001f)
            {
                float hitTime = 1.0f;
                glm::vec3 hitNormal, hitPoint;
                auto tsw0 = std::chrono::steady_clock::now();
                bool sweepHit = sweepSphereTriangle(bs.center, bs.sweepDelta, bs.radius + skin, tri,
                                                     hitTime, hitNormal, hitPoint);
                auto tsw1 = std::chrono::steady_clock::now();
                gBW.sweepSphereTriangleMs += std::chrono::duration<float, std::milli>(tsw1 - tsw0).count();
                gBW.sweepTests++;

                if (sweepHit && hitTime < 1.0f)
                {
                    float depth = (bs.radius + skin) - glm::dot(bs.center - hitPoint, hitNormal);
                    depth = std::max(0.0f, depth - skin);
                    if (depth > SLIDE_SLOP) {
                        contacts.push_back({hitNormal, hitPoint, bs.sweepDelta, depth, triIdx, nullptr, bs.label});
                        ++sweepHits;
                        continue;
                    }
                }
            }

            auto tst0 = std::chrono::steady_clock::now();
            Contact c;
            bool staticHit = sphereTriangleContact(bs.center, bs.radius + skin, tri, c);
            auto tst1 = std::chrono::steady_clock::now();
            gBW.sphereTriangleContactMs += std::chrono::duration<float, std::milli>(tst1 - tst0).count();
            gBW.staticTests++;

            if (staticHit && c.penetration > (SLIDE_SLOP + skin))
            {
                float skinPen = std::max(0.0f, c.penetration - skin);
                contacts.push_back({c.normal, c.point, bs.sweepDelta, skinPen, triIdx, nullptr, bs.label});
            }
        }
    }

    gBW.triangleTests = totalTests;
    gBW.contactsProduced = (int)contacts.size();

    auto t1 = std::chrono::steady_clock::now();
    gBW.collectContactsMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    return contacts;
}

std::vector<glm::vec3> collectPlayerBodyCollisionSamples(Player& p)
{
    std::vector<glm::vec3> samples;

    // Defensive limits: a valid player body is only a few units across. If a body
    // part's world transform is garbage (scattered over hundreds of units — see the
    // node-order bug in updateModelWorldTransforms), reject it so broadphase queries
    // never build a map-sized AABB.
    constexpr float MAX_BODY_EXTENT = 8.0f;
    constexpr float MAX_COLLIDER_AXIS = 4.0f;

    for (const Collider& collider : p.bodyColliders)
    {
        auto it = std::find_if(p.nodes.begin(), p.nodes.end(), [&](const TransformNode& node) {
            return node.name == collider.name;
        });
        if (it == p.nodes.end())
            continue;

        const glm::mat4& xform = it->worldTransform;

        // Compute capsule axis from collider bounds, then sample along it.
        // This replaces the old per-vertex sampling that caused limb snagging
        // on world geometry corners.
        glm::vec3 localCenter = (collider.localMin + collider.localMax) * 0.5f;
        glm::vec3 localExtents = (collider.localMax - collider.localMin) * 0.5f;
        float axisLen = glm::length(localExtents);
        glm::vec3 axisDir(0.0f, 0.0f, 1.0f);
        if (axisLen > 0.001f)
            axisDir = localExtents / axisLen;
        axisLen = std::min(axisLen, MAX_COLLIDER_AXIS);

        glm::vec3 worldCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
        if (glm::length(worldCenter - p.pos) > MAX_BODY_EXTENT)
            continue; // garbage transform — ignore this body part

        float radius = std::min(localExtents.x, localExtents.y) * 1.5f;
        // Scale radius up slightly to fill gaps between old triangle vertices
        radius = std::max(radius, BODY_SAMPLE_RADIUS);
        radius = std::min(radius, 0.35f); // cap to avoid excessive size

        glm::vec3 worldA = worldCenter - glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));
        glm::vec3 worldB = worldCenter + glm::vec3(xform * glm::vec4(axisDir * axisLen, 0.0f));
        if (glm::length(worldB - worldA) < 0.001f) {
            // Nearly spherical part: emit a single sample
            samples.push_back(worldCenter);
            continue;
        }

        // Sample 3 spheres along the capsule axis for smooth sliding
        for (int i = 0; i < 3; i++) {
            float t = (float)i / 2.0f;
            glm::vec3 pt = worldA + (worldB - worldA) * t;
            samples.push_back(pt);
        }
    }

    return samples;
}
