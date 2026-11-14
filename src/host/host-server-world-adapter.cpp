// 2026-11-13
// Resident read-only world adapter implementation.

#include "host/host-server-world-adapter.h"

#include <cmath>
#include <cstring>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "host/host-server-spawn.h"
#include "network/server.h"

namespace mimita::host {
namespace {

// Pure resident copy of the Möller-Trumbore test. Keeping it local avoids the
// resident host linking against a game-owned implementation.
bool hostRayTriangle(const glm::vec3& origin, const glm::vec3& direction,
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

}  // namespace

HostServerWorldAdapter& HostServerWorldAdapter::instance() {
    static HostServerWorldAdapter adapter;
    return adapter;
}

HostServerWorldAdapter::HostServerWorldAdapter() {
    adapter_.version = kWorldCapabilityVersion;
    adapter_.structSize = static_cast<std::uint32_t>(sizeof(WorldAdapterV1));
    adapter_.owner = this;
    adapter_.info = &HostServerWorldAdapter::infoThunk;
    adapter_.raycast = &HostServerWorldAdapter::raycastThunk;
    adapter_.pick_spawn = &HostServerWorldAdapter::pickSpawnThunk;
}

void HostServerWorldAdapter::bind(MimitaNet::HeadlessWorld* world) {
    world_ = world;
}

void HostServerWorldAdapter::setSpawnOverride(bool enabled, float x, float y,
                                              float z) {
    spawnOverrideEnabled_ = enabled;
    spawnOverridePosition_[0] = x;
    spawnOverridePosition_[1] = y;
    spawnOverridePosition_[2] = z;
}

void HostServerWorldAdapter::clear() {
    world_ = nullptr;
    spawnOverrideEnabled_ = false;
    spawnOverridePosition_[0] = 0.0f;
    spawnOverridePosition_[1] = 0.0f;
    spawnOverridePosition_[2] = 0.0f;
}

HostResult HostServerWorldAdapter::infoThunk(void* owner, WorldInfoV1* out) {
    auto* adapter = static_cast<HostServerWorldAdapter*>(owner);
    if (!adapter || !out || out->structSize < sizeof(WorldInfoV1)) {
        return HostResult::InvalidArgument;
    }
    std::memset(out, 0, sizeof(WorldInfoV1));
    out->structSize = static_cast<std::uint32_t>(sizeof(WorldInfoV1));
    out->version = kWorldCapabilityVersion;
    if (!adapter->world_) {
        return HostResult::Ok;
    }
    const MimitaNet::HeadlessWorld& world = *adapter->world_;
    out->loaded = 1;
    out->triangleCount = static_cast<std::uint32_t>(world.triangles.size());
    out->glbObjectCount = static_cast<std::uint32_t>(world.glbObjects.size());
    out->spawnPointCount = static_cast<std::uint32_t>(world.spawnPoints.size());
    out->boundsMin[0] = world.boundsMin.x;
    out->boundsMin[1] = world.boundsMin.y;
    out->boundsMin[2] = world.boundsMin.z;
    out->boundsMax[0] = world.boundsMax.x;
    out->boundsMax[1] = world.boundsMax.y;
    out->boundsMax[2] = world.boundsMax.z;
    return HostResult::Ok;
}

HostResult HostServerWorldAdapter::raycastThunk(
    void* owner, const float origin[3], const float direction[3],
    float maxDistance, WorldRayHitV1* out) {
    auto* adapter = static_cast<HostServerWorldAdapter*>(owner);
    if (!adapter || !origin || !direction || !out ||
        out->structSize < sizeof(WorldRayHitV1) || !(maxDistance > 0.0f)) {
        return HostResult::InvalidArgument;
    }
    std::memset(out, 0, sizeof(WorldRayHitV1));
    out->structSize = static_cast<std::uint32_t>(sizeof(WorldRayHitV1));
    out->version = kWorldCapabilityVersion;
    out->triangleIndex = static_cast<std::uint32_t>(-1);
    if (!adapter->world_) {
        return HostResult::Ok;
    }

    const glm::vec3 rayOrigin(origin[0], origin[1], origin[2]);
    glm::vec3 rayDirection(direction[0], direction[1], direction[2]);
    if (glm::length2(rayDirection) <= 0.0f) {
        return HostResult::InvalidArgument;
    }
    rayDirection = glm::normalize(rayDirection);

    float nearest = maxDistance;
    int selected = -1;
    const auto& triangles = adapter->world_->triangles;
    for (int i = 0; i < static_cast<int>(triangles.size()); ++i) {
        float distance = 0.0f;
        if (hostRayTriangle(rayOrigin, rayDirection, triangles[i], distance) &&
            distance < nearest) {
            nearest = distance;
            selected = i;
        }
    }
    if (selected < 0) {
        return HostResult::Ok;
    }
    const auto& triangle = triangles[static_cast<std::size_t>(selected)];
    const glm::vec3 point = rayOrigin + rayDirection * nearest;
    out->hit = 1;
    out->triangleIndex = static_cast<std::uint32_t>(selected);
    out->distance = nearest;
    out->point[0] = point.x;
    out->point[1] = point.y;
    out->point[2] = point.z;
    out->normal[0] = triangle.normal.x;
    out->normal[1] = triangle.normal.y;
    out->normal[2] = triangle.normal.z;
    return HostResult::Ok;
}

HostResult HostServerWorldAdapter::pickSpawnThunk(void* owner,
                                                   std::uint32_t playerId,
                                                   WorldSpawnV1* out) {
    auto* adapter = static_cast<HostServerWorldAdapter*>(owner);
    if (!adapter || !out || out->structSize < sizeof(WorldSpawnV1)) {
        return HostResult::InvalidArgument;
    }
    std::memset(out, 0, sizeof(WorldSpawnV1));
    out->structSize = static_cast<std::uint32_t>(sizeof(WorldSpawnV1));
    out->version = kWorldCapabilityVersion;
    out->spawnIndex = static_cast<std::uint32_t>(-1);
    const glm::vec3 overridePosition(adapter->spawnOverridePosition_[0],
                                     adapter->spawnOverridePosition_[1],
                                     adapter->spawnOverridePosition_[2]);
    SpawnSelection selection;
    if (!pickServerSpawn(adapter->world_, playerId,
                         adapter->spawnOverrideEnabled_, overridePosition,
                         &selection)) {
        return HostResult::NotFound;
    }
    out->found = 1;
    out->spawnIndex = selection.index >= 0
        ? static_cast<std::uint32_t>(selection.index)
        : static_cast<std::uint32_t>(-1);
    out->position[0] = selection.position.x;
    out->position[1] = selection.position.y;
    out->position[2] = selection.position.z;
    out->yaw = selection.yaw;
    return HostResult::Ok;
}

}  // namespace mimita::host
