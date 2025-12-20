// C:\important\quiet\n\mimita-public\mimita-public\src\physics\collision-capsule-aabb.h
// dec 18 2025
/**
 * purpose
 * json collision header
 * so we dont have fucntions in phsics cluttering it up and makING ME MAD
 */

#pragma once
#include <glm/glm.hpp>
#include "physics/physics-types.h"

glm::vec3 collideCapsuleAABBMove(
    const Capsule& capStart,
    const glm::vec3& move,
    const glm::vec3& boxCenter,
    const glm::vec3& boxSize,
    bool& onGround,
    glm::vec3* outNormalLocal
);
