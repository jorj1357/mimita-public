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

static glm::vec3 clampPointAABB(
    const glm::vec3& p,
    const glm::vec3& c,
    const glm::vec3& h)
{
    return glm::clamp(p, c - h, c + h);
}

glm::vec3 collideCapsuleAABBMove(
    const Capsule& capStart,
    const glm::vec3& move,
    const glm::vec3& boxCenter,
    const glm::vec3& boxSize,
    bool& onGround)
{
    Capsule cap = capStart;
    cap.a += move;
    cap.b += move;

    glm::vec3 half = boxSize * 0.5f;

    // which one idk dec 18 2025 
    glm::vec3 bestP(0.0f), bestQ(0.0f);
    // glm::vec3 bestP, bestQ;
    float bestD2 = 1e30f;

    for (glm::vec3 p : { cap.a, cap.b }) {
        glm::vec3 q = clampPointAABB(p, boxCenter, half);
        float d2 = glm::dot(p - q, p - q);
        if (d2 < bestD2) {
            bestD2 = d2;
            bestP = p;
            bestQ = q;
        }
    }

    if (bestD2 >= cap.r * cap.r)
        return move;

    float dist = sqrtf(glm::max(bestD2, 1e-8f));
    float pen = cap.r - dist;

    glm::vec3 n = (dist > 1e-6f)
        ? (bestP - bestQ) / dist
        : glm::vec3(0,1,0);

    if (n.y > MAX_SLOPE_ANGLE) onGround = true;

    glm::vec3 out = move + n * pen;
    float into = glm::dot(out, n);
    if (into < 0.0f) out -= n * into;

    return out;
}
