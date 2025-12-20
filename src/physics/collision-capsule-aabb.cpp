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

static glm::vec3 closestPointOnSegment(
    const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b
) {
    glm::vec3 ab = b - a;
    float t = glm::dot(p - a, ab) / glm::dot(ab, ab);
    t = glm::clamp(t, 0.0f, 1.0f);
    return a + ab * t;
}

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

    glm::vec3 half = boxSize * 0.5f;

    glm::vec3 bestCap(0.0f);
    glm::vec3 bestBox(0.0f);
    float bestD2 = 1e30f;

    // sample along capsule segment
    const int SAMPLES = 8;
    for (int i = 0; i < SAMPLES; i++) {
        float t = i / float(SAMPLES - 1);
        glm::vec3 p = glm::mix(cap.a, cap.b, t);

        glm::vec3 q = clampPointAABB(p, boxCenter, half);
        float d2 = glm::dot(p - q, p - q);

        if (d2 < bestD2) {
            bestD2 = d2;
            bestCap = p;
            bestBox = q;
        }
    }

    if (bestD2 >= cap.r * cap.r) {
        if (outNormalLocal) *outNormalLocal = glm::vec3(0.0f);
        return move;
    }

    float dist = sqrtf(glm::max(bestD2, 1e-8f));
    float pen = cap.r - dist;

    glm::vec3 n = (bestCap - bestBox) / dist;

    if (outNormalLocal)
        *outNormalLocal = n;

    glm::vec3 out = move + n * pen;

    float into = glm::dot(out, n);
    if (into < 0.0f)
        out -= n * into;

    // ground test is LOCAL here
    if (n.z >= MIN_GROUND_DOT) {
        onGround = true;

        glm::vec3 horiz(out.x, out.y, 0.0f);
        horiz -= n * glm::dot(horiz, n);

        out.x = horiz.x;
        out.y = horiz.y;
    }

    return out;
}
