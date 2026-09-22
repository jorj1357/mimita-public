// 07 21 2026, 20 45
/* purpose
* Declares deterministic pellet spread generation for generic hitscan weapons.
* Shares one FIXED grid direction pattern between client prediction, server authority, and tests.
* Keeps shotgun-like multi-ray behavior bounded by a small fixed pellet cap.
* Does NOT apply damage, trace collision, send packets, or render muzzle effects.
* Does NOT own weapon definitions, ammo runtime, or world/NPC/player collision state.
* Does NOT randomize spread: the pattern is identical every shot (no RNG, no seed).
*
* Header-only so the SAME implementation serves the cold EXE and the hot game DLL
* (one spread owner across the hot/cold boundary; there is no second copy).
*/

#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

constexpr int MAX_PELLETS_PER_BLAST = 16;

struct PelletPatternConfig
{
    int pelletCount = 1;
    float spreadDegrees = 0.0f;
    // Inert: kept for wire/struct compatibility. The pattern is deterministic
    // and ignores this value.
    uint32_t spreadSeed = 0;
};

// Deterministic fixed grid: the single source of the pellet pattern for every
// path (client prediction, server authority, NPCs, hot behavior). No RNG and no
// seed. cols = ceil(sqrt(pelletCount)), rows = ceil(pelletCount / cols); each
// pellet is offset on a normalized [-1, 1] grid scaled by half the spread angle.
inline int buildFixedPelletDirections(
    const glm::vec3& baseDirection,
    int pelletCount,
    float spreadDegrees,
    glm::vec3* outDirections,
    int outputCapacity)
{
    if (!outDirections || outputCapacity <= 0)
        return 0;

    const int count = std::min({pelletCount, outputCapacity, MAX_PELLETS_PER_BLAST});
    if (count <= 0)
        return 0;

    const glm::vec3 aim = glm::length(baseDirection) > 0.0001f
        ? glm::normalize(baseDirection)
        : glm::vec3(1.0f, 0.0f, 0.0f);

    if (spreadDegrees <= 0.0f)
    {
        outDirections[0] = aim;
        return 1;
    }

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    if (std::fabs(glm::dot(aim, up)) > 0.99f)
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(aim, up));
    const glm::vec3 localUp = glm::normalize(glm::cross(right, aim));

    const float halfAngleRad = glm::radians(spreadDegrees * 0.5f);
    const int cols = std::max(1, (int)std::ceil(std::sqrt((float)count)));
    const int rows = std::max(1, (int)std::ceil((float)count / (float)cols));

    int idx = 0;
    for (int r = 0; r < rows && idx < count; ++r)
    {
        for (int c = 0; c < cols && idx < count; ++c, ++idx)
        {
            const float fx = cols > 1 ? (c / ((float)cols - 1.0f)) * 2.0f - 1.0f : 0.0f;
            const float fy = rows > 1 ? (r / ((float)rows - 1.0f)) * 2.0f - 1.0f : 0.0f;
            const float ha = halfAngleRad * fx;
            const float va = halfAngleRad * fy;
            const glm::quat rot = glm::angleAxis(ha, localUp) * glm::angleAxis(va, right);
            outDirections[idx] = glm::normalize(rot * aim);
        }
    }

    return count;
}

// Back-compat wrapper over buildFixedPelletDirections (ignores spreadSeed).
inline int generatePelletDirections(
    const glm::vec3& baseDirection,
    const PelletPatternConfig& config,
    glm::vec3* outDirections,
    int outputCapacity)
{
    return buildFixedPelletDirections(
        baseDirection, config.pelletCount, config.spreadDegrees,
        outDirections, outputCapacity);
}

// Deterministic single-ray spread (no RNG). `cycleIndex` is advanced in place
// and indexes a fixed offset sequence, so the same burst replays the same
// pattern and the result is identical for a given cycle position.
inline glm::vec3 buildFixedSpreadDirection(
    const glm::vec3& baseDirection,
    float spreadDegrees,
    unsigned int& cycleIndex)
{
    if (spreadDegrees <= 0.0f)
        return baseDirection;

    // Fixed offset sequence: center, then the four axes, then the four
    // diagonals. Repeats after kPatternCount shots.
    static const glm::vec2 kSpreadPattern[] = {
        { 0.0f,     0.0f     },
        { 1.0f,     0.0f     },
        { 0.0f,     1.0f     },
        { -1.0f,    0.0f     },
        { 0.0f,     -1.0f    },
        { 0.7071f,  0.7071f  },
        { -0.7071f, 0.7071f  },
        { -0.7071f, -0.7071f },
        { 0.7071f,  -0.7071f }
    };
    constexpr int kPatternCount = (int)(sizeof(kSpreadPattern) / sizeof(kSpreadPattern[0]));
    const glm::vec2 offset = kSpreadPattern[cycleIndex % kPatternCount];
    ++cycleIndex;

    const glm::vec3 dir = glm::length(baseDirection) > 0.0001f
        ? glm::normalize(baseDirection)
        : glm::vec3(1.0f, 0.0f, 0.0f);
    const float radius = std::tan(glm::radians(spreadDegrees));
    const glm::vec3 up = std::fabs(dir.z) < 0.99f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 right = glm::normalize(glm::cross(dir, up));
    const glm::vec3 fwd = glm::normalize(glm::cross(right, up));
    return glm::normalize(dir + (right * offset.x + fwd * offset.y) * radius);
}
