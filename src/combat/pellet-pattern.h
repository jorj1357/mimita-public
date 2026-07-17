#pragma once

#include <glm/glm.hpp>

constexpr int MAX_PELLETS_PER_BLAST = 16;

struct PelletPatternConfig
{
    int pelletCount = 1;
    float spreadDegrees = 0.0f;
    uint32_t spreadSeed = 0;
};

int generatePelletDirections(
    const glm::vec3& baseDirection,
    const PelletPatternConfig& config,
    glm::vec3* outDirections,
    int outputCapacity);
