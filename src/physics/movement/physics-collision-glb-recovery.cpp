#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"
#include "debug/debug-log.h"
#include "devtools/dev-config.h"
#include "config/player-settings.h"
#include "world/world.h"

#include <algorithm>
#include <cstdio>

// =====================================================
// DEBUG TOGGLE
// =====================================================
#define PHYS_LOG(...) Debug::logThrottled(Debug::Category::Collision, "physics-collision", DebugConfig::PRINT_INTERVAL, __VA_ARGS__)

bool rejectBelowTopFaceContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    const glm::vec3& normal,
    const glm::vec3& point,
    int triangleIndex,
    const char* phase)
{
    if (normal.z <= MAX_WALKABLE_SLOPE_DOT)
        return false;

    const float feetZ = cap.a.z - cap.r;
    const float tolerance = std::max(
        GetPlayerSettings().collisionSeamTolerance,
        COLLISION_SKIN + 0.005f);
    if (feetZ + tolerance >= point.z)
        return false;

    const float triMaxZ = std::max({tri.a.z, tri.b.z, tri.c.z});
    Debug::logThrottled(Debug::Category::Collision, "seam-filter-below-top",
        DebugConfig::PRINT_INTERVAL,
        "[SEAM FILTER] ignored top contact reason=below_top_face phase=%s tri=%d contactZ=%.3f feetZ=%.3f triMaxZ=%.3f\n",
        phase ? phase : "unknown", triangleIndex, point.z, feetZ, triMaxZ);
    return true;
}

std::vector<RecoveryContact> collectCapsuleRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<int>& candidates
) {
    std::vector<RecoveryContact> contacts;
    for (int triIndex : candidates)
    {
        if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
            continue;
        Contact contact;
        if (capsuleTriangleContact(cap, world.collisionMesh.triangles[triIndex], triIndex, contact))
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (rejectBelowTopFaceContact(cap, tri, contact.normal, contact.point, triIndex, "recovery"))
                continue;
            contacts.push_back({
                contact.normal,
                contact.point,
                contact.penetration,
                contact.triangleIndex,
                nullptr,
                "capsule-triangle"
            });
        }
    }
    return contacts;
}

std::vector<RecoveryContact> collectGLBRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<glm::vec3>& bodySamples,
    const std::vector<int>& candidates,
    float bodySampleRadius
) {
    std::vector<RecoveryContact> contacts;

    for (int triIndex : candidates)
    {
        if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
            continue;

        Contact contact;
        if (capsuleTriangleContact(cap, world.collisionMesh.triangles[triIndex], triIndex, contact))
        {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (rejectBelowTopFaceContact(cap, tri, contact.normal, contact.point, triIndex, "glb-recovery"))
                continue;
            contacts.push_back({
                contact.normal,
                contact.point,
                contact.penetration,
                contact.triangleIndex,
                nullptr,
                "capsule-triangle"
            });
        }
    }

    for (glm::vec3 sample : bodySamples)
    {
        for (int triIndex : candidates)
        {
            if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
                continue;

            Contact contact;
            if (!sphereTriangleContact(sample, bodySampleRadius, world.collisionMesh.triangles[triIndex], contact))
                continue;

            contact.triangleIndex = triIndex;
            contacts.push_back({
                contact.normal,
                contact.point,
                contact.penetration,
                contact.triangleIndex,
                nullptr,
                "body-triangle"
            });
        }
    }

    return contacts;
}
