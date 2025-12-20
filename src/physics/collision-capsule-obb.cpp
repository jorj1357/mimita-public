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
// debugging
#include <thread>
#include <chrono>
#include <cstdio>

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

    glm::vec3 nLocal(0.0f);

    glm::vec3 resolvedLocal =
        collideCapsuleAABBMove(
            capLocal,
            moveLocal,
            glm::vec3(0.0f),
            // testing no size mult
            // dont multiply by 0.5f or 2.0f anywhere but collision capsule aabb
            boxHalf,
            // boxHalf * 2.0f,
            onGround,
            &nLocal
        );

    glm::vec3 resolvedWorld = boxRot * resolvedLocal;

    return resolvedWorld;
}


