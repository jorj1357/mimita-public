#include "audio/audio-acoustics.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <vector>

#include "physics/movement/physics-collision.h"
#include "physics/physics-types.h"
#include "physics/ray-utils.h"
#include "world/world.h"

namespace {

bool raycastWorld(const World& world, const glm::vec3& origin,
                  const glm::vec3& direction, float maxDistance,
                  float& outDistance)
{
    if (maxDistance <= 0.0f || world.collisionMesh.triangles.empty())
        return false;

    static thread_local std::vector<int> candidates;
    candidates.clear();

    const glm::vec3 end = origin + direction * maxDistance;
    AABB bounds;
    bounds.min = glm::min(origin, end) - glm::vec3(0.05f);
    bounds.max = glm::max(origin, end) + glm::vec3(0.05f);
    appendChunkTrianglesForAABB(world, bounds, 0.0f, candidates,
                                "audioAcoustics", true);

    float nearest = maxDistance;
    bool hit = false;
    for (int index : candidates) {
        if (index < 0 ||
            index >= static_cast<int>(world.collisionMesh.triangles.size()))
            continue;
        float distance = 0.0f;
        if (!rayTriangle(origin, direction,
                         world.collisionMesh.triangles[(size_t)index],
                         distance))
            continue;
        if (distance > 0.02f && distance < nearest) {
            nearest = distance;
            hit = true;
        }
    }
    if (hit)
        outDistance = nearest;
    return hit;
}

float probeEnclosure(const World& world, const glm::vec3& position, float radius)
{
    if (radius <= 0.0f || world.collisionMesh.triangles.empty())
        return 0.0f;

    // Ignore straight down: a floor under the listener is not an enclosure.
    static const glm::vec3 directions[] = {
        { 1.0f,  0.0f, 0.0f}, {-1.0f,  0.0f, 0.0f},
        { 0.0f,  1.0f, 0.0f}, { 0.0f, -1.0f, 0.0f},
        { 0.0f,  0.0f, 1.0f},
        { 0.70710678f,  0.70710678f, 0.0f},
        { 0.70710678f, -0.70710678f, 0.0f},
        {-0.70710678f,  0.70710678f, 0.0f},
        {-0.70710678f, -0.70710678f, 0.0f},
    };

    float enclosed = 0.0f;
    int hits = 0;
    for (const glm::vec3& direction : directions) {
        float distance = 0.0f;
        if (!raycastWorld(world, position, direction, radius, distance))
            continue;
        if (distance < 0.6f)
            continue; // Surface the listener is standing on/next to.
        enclosed += std::clamp(1.0f - distance / radius, 0.0f, 1.0f);
        ++hits;
    }
    if (hits == 0)
        return 0.0f;
    return std::clamp(enclosed / (float)hits, 0.0f, 1.0f);
}

glm::vec3 safeCross(const glm::vec3& direction)
{
    glm::vec3 right = glm::cross(direction, glm::vec3(0.0f, 0.0f, 1.0f));
    if (glm::dot(right, right) < 0.0001f)
        right = glm::cross(direction, glm::vec3(0.0f, 1.0f, 0.0f));
    if (glm::dot(right, right) < 0.0001f)
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(right);
}

} // namespace

AcousticEstimate estimateWorldAcoustics(const World* world,
                                        const SpatialProfile& profile,
                                        const glm::vec3& source,
                                        const glm::vec3& listener)
{
    AcousticEstimate estimate;
    if (!world || world->collisionMesh.triangles.empty())
        return estimate;

    const glm::vec3 offset = listener - source;
    const float distance = glm::length(offset);
    if (distance < 0.05f)
        return estimate;

    const glm::vec3 direction = offset / distance;
    const glm::vec3 right = safeCross(direction);
    const glm::vec3 up = glm::normalize(glm::cross(right, direction));

    // Three source-to-listener probes catch a wall or a thin obstruction
    // without pretending to trace reflections.
    const glm::vec3 probes[] = {
        source,
        source + right * 0.40f,
        source + up * 0.40f,
    };
    int blocked = 0;
    for (const glm::vec3& origin : probes) {
        float hitDistance = 0.0f;
        if (raycastWorld(*world, origin, direction, distance, hitDistance) &&
            hitDistance < distance - 0.6f) {
            ++blocked;
        }
    }
    estimate.obstruction = (float)blocked / (float)std::size(probes);

    const float listenerEnclosure = probeEnclosure(*world, listener,
                                                   profile.probeRadius);
    const float sourceEnclosure = probeEnclosure(*world, source,
                                                 profile.probeRadius);
    estimate.enclosure = std::clamp(
        (listenerEnclosure + sourceEnclosure) * 0.5f, 0.0f, 1.0f);
    return estimate;
}
