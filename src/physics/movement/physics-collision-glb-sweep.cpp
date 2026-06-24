#include "physics/movement/physics-collision-shared.h"
#include "physics/movement/physics-collision-glb-sweep.h"

#include <cmath>
#include <glm/glm.hpp>
#include <cstdio>

// =====================================================
// Triangle geometry helpers
// =====================================================

glm::vec3 closestPointOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + ab * v;
    }

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + ac * w;
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + (c - b) * w;
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

static float pointSegmentDistanceSq(glm::vec3 p, glm::vec3 a, glm::vec3 b)
{
    glm::vec3 ab = b - a;
    float abLenSq = glm::dot(ab, ab);
    if (abLenSq <= 0.00000001f)
        return glm::dot(p - a, p - a);

    float t = glm::clamp(glm::dot(p - a, ab) / abLenSq, 0.0f, 1.0f);
    glm::vec3 closest = a + ab * t;
    return glm::dot(p - closest, p - closest);
}

bool pointInTriangle(glm::vec3 p, const CollisionTriangle& tri)
{
    glm::vec3 closest = closestPointOnTriangle(p, tri.a, tri.b, tri.c);
    glm::vec3 delta = closest - p;
    return glm::dot(delta, delta) < 0.0001f;
}

const char* triangleFeatureLabel(const CollisionTriangle& tri, const glm::vec3& point)
{
    constexpr float FEATURE_EPS = 0.025f;
    constexpr float FEATURE_EPS_SQ = FEATURE_EPS * FEATURE_EPS;

    if (glm::dot(point - tri.a, point - tri.a) <= FEATURE_EPS_SQ ||
        glm::dot(point - tri.b, point - tri.b) <= FEATURE_EPS_SQ ||
        glm::dot(point - tri.c, point - tri.c) <= FEATURE_EPS_SQ)
        return "vertex";

    if (pointSegmentDistanceSq(point, tri.a, tri.b) <= FEATURE_EPS_SQ ||
        pointSegmentDistanceSq(point, tri.b, tri.c) <= FEATURE_EPS_SQ ||
        pointSegmentDistanceSq(point, tri.c, tri.a) <= FEATURE_EPS_SQ)
        return "edge";

    return "face";
}

// =====================================================
// Sweep primitives
// =====================================================

static bool sweepSpherePoint(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    glm::vec3 point,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
) {
    float a = glm::dot(move, move);
    if (a < ALMOST_ZERO)
        return false;

    glm::vec3 rel = start - point;
    float b = 2.0f * glm::dot(rel, move);
    float c = glm::dot(rel, rel) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return false;

    float t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f)
        return false;

    glm::vec3 center = start + move * t;
    glm::vec3 n = center - point;
    if (glm::dot(n, n) < 0.000001f)
        n = -glm::normalize(move);
    else
        n = glm::normalize(n);

    hitTime = t;
    hitNormal = n;
    hitPoint = point;
    return true;
}

bool sweepSphereEdge(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    glm::vec3 edgeA,
    glm::vec3 edgeB,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
) {
    glm::vec3 edgeDir = edgeB - edgeA;
    float edgeLen = glm::length(edgeDir);
    if (edgeLen < 0.000001f)
        return false;
    edgeDir /= edgeLen;

    glm::vec3 rel = start - edgeA;
    float proj = glm::dot(rel, edgeDir);
    glm::vec3 relPerp = rel - edgeDir * proj;
    glm::vec3 movePerp = move - edgeDir * glm::dot(move, edgeDir);

    float a = glm::dot(movePerp, movePerp);
    if (a < ALMOST_ZERO)
        return false;

    float b = 2.0f * glm::dot(relPerp, movePerp);
    float c = glm::dot(relPerp, relPerp) - radius * radius;

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return false;

    float t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0.0f || t > 1.0f)
        return false;

    glm::vec3 centerAtT = start + move * t;
    glm::vec3 relAtT = centerAtT - edgeA;
    float projAtT = glm::dot(relAtT, edgeDir);
    if (projAtT < 0.0f || projAtT > edgeLen)
        return false;

    glm::vec3 closestOnEdge = edgeA + edgeDir * projAtT;
    glm::vec3 normal = centerAtT - closestOnEdge;
    float dist = glm::length(normal);
    if (dist < 0.000001f)
        return false;
    normal /= dist;

    hitTime = t;
    hitNormal = normal;
    hitPoint = closestOnEdge;
    return true;
}

bool sweepSphereTriangle(
    glm::vec3 start,
    glm::vec3 move,
    float radius,
    const CollisionTriangle& tri,
    float& hitTime,
    glm::vec3& hitNormal,
    glm::vec3& hitPoint
) {
    float bestT = 1.0f;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP(0.0f);
    bool hit = false;

    glm::vec3 n = tri.normal;
    float dist = glm::dot(start - tri.a, n);
    if (dist < 0.0f)
    {
        n = -n;
        dist = -dist;
    }

    float denom = glm::dot(move, n);
    if (denom < -ALMOST_ZERO)
    {
        float t = (radius - dist) / denom;
        if (t >= 0.0f && t <= 1.0f)
        {
            glm::vec3 centerAtHit = start + move * t;
            glm::vec3 planePoint = centerAtHit - n * radius;
            if (pointInTriangle(planePoint, tri))
            {
                bestT = t;
                bestN = n;
                bestP = planePoint;
                hit = true;
            }
        }
    }

    {
        glm::vec3 edgePairs[3][2] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};
        for (auto& ep : edgePairs)
        {
            float t = 1.0f;
            glm::vec3 en(0.0f);
            glm::vec3 epPt(0.0f);
            if (sweepSphereEdge(start, move, radius, ep[0], ep[1], t, en, epPt) && t < bestT)
            {
                bestT = t;
                bestN = en;
                bestP = epPt;
                hit = true;
            }
        }
    }

    glm::vec3 pts[3] = {tri.a, tri.b, tri.c};
    for (glm::vec3 pt : pts)
    {
        float t = 1.0f;
        glm::vec3 pn(0.0f);
        glm::vec3 pp(0.0f);
        if (sweepSpherePoint(start, move, radius, pt, t, pn, pp) && t < bestT)
        {
            bestT = t;
            bestN = pn;
            bestP = pp;
            hit = true;
        }
    }

    if (!hit)
        return false;

    hitTime = bestT;
    hitNormal = bestN;
    hitPoint = bestP;
    return true;
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

// =====================================================
// Capsule-triangle functions
// =====================================================

bool capsuleTriangleSweep(
    const Capsule& cap,
    const glm::vec3& move,
    const CollisionTriangle& tri,
    int triIndex,
    SweepHit& out
) {
    constexpr int NUM_SAMPLES = 7;
    glm::vec3 samples[NUM_SAMPLES];
    for (int i = 0; i < NUM_SAMPLES; ++i) {
        float t = (float)i / (float)(NUM_SAMPLES - 1);
        samples[i] = cap.a + (cap.b - cap.a) * t;
    }
    bool hit = false;
    float bestT = 1.0f;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP(0.0f);

    for (glm::vec3 sample : samples)
    {
        float t = 1.0f;
        glm::vec3 n(0.0f);
        glm::vec3 p(0.0f);
        float skinRadius = cap.r;
        if (sweepSphereTriangle(sample, move, skinRadius, tri, t, n, p) && t < bestT)
        {
            bestT = t;
            bestN = n;
            bestP = p;
            hit = true;
        }
    }

    if (!hit)
        return false;

    out.hit = true;
    out.time = bestT;
    out.normal = bestN;
    out.point = bestP;
    out.triangleIndex = triIndex;
    out.colliderName = "world_glb";
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
