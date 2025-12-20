// C:\important\quiet\n\mimita-public\mimita-public\src\physics\collision-capsule-obb.h
// dec 18 2025
/**
 * purpose
lets us do ramps and surfing 
like csgo 
but headers and fucntins 
 */

#pragma once

#include <glm/glm.hpp>
#include "physics-types.h"

glm::vec3 collideCapsuleOBBMove(
    const Capsule& capWorld,
    const glm::vec3& moveWorld,
    const glm::vec3& boxPos,
    const glm::vec3& boxHalf,
    const glm::mat3& boxRot,
    bool& onGround
);
