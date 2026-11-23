// Minimal world-ray query declarations for translation units that do not need
// the full movement collision API.
#pragma once

#include <glm/glm.hpp>

class World;

bool rayTraverseGridCells(
    const World& world,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    float maxDist,
    float& hitDist,
    glm::vec3* outNormal);
