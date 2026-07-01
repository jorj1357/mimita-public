#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"

#include <chrono>
#include <cmath>
#include <glm/glm.hpp>
#include <cstdio>
#include "world/world.h"
#include "debug/debug-log.h"

#define CONTACT_LOG(...) Debug::logThrottled(Debug::Category::Collision, "contact-collect", 1.0f, __VA_ARGS__)

bool sphereTriangleContact(
    glm::vec3 center,
    float radius,
    const CollisionTriangle& tri,
    Contact& contact
) {
    glm::vec3 closest = closestPointOnTriangle(center, tri.a, tri.b, tri.c);
    glm::vec3 delta = center - closest;
    float dist2 = glm::dot(delta, delta);
    if (dist2 > radius * radius)
        return false;

    float dist = sqrtf(std::max(dist2, 0.0f));
    glm::vec3 n;
    if (dist > 0.00001f) {
        n = delta / dist;
        float barycentricSum = 0.0f;
        glm::vec3 edgeDirs[3] = {tri.b - tri.a, tri.c - tri.b, tri.a - tri.c};
        for (int ei = 0; ei < 3; ++ei) {
            float edgeLen = glm::length(edgeDirs[ei]);
            if (edgeLen < 0.0001f) continue;
            glm::vec3 edgeDir = edgeDirs[ei] / edgeLen;
            glm::vec3 toClosest = closest - (ei == 0 ? tri.a : (ei == 1 ? tri.b : tri.c));
            float alongEdge = glm::dot(toClosest, edgeDir);
            if (alongEdge >= 0.0f && alongEdge <= edgeLen)
                barycentricSum += 1.0f;
        }
        if (barycentricSum < 2.5f) {
            float blend = 0.3f;
            n = glm::normalize(n + tri.normal * blend);
        }
    } else {
        float side = glm::dot(center - tri.a, tri.normal);
        n = (side >= 0.0f) ? tri.normal : -tri.normal;
    }

    contact.point = closest;
    contact.normal = n;
    contact.penetration = radius - dist;
    return true;
}

bool capsuleTriangleContact(
    const Capsule& cap,
    const CollisionTriangle& tri,
    int triIndex,
    Contact& out
) {
    constexpr int NUM_SAMPLES = 7;
    glm::vec3 samples[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        float t = (float)i / (float)(NUM_SAMPLES - 1);
        samples[i] = cap.a + (cap.b - cap.a) * t;
    }
    bool hit = false;
    Contact best;

    for (glm::vec3 sample : samples)
    {
        Contact c;
        float skinRadius = cap.r + COLLISION_SKIN;
        if (sphereTriangleContact(sample, skinRadius, tri, c))
        {
            c.penetration = std::max(0.0f, c.penetration - COLLISION_SKIN);
            if (!hit || c.penetration > best.penetration)
            {
                best = c;
                hit = true;
            }
        }
    }

    if (!hit)
        return false;

    best.triangleIndex = triIndex;
    out = best;
    return true;
}

std::vector<RecoveryContact> collectCapsuleRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<int>& candidates
) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<RecoveryContact> contacts;
    constexpr int SAMPLE_COUNT = 5;
    for (int triIndex : candidates)
    {
        if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
            continue;
        const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
        for (int i = 0; i < SAMPLE_COUNT; ++i)
        {
            float t = (SAMPLE_COUNT == 1) ? 0.0f : (float)i / (float)(SAMPLE_COUNT - 1);
            glm::vec3 sample = cap.a + (cap.b - cap.a) * t;
            Contact c;
            if (capsuleTriangleContact(cap, tri, triIndex, c))
            {
                RecoveryContact rc;
                rc.normal = c.normal;
                rc.point = c.point;
                rc.penetration = c.penetration;
                rc.triangleIndex = triIndex;
                rc.block = nullptr;
                rc.label = "glb-recovery";
                contacts.push_back(rc);
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    CONTACT_LOG(
        "[CONTACT_CAPSULE] candidates=%zu contacts=%zu samples=%d elapsedMs=%.2f\n",
        candidates.size(), contacts.size(), SAMPLE_COUNT, elapsedMs);
    if (contacts.size() > 50) {
        Debug::warn(Debug::Category::Collision,
            "[CONTACT WARNING] collectCapsuleRecoveryContacts: %zu contacts exceeds threshold 50\n",
            contacts.size());
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
    auto t0 = std::chrono::steady_clock::now();
    std::vector<RecoveryContact> contacts;
    int capsuleHits = 0, sphereHits = 0;
    for (int triIndex : candidates)
    {
        if (triIndex < 0 || triIndex >= (int)world.collisionMesh.triangles.size())
            continue;
        const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
        Contact c;
        if (capsuleTriangleContact(cap, tri, triIndex, c))
        {
            RecoveryContact rc;
            rc.normal = c.normal;
            rc.point = c.point;
            rc.penetration = c.penetration;
            rc.triangleIndex = triIndex;
            rc.block = nullptr;
            rc.label = "glb-body-recovery";
            contacts.push_back(rc);
            ++capsuleHits;
        }
        for (const glm::vec3& sample : bodySamples)
        {
            Contact sc;
            if (sphereTriangleContact(sample, bodySampleRadius, tri, sc))
            {
                RecoveryContact rc;
                rc.normal = sc.normal;
                rc.point = sc.point;
                rc.penetration = sc.penetration;
                rc.triangleIndex = triIndex;
                rc.block = nullptr;
                rc.label = "glb-sphere-recovery";
                contacts.push_back(rc);
                ++sphereHits;
            }
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    CONTACT_LOG(
        "[CONTACT_GLB] candidates=%zu capsuleHits=%d sphereHits=%d total=%zu bodySamples=%zu elapsedMs=%.2f\n",
        candidates.size(), capsuleHits, sphereHits, contacts.size(), bodySamples.size(), elapsedMs);
    return contacts;
}
