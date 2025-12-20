// C:\important\quiet\n\mimita-public\mimita-public\src\physics\collision-capsule-aabb.cpp
// dec 18 2025
/**
 * purpose
 * json collision  function haver 
 * so we dont have fucntions in phsics 
 */

#include "collision-capsule-aabb.h"
#include "physics/config.h"
#include <algorithm>
#include <cmath>
// debugging
#include <thread>
#include <chrono>
#include <cstdio>

// why unused i dont know but wahteve dec 19 2025 

// static glm::vec3 closestPointOnSegment(
//     const glm::vec3& p,
//     const glm::vec3& a,
//     const glm::vec3& b
// ) {
//     glm::vec3 ab = b - a;
//     float t = glm::dot(p - a, ab) / glm::dot(ab, ab);
//     t = glm::clamp(t, 0.0f, 1.0f);
//     return a + ab * t;
// }

static glm::vec3 clampPointAABB(
    const glm::vec3& p,
    const glm::vec3& c,
    const glm::vec3& h
) {
    return glm::clamp(p, c - h, c + h);
}

glm::vec3 collideCapsuleAABBMove(
    const Capsule& capStart,
    const glm::vec3& move,
    const glm::vec3& boxCenter,
    const glm::vec3& boxSize,
    bool& onGround,
    glm::vec3* outNormalLocal
) {
    Capsule cap = capStart;
    cap.a += move;
    cap.b += move;

    // testing no half size mults dec 19 2025
    glm::vec3 half = boxSize * 0.5f;
    glm::vec3 expandedHalf = half + glm::vec3(cap.r);

    // dec 19 2025 todo DONT SAMPLE JUST CHECK THE ENTIRRREEE BLOCK 
    // Closest point on segment to AABB (no sampling)
    glm::vec3 segDir = cap.b - cap.a;
    float segLen2 = glm::dot(segDir, segDir);

    float t = 0.0f;
    if (segLen2 > 0.0f) {
        glm::vec3 c = clampPointAABB(cap.a, boxCenter, expandedHalf);
        t = glm::dot(c - cap.a, segDir) / segLen2;
        t = glm::clamp(t, 0.0f, 1.0f);
    }

    glm::vec3 bestCap = cap.a + segDir * t;
    glm::vec3 bestBox = clampPointAABB(bestCap, boxCenter, expandedHalf);

    glm::vec3 delta = bestCap - bestBox;
    float d2 = glm::dot(delta, delta);

    const float EPS = 1e-6f;

    if (d2 <= cap.r * cap.r + EPS) {


        float dist = sqrtf(glm::max(d2, EPS));

        glm::vec3 n;
        if (dist > EPS) {
            n = delta / dist;
        } else {
            // Degenerate case: capsule axis inside expanded box
            // Push up by strongest separating axis (z-up bias)
            n = glm::vec3(0, 0, 1);
        }

        if (outNormalLocal) *outNormalLocal = n;

        float pen = cap.r - dist;
        if (pen <= 0.0f)
            return move;

        glm::vec3 out = move + n * pen;

        float into = glm::dot(out, n);
        if (into < 0.0f)
            out -= n * into;

        if (n.z >= MIN_GROUND_DOT) {
            onGround = true;

            glm::vec3 horiz(out.x, out.y, 0.0f);
            horiz -= n * glm::dot(horiz, n);
            out.x = horiz.x;
            out.y = horiz.y;
        }

        return out;
    }

    if (outNormalLocal) *outNormalLocal = glm::vec3(0.0f);
    return move;

}
