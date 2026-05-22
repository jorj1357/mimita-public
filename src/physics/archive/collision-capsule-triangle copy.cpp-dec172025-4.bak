// C:\important\quiet\n\mimita-public\mimita-public\src\physics\collision-capsule-triangle.cpp
// dec 16 2025
/**
 * purpose
 * this has all the SB that we dont want in the phsics cpp file 
 * so phsics just calls the functions 
 * and its all nice and readable 
 */

// collision-capsule-triangle.cpp
// dec 16 2025
// REAL capsule vs triangle collision

#include "collision-capsule-triangle.h"
#include "physics/config.h"
#include <glm/glm.hpp>
#include <algorithm>

// ----------------------------
// Helpers
// ----------------------------

static glm::vec3 closestPointOnTriangle(const glm::vec3& p, const Triangle& t)
{
    // Ericson - Real-Time Collision Detection (barycentric region tests)
    const glm::vec3 a = t.a, b = t.b, c = t.c;
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    const glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    float vc = d1*d4 - d3*d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
    {
        float v = d1 / (d1 - d3);
        return a + v * ab;
    }

    const glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vb = d5*d2 - d1*d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
    {
        float w = d2 / (d2 - d6);
        return a + w * ac;
    }

    float va = d3*d6 - d5*d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
    {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + w * (c - b);
    }

    // inside face region
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return a + ab * v + ac * w;
}

static glm::vec3 closestPointOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 ab = b - a;
    float t = glm::dot(p - a, ab) / glm::dot(ab, ab);
    t = glm::clamp(t, 0.0f, 1.0f);
    return a + t * ab;
}

glm::vec3 collideCapsuleTriangleMove(
    const Capsule& cap0,
    const glm::vec3& move,
    const Triangle& tri,
    bool& onGround)
{
    // move capsule
    Capsule cap = cap0;
    cap.a += move;
    cap.b += move;

    // find closest point on triangle to capsule segment (approx but REAL surface)
    // take closest of: triangle-to-a, triangle-to-b, and segment-to-triangle using midpoint projection
    glm::vec3 pa = closestPointOnTriangle(cap.a, tri);
    glm::vec3 pb = closestPointOnTriangle(cap.b, tri);

    glm::vec3 mid = 0.5f * (cap.a + cap.b);
    glm::vec3 pm = closestPointOnTriangle(mid, tri);
    glm::vec3 sm = closestPointOnSegment(pm, cap.a, cap.b);

    // pick best (smallest distance)
    glm::vec3 bestCapP = cap.a;
    glm::vec3 bestTriP = pa;
    float bestD2 = glm::dot(cap.a - pa, cap.a - pa);

    float d2b = glm::dot(cap.b - pb, cap.b - pb);
    if (d2b < bestD2) { bestD2 = d2b; bestCapP = cap.b; bestTriP = pb; }

    float d2m = glm::dot(sm - pm, sm - pm);
    if (d2m < bestD2) { bestD2 = d2m; bestCapP = sm; bestTriP = pm; }

    float r = cap.r;
    if (bestD2 >= r*r) return move;

    float dist = sqrtf(bestD2);
    glm::vec3 n = (dist > 0.00001f) ? (bestCapP - bestTriP) / dist : glm::vec3(0,1,0);

    // slide: remove into-normal motion
    glm::vec3 outMove = move;
    float into = glm::dot(outMove, n);
    if (into < 0.0f) outMove -= n * into;

    // ground check using triangle normal (not the separation normal)
    glm::vec3 triN = glm::normalize(glm::cross(tri.b - tri.a, tri.c - tri.a));
    if (triN.y > MAX_SLOPE_ANGLE) onGround = true;

    // add push-out so we don't remain interpenetrating (tiny extra)
    float pen = r - dist;
    outMove += n * (pen + 0.001f);

    return outMove;
}

