// 07 21 2026, 20 45
/* purpose
* Declares deterministic pellet spread generation for generic hitscan weapons.
* Shares one seeded direction pattern between client prediction, server authority, and tests.
 * Keeps shotgun-like multi-ray behavior bounded by a fixed pellet cap.
* Does NOT apply damage, trace collision, send packets, or render muzzle effects.
* Does NOT own weapon definitions, ammo runtime, or world/NPC/player collision state.
* Does NOT randomize spread from global process state.
*/

#pragma once

#include <glm/glm.hpp>

constexpr int MAX_PELLETS_PER_BLAST = 100;

struct PelletPatternConfig
{
    int pelletCount = 1;
    float spreadDegrees = 0.0f;
    uint32_t spreadSeed = 0;
    bool squareSpread = false;
};

int generatePelletDirections(
    const glm::vec3& baseDirection,
    const PelletPatternConfig& config,
    glm::vec3* outDirections,
    int outputCapacity);
