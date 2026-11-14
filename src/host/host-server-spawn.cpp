// 2026-11-13
// Resident safe-spawn selection.
//
// This is a resident copy of the server spawn policy. It keeps the host free of
// game-owned collision helpers; spawn selection is rare, so a bounded linear
// scan is acceptable.

#include "host/host-server-spawn.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

#include "network/server.h"

namespace mimita::host {
namespace {

bool spawn_ray_triangle(const glm::vec3& origin, const glm::vec3& direction,
                        const CollisionTriangle& triangle, float& distance) {
    const glm::vec3 edge1 = triangle.b - triangle.a;
    const glm::vec3 edge2 = triangle.c - triangle.a;
    const glm::vec3 p = glm::cross(direction, edge2);
    const float det = glm::dot(edge1, p);
    if (std::fabs(det) < 0.000001f) {
        return false;
    }
    const float invDet = 1.0f / det;
    const glm::vec3 t = origin - triangle.a;
    const float u = glm::dot(t, p) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    const glm::vec3 q = glm::cross(t, edge1);
    const float v = glm::dot(direction, q) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    distance = glm::dot(edge2, q) * invDet;
    return distance > 0.0f;
}

bool raycast_down(const MimitaNet::HeadlessWorld& world,
                  const glm::vec3& origin, float maxDistance,
                  glm::vec3& outHit, glm::vec3& outNormal) {
    const glm::vec3 direction(0.0f, 0.0f, -1.0f);
    float nearest = maxDistance;
    bool hit = false;
    for (std::size_t index = 0; index < world.triangles.size(); ++index) {
        if (index < world.solidIds.size() &&
            world.solidIds[index] == kRemovedCollisionSolidId) {
            continue;
        }
        const CollisionTriangle& triangle = world.triangles[index];
        float distance = 0.0f;
        if (!spawn_ray_triangle(origin, direction, triangle, distance) ||
            distance >= nearest) {
            continue;
        }
        nearest = distance;
        outHit = origin + direction * distance;
        outNormal = triangle.normal;
        hit = true;
    }
    return hit;
}

bool point_inside_world(const MimitaNet::HeadlessWorld& world,
                        const glm::vec3& point) {
    const glm::vec3 direction =
        glm::normalize(glm::vec3(1.0f, 0.3713907f, 0.193f));
    int hits = 0;
    for (std::size_t index = 0; index < world.triangles.size(); ++index) {
        if (index < world.solidIds.size() &&
            world.solidIds[index] == kRemovedCollisionSolidId) {
            continue;
        }
        float distance = 0.0f;
        if (spawn_ray_triangle(point, direction, world.triangles[index],
                               distance) &&
            distance > 1.0e-4f) {
            ++hits;
        }
    }
    return (hits % 2) != 0;
}

glm::vec3 closest_point_triangle(glm::vec3 p, glm::vec3 a, glm::vec3 b,
                                 glm::vec3 c) {
    const glm::vec3 ab = b - a;
    const glm::vec3 ac = c - a;
    const glm::vec3 ap = p - a;
    const float d1 = glm::dot(ab, ap);
    const float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp);
    const float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        return a + (d1 / (d1 - d3)) * ab;
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp);
    const float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 >= 0.0f) {
        return a + (d2 / (d2 - d6)) * ac;
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        return b + ((d4 - d3) / ((d4 - d3) + (d5 - d6))) * (c - b);
    }
    const float denom = 1.0f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

bool capsule_fits(const MimitaNet::HeadlessWorld& world,
                  const glm::vec3& pos) {
    const glm::vec3 samples[3] = {
        pos + glm::vec3(0.0f, 0.0f, -PLAYER_HEIGHT * 0.5f + PLAYER_RADIUS),
        pos,
        pos + glm::vec3(0.0f, 0.0f, PLAYER_HEIGHT * 0.5f - PLAYER_RADIUS),
    };
    for (const glm::vec3& sample : samples) {
        if (point_inside_world(world, sample)) {
            return false;
        }
        for (std::size_t index = 0; index < world.triangles.size(); ++index) {
            if (index < world.solidIds.size() &&
                world.solidIds[index] == kRemovedCollisionSolidId) {
                continue;
            }
            const CollisionTriangle& triangle = world.triangles[index];
            const glm::vec3 closest = closest_point_triangle(
                sample, triangle.a, triangle.b, triangle.c);
            const float distance = glm::length(sample - closest);
            if (distance < PLAYER_RADIUS && distance >= 0.00001f) {
                return false;
            }
        }
    }
    return true;
}

bool validate_standing_spot(
    const MimitaNet::HeadlessWorld& world, const glm::vec3& candidate,
    glm::vec3& outStanding,
    const std::chrono::steady_clock::time_point* deadline) {
    const float maxDistance =
        std::max(200.0f, (world.boundsMax.z - world.boundsMin.z) + 64.0f);

    glm::vec3 upHit{0.0f};
    glm::vec3 upNormal{0.0f};
    const bool hasCeiling = raycast_down(
        world, candidate + glm::vec3(0.0f, 0.0f, 0.1f), 64.0f, upHit,
        upNormal);
    const float ceilingZ = hasCeiling ? upHit.z : candidate.z + 64.0f;

    const float lifts[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f,
                           3.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f};
    const float rings[] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f};
    for (float ring : rings) {
        const int directions = ring <= 0.0f ? 1 : 8;
        for (int directionIndex = 0; directionIndex < directions;
             ++directionIndex) {
            const float angle =
                directions == 1 ? 0.0f : directionIndex * 0.7853981634f;
            const glm::vec3 offset(std::cos(angle) * ring,
                                   std::sin(angle) * ring, 0.0f);
            for (float lift : lifts) {
                if (deadline &&
                    std::chrono::steady_clock::now() >= *deadline) {
                    return false;
                }
                if (lift > 0.0f && candidate.z + lift >= ceilingZ - 0.05f) {
                    break;
                }
                const glm::vec3 probe =
                    candidate + offset + glm::vec3(0.0f, 0.0f, lift);
                if (point_inside_world(world, probe)) {
                    continue;
                }
                glm::vec3 hit{0.0f};
                glm::vec3 normal{0.0f};
                if (!raycast_down(world, probe + glm::vec3(0.0f, 0.0f, 0.5f),
                                  maxDistance, hit, normal)) {
                    continue;
                }
                const glm::vec3 standing(
                    probe.x, probe.y,
                    hit.z + PLAYER_HEIGHT * 0.5f + 0.02f);
                if (standing.z + PLAYER_HEIGHT * 0.5f >= ceilingZ - 0.05f) {
                    continue;
                }
                if (point_inside_world(world, standing)) {
                    continue;
                }
                if (!capsule_fits(world, standing)) {
                    continue;
                }
                outStanding = standing;
                return true;
            }
        }
    }
    return false;
}

bool probe_safe_standing(const MimitaNet::HeadlessWorld& world,
                         const glm::vec3& hint, glm::vec3& outPos,
                         float& outYaw) {
    const glm::vec3 mn = world.boundsMin;
    const glm::vec3 mx = world.boundsMax;
    if (!(mx.x > mn.x) || !(mx.y > mn.y) || !(mx.z > mn.z)) {
        return false;
    }
    constexpr float kProbeRadius = 32.0f;
    constexpr float kProbeStep = 4.0f;
    const auto probeDeadline = std::chrono::steady_clock::now() +
                               std::chrono::milliseconds(250);
    const float x0 = std::max(mn.x, hint.x - kProbeRadius);
    const float x1 = std::min(mx.x, hint.x + kProbeRadius);
    const float y0 = std::max(mn.y, hint.y - kProbeRadius);
    const float y1 = std::min(mx.y, hint.y + kProbeRadius);
    const float maxDrop = (mx.z - mn.z) + 16.0f;
    float bestDistSq = std::numeric_limits<float>::max();
    bool found = false;
    for (float x = x0; x <= x1; x += kProbeStep) {
        for (float y = y0; y <= y1; y += kProbeStep) {
            if (std::chrono::steady_clock::now() >= probeDeadline) {
                return found;
            }
            const glm::vec3 origin(x, y, std::max(mn.z, mx.z - 0.5f));
            glm::vec3 hit{0.0f};
            glm::vec3 normal{0.0f};
            if (!raycast_down(world, origin, maxDrop, hit, normal)) {
                continue;
            }
            const glm::vec3 candidate(x, y, hit.z + PLAYER_HEIGHT * 0.5f);
            glm::vec3 standing{0.0f};
            if (!validate_standing_spot(world, candidate, standing,
                                        &probeDeadline)) {
                continue;
            }
            const float dx = standing.x - hint.x;
            const float dy = standing.y - hint.y;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq < bestDistSq) {
                bestDistSq = distanceSq;
                outPos = standing;
                outYaw = 0.0f;
                found = true;
            }
        }
    }
    return found;
}

}  // namespace

bool pickServerSpawn(const MimitaNet::HeadlessWorld* world,
                     std::uint32_t playerId, bool overrideEnabled,
                     const glm::vec3& overridePosition, SpawnSelection* out) {
    if (!out) {
        return false;
    }
    *out = SpawnSelection{};
    const std::size_t playerIndex =
        playerId > 0 ? static_cast<std::size_t>(playerId - 1) : 0;

    if (overrideEnabled) {
        out->position = overridePosition;
        if (world && !world->spawnPoints.empty()) {
            out->yaw =
                world->spawnPoints[playerIndex % world->spawnPoints.size()].yaw;
        }
        out->index = -1;
        out->validated = false;
        return true;
    }
    if (!world) {
        return false;
    }

    if (!world->spawnPoints.empty()) {
        const std::size_t defaultIndex = playerIndex % world->spawnPoints.size();
        for (std::size_t k = 0; k < world->spawnPoints.size(); ++k) {
            const std::size_t index =
                (defaultIndex + k) % world->spawnPoints.size();
            glm::vec3 standing{0.0f};
            if (!validate_standing_spot(*world,
                                        world->spawnPoints[index].position,
                                        standing, nullptr)) {
                continue;
            }
            out->position = standing;
            out->yaw = world->spawnPoints[index].yaw;
            out->index = static_cast<int>(index);
            out->validated = true;
            return true;
        }
        glm::vec3 probePos{0.0f};
        float probeYaw = 0.0f;
        if (probe_safe_standing(*world,
                                world->spawnPoints[defaultIndex].position,
                                probePos, probeYaw)) {
            out->position = probePos;
            out->yaw = probeYaw;
            out->index = -1;
            out->validated = true;
            return true;
        }
        out->position = world->spawnPoints[defaultIndex].position;
        out->yaw = world->spawnPoints[defaultIndex].yaw;
        out->index = static_cast<int>(defaultIndex);
        out->validated = false;
        return true;
    }

    const glm::vec3 hint(
        (world->boundsMin.x + world->boundsMax.x) * 0.5f,
        (world->boundsMin.y + world->boundsMax.y) * 0.5f, 0.0f);
    glm::vec3 probePos{0.0f};
    float probeYaw = 0.0f;
    if (probe_safe_standing(*world, hint, probePos, probeYaw)) {
        out->position = probePos;
        out->yaw = probeYaw;
        out->index = -1;
        out->validated = true;
        return true;
    }
    const glm::vec3 fallback(
        1.0f + static_cast<float>(playerIndex) * 1.5f, 5.0f, 30.0f);
    glm::vec3 standing{0.0f};
    if (validate_standing_spot(*world, fallback, standing, nullptr)) {
        out->position = fallback;
        out->yaw = 0.0f;
        out->index = 0;
        out->validated = true;
        return true;
    }
    return false;
}

}  // namespace mimita::host
