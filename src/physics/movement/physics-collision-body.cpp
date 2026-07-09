#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include "physics/config.h"
#include "entities/player.h"
#include "world/world.h"
#include "physics/movement/physics-collision.h"
#include "physics/movement/physics-collision-shared.h"
#include "combat/weapon-collision-config.h"
#include "debug/debug-log.h"

#define BWLOG(...) Debug::logThrottled(Debug::Category::Collision, "bw-investigate", 1.0f, __VA_ARGS__)

// Small skin/margin on weapon and body collision spheres to prevent exact-edge
// contact on triangle seams and corners. Applied the same way as COLLISION_SKIN
// for the root capsule: added to sphere radius during contact detection, then
// subtracted from the resulting penetration depth.
constexpr float WEAPON_COLLISION_SKIN = 0.04f;

BWInvestigate gBW;

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
        p.weaponCollisionDebug.valid = false;
        p.weaponCollisionDebug.spheres.clear();
        p.weaponCollisionDebug.capsule.enabled = false;
        return;
    }

    // Apply JSON config — this is the ONLY source of weapon collision data.
    // Handles prev saving, populates debug spheres/capsule, sets fromJsonConfig.
    WeaponCollisionJsonConfig::instance().applyCollisionConfig(p);

    if (!p.weaponCollisionDebug.fromJsonConfig) {
        // No JSON config for this weapon: no weapon collision.
        // This is intentional — weapon collision is fully data-driven.
        p.weaponCollisionDebug.valid = false;
        p.weaponCollisionDebug.spheres.clear();
        p.weaponCollisionDebug.capsule.enabled = false;
    }
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
std::vector<RecoveryContact> collectBodyWeaponContacts(
    const Player& p,
    const World& world,
    const std::vector<int>& candidates,
    const std::vector<BodyWeaponSphere>& spheres
) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<RecoveryContact> contacts;
    constexpr float SLIDE_SLOP = 0.002f;

    gBW.candidateCount = (int)candidates.size();
    int totalTests = 0;
    int sweepHits = 0;

    for (const BodyWeaponSphere& bs : spheres)
    {
        for (int triIdx : candidates)
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
                bool sweepHit = sweepSphereTriangle(bs.center, bs.sweepDelta, bs.radius + WEAPON_COLLISION_SKIN, tri,
                                                     hitTime, hitNormal, hitPoint);
                auto tsw1 = std::chrono::steady_clock::now();
                gBW.sweepSphereTriangleMs += std::chrono::duration<float, std::milli>(tsw1 - tsw0).count();
                gBW.sweepTests++;

                if (sweepHit && hitTime < 1.0f)
                {
                    float depth = (bs.radius + WEAPON_COLLISION_SKIN) - glm::dot(bs.center - hitPoint, hitNormal);
                    depth = std::max(0.0f, depth - WEAPON_COLLISION_SKIN);
                    if (depth > SLIDE_SLOP) {
                        contacts.push_back({hitNormal, hitPoint, depth, triIdx, nullptr, bs.label});
                        ++sweepHits;
                        continue;
                    }
                }
            }

            auto tst0 = std::chrono::steady_clock::now();
            Contact c;
            bool staticHit = sphereTriangleContact(bs.center, bs.radius + WEAPON_COLLISION_SKIN, tri, c);
            auto tst1 = std::chrono::steady_clock::now();
            gBW.sphereTriangleContactMs += std::chrono::duration<float, std::milli>(tst1 - tst0).count();
            gBW.staticTests++;

            if (staticHit && c.penetration > (SLIDE_SLOP + WEAPON_COLLISION_SKIN))
            {
                float skinPen = std::max(0.0f, c.penetration - WEAPON_COLLISION_SKIN);
                contacts.push_back({c.normal, c.point, skinPen, triIdx, nullptr, bs.label});
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

        glm::vec3 worldCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
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
