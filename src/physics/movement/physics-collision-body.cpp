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
    if (!p.collision.hasWeaponCollisionCapsule)
        return;

    for (const PhysicalBodyPart& part : p.physicalBody.parts)
    {
        if (part.name != "rightArm")
            continue;

        glm::mat4 weaponXform = part.worldTransform * p.weaponLocalToArm;
        p.weaponCollisionCapsule.a = glm::vec3(weaponXform * glm::vec4(p.weaponGripLocal, 1.0f));
        p.weaponCollisionCapsule.b = glm::vec3(weaponXform * glm::vec4(p.weaponMuzzleLocal, 1.0f));
        p.weaponCollisionCapsule.r = p.weaponRadiusLocal;

        // Compute world transform for configurable weapon colliders
        glm::mat4 wcWorld = weaponXform;
        p.weaponCollisionWorld = wcWorld;
        return;
    }

    p.collision.hasWeaponCollisionCapsule = false;
    p.weaponCollisionWorld = glm::mat4(1.0f);
}

// Generate sphere samples for configurable weapon colliders from JSON collision config.
// Uses fixed 5 samples per collider along the dominant axis.
void collectWeaponConfigSpheres(
    Player& p,
    std::vector<BodyWeaponSphere>& spheres
) {
    auto t0 = std::chrono::steady_clock::now();
    auto& cfg = p.weaponCollisionConfig;
    gBW.configColliderCount = (int)cfg.colliders.size();
    if (!cfg.enabled || cfg.colliders.empty())
        return;

    glm::mat4 weaponWorld = p.weaponCollisionWorld;

    for (const auto& wc : cfg.colliders)
    {
        // Build collider local transform
        glm::mat4 local(1.0f);
        local = glm::translate(local, wc.position);
        local = glm::rotate(local, glm::radians(wc.rotationDegrees.x), glm::vec3(1,0,0));
        local = glm::rotate(local, glm::radians(wc.rotationDegrees.y), glm::vec3(0,1,0));
        local = glm::rotate(local, glm::radians(wc.rotationDegrees.z), glm::vec3(0,0,1));

        glm::mat4 worldXform = weaponWorld * local;
        glm::vec3 halfSize = wc.size * 0.5f;

        // Find dominant axis
        float ex = std::fabs(halfSize.x);
        float ey = std::fabs(halfSize.y);
        float ez = std::fabs(halfSize.z);
        int domAxis = 0;
        float domLen = ex;
        if (ey > domLen) { domAxis = 1; domLen = ey; }
        if (ez > domLen) { domAxis = 2; domLen = ez; }
        if (domLen < 0.001f) domLen = 0.001f;

        // Local axis direction in world space
        glm::vec3 axisDir(0.0f);
        axisDir[domAxis] = 1.0f;
        glm::vec3 worldAxis = glm::normalize(glm::vec3(worldXform * glm::vec4(axisDir, 0.0f)));

        // Center point in world space
        glm::vec3 center = glm::vec3(worldXform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

        // Sphere radius = half of the smaller-face diagonal, clamped
        float r = 0.15f;
        {
            float s1 = (domAxis == 0) ? ey : ex;
            float s2 = (domAxis == 2) ? ey : ez;
            r = std::sqrt(s1 * s1 + s2 * s2);
            r = std::max(r, 0.10f);
            r = std::min(r, 0.40f);
        }

        // Fixed 5 samples along the dominant axis (matching pre-regression behavior).
        constexpr int CONFIG_COLLIDER_SPHERES = 5;
        gBW.configSpheresGenerated += CONFIG_COLLIDER_SPHERES;

        for (int si = 0; si < CONFIG_COLLIDER_SPHERES; ++si)
        {
            float t = (CONFIG_COLLIDER_SPHERES > 1)
                ? (float)si / (float)(CONFIG_COLLIDER_SPHERES - 1) * 2.0f - 1.0f
                : 0.0f;
            glm::vec3 pos = center + worldAxis * domLen * t;
            spheres.push_back({pos, r, wc.name.c_str(), glm::vec3(0.0f)});
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    gBW.configSpheresMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

// Collect sphere samples from all body parts + weapon for contact testing.
// Returns spheres with their movement delta (root movement + animation delta).
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

    // 2. Weapon capsule spheres (fixed 5 samples along axis)
    {
        auto tw0 = std::chrono::steady_clock::now();
        if (p.collision.hasWeaponCollisionCapsule)
        {
            constexpr int WEAPON_CAPSULE_SAMPLES = 5;
            const Capsule& wc = p.weaponCollisionCapsule;
            gBW.weaponCapsuleSphereCount = WEAPON_CAPSULE_SAMPLES;
            for (int si = 0; si < WEAPON_CAPSULE_SAMPLES; ++si)
            {
                float t = (WEAPON_CAPSULE_SAMPLES > 1)
                    ? (float)si / (float)(WEAPON_CAPSULE_SAMPLES - 1) : 0.5f;
                glm::vec3 spherePos = wc.a + (wc.b - wc.a) * t;
                spheres.push_back({spherePos, wc.r, "weapon", p.vel * 0.016f});
            }
        }
        auto tw1 = std::chrono::steady_clock::now();
        gBW.weaponCapsuleSpheresMs = std::chrono::duration<float, std::milli>(tw1 - tw0).count();
    }

    // 3. Configurable weapon collider spheres (from weapon JSON collision config)
    collectWeaponConfigSpheres(p, spheres);
    gBW.configSphereCount = gBW.configSpheresGenerated;

    // 4. Mesh vertex spheres (from weapon GLB collision mesh)
    {
        auto& cm = p.weaponCollisionMesh;
        if (cm.valid && !cm.worldPositions.empty()) {
            int meshCount = 0;
            for (size_t i = 0; i < cm.worldPositions.size(); i++) {
                glm::vec3 sweepDelta{0.0f};
                if (i < cm.prevPositions.size())
                    sweepDelta = cm.worldPositions[i] - cm.prevPositions[i];
                spheres.push_back({cm.worldPositions[i], cm.vertexRadius, "weapon_mesh", sweepDelta});
                meshCount++;
            }
            gBW.configSpheresGenerated = meshCount;
            gBW.configSphereCount = meshCount;
        }
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
