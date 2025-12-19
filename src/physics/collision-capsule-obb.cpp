// C:\important\quiet\n\mimita-public\mimita-public\src\physics\collision-capsule-obb.cpp
// dec 18 2025
/**
 * purpose
lets us do ramps and surfing 
like csgo 
 */

#include "collision-capsule-obb.h"
#include "collision-capsule-aabb.h"
#include <glm/glm.hpp>

glm::vec3 collideCapsuleOBBMove(
    const Capsule& capWorld,
    const glm::vec3& moveWorld,
    const glm::vec3& boxPos,
    const glm::vec3& boxHalf,
    const glm::mat3& boxRot,
    bool& onGround
) {
    // inverse rotation (world -> box local)
    glm::mat3 invR = glm::transpose(boxRot);

    // transform capsule into box local space
    Capsule capLocal;
    capLocal.r = capWorld.r;
    capLocal.a = invR * (capWorld.a - boxPos);
    capLocal.b = invR * (capWorld.b - boxPos);

    glm::vec3 moveLocal = invR * moveWorld;

    // collide in local AABB space
    glm::vec3 resolvedLocal =
        collideCapsuleAABBMove(
            capLocal,
            moveLocal,
            glm::vec3(0.0f),
            boxHalf * 2.0f,
            onGround
        );

    // rotate movement back to world space
    glm::vec3 resolvedWorld = boxRot * resolvedLocal;

    return resolvedWorld;
}

