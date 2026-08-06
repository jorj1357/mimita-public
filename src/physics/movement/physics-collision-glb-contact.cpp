#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <glm/glm.hpp>
#include <cstdio>
#include "world/world.h"
#include "debug/debug-log.h"

#define CONTACT_LOG(...) Debug::logThrottled(Debug::Category::Collision, "contact-collect", 1.0f, __VA_ARGS__)

// Closest point on a segment AB to a point P.
static glm::vec3 closestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 ab = b - a;
    float denom = glm::dot(ab, ab);
    if (denom < 1e-12f)
        return a;
    float t = std::clamp(glm::dot(p - a, ab) / denom, 0.0f, 1.0f);
    return a + ab * t;
}

// Closest points between two segments AB and CD; returns squared distance.
static float closestPointSegmentSegment(
    const glm::vec3& a, const glm::vec3& b,
    const glm::vec3& c, const glm::vec3& d,
    glm::vec3& outA, glm::vec3& outB)
{
    glm::vec3 ab = b - a;
    glm::vec3 cd = d - c;
    glm::vec3 ac = c - a;
    const float e = glm::dot(ab, ab);
    const float f = glm::dot(cd, cd);
    const float g = glm::dot(ab, cd);
    const float h = glm::dot(ac, cd);
    const float s1 = glm::dot(ac, ab);
    const float denom = e * f - g * g;
    const float safeF = f > 1e-12f ? f : 1.0f;
    const float safeE = e > 1e-12f ? e : 1.0f;
    float s = 0.0f;
    float t = 0.0f;
    if (denom > 1e-8f)
    {
        s = (g * h - s1 * f) / denom;
        t = (h + g * s) / safeF;
    }
    else
    {
        s = 0.0f;
        t = h / safeF;
    }
    s = std::clamp(s, 0.0f, 1.0f);
    t = std::clamp(t, 0.0f, 1.0f);
    s = std::clamp((g * t + s1) / safeE, 0.0f, 1.0f);
    t = std::clamp((h + g * s) / safeF, 0.0f, 1.0f);
    outA = a + ab * s;
    outB = c + cd * t;
    return glm::dot(outA - outB, outA - outB);
}

// Closest points between a segment AB and a triangle; returns squared distance.
// Combines face, edge, and vertex cases so no penetration is ever missed.
static float closestSegmentTriangle(
    const glm::vec3& a, const glm::vec3& b,
    const glm::vec3& t0, const glm::vec3& t1, const glm::vec3& t2,
    glm::vec3& outSeg, glm::vec3& outTri)
{
    float bestD = FLT_MAX;
    glm::vec3 bestSeg(0.0f);
    glm::vec3 bestTri(0.0f);
    auto consider = [&](const glm::vec3& seg, const glm::vec3& tri, float d) {
        if (d < bestD) { bestD = d; bestSeg = seg; bestTri = tri; }
    };

    // Segment endpoints vs triangle.
    {
        const glm::vec3 pa = closestPointOnTriangle(a, t0, t1, t2);
        consider(a, pa, glm::dot(a - pa, a - pa));
        const glm::vec3 pb = closestPointOnTriangle(b, t0, t1, t2);
        consider(b, pb, glm::dot(b - pb, b - pb));
    }

    // Triangle vertices vs segment.
    {
        const glm::vec3 verts[3] = {t0, t1, t2};
        for (int i = 0; i < 3; ++i)
        {
            const glm::vec3 sp = closestPointOnSegment(verts[i], a, b);
            consider(sp, verts[i], glm::dot(sp - verts[i], sp - verts[i]));
        }
    }

    // Segment vs each triangle edge.
    {
        const glm::vec3 edges[3][2] = {{t0, t1}, {t1, t2}, {t2, t0}};
        for (int i = 0; i < 3; ++i)
        {
            glm::vec3 sA, sB;
            const float d = closestPointSegmentSegment(a, b, edges[i][0], edges[i][1], sA, sB);
            consider(sA, sB, d);
        }
    }

    // Segment vs triangle face: closest point on the segment to the plane, kept
    // only if the plane point projects inside the triangle.
    {
        glm::vec3 n = glm::cross(t1 - t0, t2 - t0);
        const float nLen = glm::length(n);
        if (nLen > 1e-6f)
        {
            n /= nLen;
            glm::vec3 ab = b - a;
            const float dn = glm::dot(ab, n);
            const float d0 = glm::dot(a - t0, n);
            float s;
            if (std::fabs(dn) > 1e-8f)
                s = std::clamp(-d0 / dn, 0.0f, 1.0f);
            else
                s = (std::fabs(d0) <= std::fabs(glm::dot(b - t0, n))) ? 0.0f : 1.0f;
            const glm::vec3 segPt = a + ab * s;
            const glm::vec3 planePt = segPt - n * glm::dot(segPt - t0, n);

            // Barycentric inside test.
            const glm::vec3 v0 = t1 - t0;
            const glm::vec3 v1 = t2 - t0;
            const glm::vec3 v2 = planePt - t0;
            const float d00 = glm::dot(v0, v0);
            const float d01 = glm::dot(v0, v1);
            const float d11 = glm::dot(v1, v1);
            const float d20 = glm::dot(v2, v0);
            const float d21 = glm::dot(v2, v1);
            const float denom = d00 * d11 - d01 * d01;
            if (std::fabs(denom) > 1e-8f)
            {
                const float v = (d11 * d20 - d01 * d21) / denom;
                const float w = (d00 * d21 - d01 * d20) / denom;
                const float u = 1.0f - v - w;
                if (u >= -1e-4f && v >= -1e-4f && w >= -1e-4f)
                    consider(segPt, planePt, glm::dot(segPt - planePt, segPt - planePt));
            }
        }
    }

    outSeg = bestSeg;
    outTri = bestTri;
    return bestD;
}

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
    // Exact capsule-vs-triangle test: distance from the capsule axis segment to
    // the triangle. One exact test per triangle instead of sampling the capsule
    // at N points, so no penetration between samples is ever missed.
    const float radius = cap.r + COLLISION_SKIN;
    glm::vec3 segPt, triPt;
    const float distSq = closestSegmentTriangle(cap.a, cap.b, tri.a, tri.b, tri.c, segPt, triPt);
    if (distSq > radius * radius)
        return false;

    const float dist = sqrtf(std::max(distSq, 0.0f));
    glm::vec3 n;
    if (dist > 0.00001f)
    {
        n = (segPt - triPt) / dist;
    }
    else
    {
        // Segment just touches / crosses the triangle plane: direction is
        // ambiguous. Push the capsule away on the side that has any part of the
        // capsule above the surface, so a floor crossing pushes up and never
        // falls through. (Matches the old sampled behavior, which used the sample
        // nearest the plane on the above side.)
        const float da = glm::dot(cap.a - tri.a, tri.normal);
        const float db = glm::dot(cap.b - tri.a, tri.normal);
        n = (da >= 0.0f || db >= 0.0f) ? tri.normal : -tri.normal;
    }

    out.normal = n;
    out.point = triPt;
    out.penetration = std::max(0.0f, radius - dist - COLLISION_SKIN);
    out.triangleIndex = triIndex;
    return true;
}

std::vector<RecoveryContact> collectCapsuleRecoveryContacts(
    const World& world,
    const Capsule& cap,
    const std::vector<int>& candidates
) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<RecoveryContact> contacts;
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
            rc.label = "glb-recovery";
            contacts.push_back(rc);
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    float elapsedMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    CONTACT_LOG(
        "[CONTACT_CAPSULE] candidates=%zu contacts=%zu elapsedMs=%.2f\n",
        candidates.size(), contacts.size(), elapsedMs);
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
