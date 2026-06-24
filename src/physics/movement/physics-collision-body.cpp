#include <algorithm>
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
        return;
    }

    p.collision.hasWeaponCollisionCapsule = false;
}

// Collect sphere samples from all body parts + weapon for contact testing.
// Returns spheres with their movement delta (root movement + animation delta).
std::vector<BodyWeaponSphere> collectBodyWeaponSpheres(Player& p)
{
    constexpr int SPHERES_PER_CAPSULE = 5;
    std::vector<BodyWeaponSphere> spheres;
    spheres.reserve(p.physicalBody.parts.size() * SPHERES_PER_CAPSULE + SPHERES_PER_CAPSULE);

    glm::vec3 rootMove = p.vel * 0.0f; // static check at current position

    // 1. Body part spheres
    for (const PhysicalBodyPart& part : p.physicalBody.parts)
    {
        glm::vec3 center;
        float radius;
        if (!computeBodyPartCenter(part.worldTransform, part.collider, center, radius))
            continue;

        glm::vec3 prevCenter;
        computeBodyPartCenter(part.previousWorldTransform, part.collider, prevCenter, radius);
        glm::vec3 sweepDelta = center - prevCenter;

        for (int si = 0; si < SPHERES_PER_CAPSULE; ++si)
        {
            spheres.push_back({center, radius, part.name.c_str(), sweepDelta});
        }
    }

    // 2. Weapon capsule spheres
    if (p.collision.hasWeaponCollisionCapsule)
    {
        const Capsule& wc = p.weaponCollisionCapsule;
        for (int si = 0; si < SPHERES_PER_CAPSULE; ++si)
        {
            float t = (SPHERES_PER_CAPSULE > 1)
                ? (float)si / (float)(SPHERES_PER_CAPSULE - 1) : 0.5f;
            glm::vec3 spherePos = wc.a + (wc.b - wc.a) * t;
            spheres.push_back({spherePos, wc.r, "weapon", glm::vec3(0.0f)});
        }
    }

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
    std::vector<RecoveryContact> contacts;
    constexpr float SLIDE_SLOP = 0.002f;

    for (const BodyWeaponSphere& bs : spheres)
    {
        for (int triIdx : candidates)
        {
            if (triIdx < 0 || triIdx >= (int)world.collisionMesh.triangles.size())
                continue;

            const CollisionTriangle& tri = world.collisionMesh.triangles[triIdx];

            float hitTime = 1.0f;
            glm::vec3 hitNormal, hitPoint;
            if (sweepSphereTriangle(bs.center, bs.sweepDelta, bs.radius, tri,
                                    hitTime, hitNormal, hitPoint) && hitTime < 1.0f)
            {
                float depth = bs.radius - glm::dot(bs.center - hitPoint, hitNormal);
                if (depth > SLIDE_SLOP) {
                    contacts.push_back({hitNormal, hitPoint, depth, triIdx, nullptr, bs.label});
                    continue;
                }
            }

            glm::vec3 normal;
            float depth;
            Contact c;
            if (sphereTriangleContact(bs.center, bs.radius, tri, c) && c.penetration > SLIDE_SLOP)
            {
                contacts.push_back({c.normal, c.point, c.penetration, triIdx, nullptr, bs.label});
            }
        }
    }

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
