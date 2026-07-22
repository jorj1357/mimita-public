// 07 21 2026, 20 45
/* purpose
* Implements deterministic seeded pellet spread for the generic hitscan execution path.
* Produces bounded normalized directions from base aim, spread degrees, pellet count, and seed.
* Keeps multi-pellet weapons reproducible across client retries and server dedupe.
* Does NOT apply damage, trace world geometry, send network packets, or create visual effects.
* Does NOT own weapon JSON loading, ammo, cooldown, or hit confirmation packets.
* Does NOT read global random state or frame time.
*/

#include "combat/pellet-pattern.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {

static uint32_t nextSpreadRand(uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

static float spreadUnit(uint32_t& state)
{
    const uint32_t v = nextSpreadRand(state);
    return ((v >> 8) & 0x00ffffffu) / 16777215.0f;
}

} // namespace

int generatePelletDirections(
    const glm::vec3& baseDirection,
    const PelletPatternConfig& config,
    glm::vec3* outDirections,
    int outputCapacity)
{
    int count = std::min({config.pelletCount, outputCapacity, MAX_PELLETS_PER_BLAST});
    glm::vec3 aim = glm::length(baseDirection) > 0.0001f
        ? glm::normalize(baseDirection)
        : glm::vec3(1.0f, 0.0f, 0.0f);
    if (count <= 0 || config.spreadDegrees <= 0.0f)
    {
        if (count > 0) outDirections[0] = aim;
        return count > 0 ? 1 : 0;
    }

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    if (std::fabs(glm::dot(aim, up)) > 0.99f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 right = glm::normalize(glm::cross(aim, up));
    glm::vec3 localUp = glm::normalize(glm::cross(right, aim));

    float halfAngleRad = glm::radians(config.spreadDegrees * 0.5f);
    uint32_t state = config.spreadSeed ? config.spreadSeed : 0x9e3779b9u;
    for (int idx = 0; idx < count; ++idx)
    {
        float u = spreadUnit(state);
        float v = spreadUnit(state);
        float r = std::sqrt(u);
        float theta = v * 6.28318530718f;
        float ha = std::cos(theta) * r * halfAngleRad;
        float va = std::sin(theta) * r * halfAngleRad;
        glm::quat rot = glm::angleAxis(ha, localUp) * glm::angleAxis(va, right);
        outDirections[idx] = glm::normalize(rot * aim);
    }

    return count;
}
