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

glm::vec3 collideCapsuleOBBMove(
    const Capsule& capWorld,
    const glm::vec3& moveWorld,
    const glm::vec3& boxPos,
    const glm::vec3& boxHalf,
    const glm::mat3& boxRot,
    bool& onGround
) {
    glm::mat3 invR = glm::transpose(boxRot);

    Capsule capLocal;
    capLocal.r = capWorld.r;
    capLocal.a = invR * (capWorld.a - boxPos);
    capLocal.b = invR * (capWorld.b - boxPos);

    glm::vec3 moveLocal = invR * moveWorld;

    bool hitGroundLocal = false;
    glm::vec3 nLocal(0,0,0);

    glm::vec3 resolvedLocal =
        collideCapsuleAABBMove(
            capLocal,
            moveLocal,
            glm::vec3(0.0f),
            boxHalf * 2.0f,
            hitGroundLocal,
            &nLocal
        );

    // Convert normal back to world and do the REAL ground test (z-up)
    if (glm::length(nLocal) > 0.00001f) {
        glm::vec3 nWorld = glm::normalize(boxRot * nLocal);
        if (glm::dot(nWorld, glm::vec3(0,0,1)) >= MIN_GROUND_DOT) {
            onGround = true;
        }
    }

    glm::vec3 resolvedWorld = boxRot * resolvedLocal;
    return resolvedWorld;
}
