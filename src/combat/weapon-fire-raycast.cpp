#include "combat/weapon-fire.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include "camera.h"
#include "entities/player.h"
#include "world/world.h"
#include "npc/npc.h"
#include "network/multiplayer-context.h"

namespace WeaponFire {

AimTarget computeAimTarget(
    const Camera& camera,
    const World& world,
    NpcSystem& npcs,
    const std::unordered_map<uint32_t, Player>* remotePlayers)
{
    constexpr float MAX_SHOT_DISTANCE = 100.0f;
    float cameraNearest = MAX_SHOT_DISTANCE;

    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float d = 0.0f;
        if (rayTriangle(camera.pos, camera.front, tri, d))
            cameraNearest = std::min(cameraNearest, d);
    }
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
        npc.body.updateModelWorldTransforms();
        for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
            float d = 0.0f;
            glm::vec3 nml;
            if (rayAabb(camera.pos, camera.front, center - half, center + half, d, nml))
                cameraNearest = std::min(cameraNearest, d);
        }
    }
    if (remotePlayers) {
        for (const auto& entry : *remotePlayers) {
            const Player& remote = entry.second;
            if (remote.dead || remote.currentHp <= 0) {
                if (MimitaNet::gNetHitDebug)
                    printf("[NET HIT SKIP AIM] remote id=%u reason=%s hp=%d\n",
                           entry.first,
                           remote.dead ? "dead" : "hp-zero",
                           remote.currentHp);
                continue;
            }
            const Capsule capsule = remote.getCapsule();
            const glm::vec3 mn(remote.pos.x - capsule.r, remote.pos.y - capsule.r, capsule.a.z - capsule.r);
            const glm::vec3 mx(remote.pos.x + capsule.r, remote.pos.y + capsule.r, capsule.b.z + capsule.r);
            float d = 0.0f;
            glm::vec3 nml;
            if (rayAabb(camera.pos, camera.front, mn, mx, d, nml))
                cameraNearest = std::min(cameraNearest, d);
        }
    }

    AimTarget result;
    result.worldPoint = camera.pos + camera.front * cameraNearest;
    result.cameraDistance = cameraNearest;
    return result;
}

bool rayTriangle(const glm::vec3& origin, const glm::vec3& direction,
                 const CollisionTriangle& tri, float& distance) {
    glm::vec3 e1 = tri.b - tri.a;
    glm::vec3 e2 = tri.c - tri.a;
    glm::vec3 p = glm::cross(direction, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 0.000001f) return false;
    float inv = 1.0f / det;
    glm::vec3 t = origin - tri.a;
    float u = glm::dot(t, p) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 q = glm::cross(t, e1);
    float v = glm::dot(direction, q) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    distance = glm::dot(e2, q) * inv;
    return distance > 0.0f;
}

bool rayAabb(const glm::vec3& origin, const glm::vec3& direction,
             const glm::vec3& mn, const glm::vec3& mx,
             float& distance, glm::vec3& normal) {
    float tmin = 0.0f;
    float tmax = 1000.0f;
    normal = glm::vec3(0.0f);
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) < 0.000001f) {
            if (origin[axis] < mn[axis] || origin[axis] > mx[axis]) return false;
            continue;
        }
        float inv = 1.0f / direction[axis];
        float a = (mn[axis] - origin[axis]) * inv;
        float b = (mx[axis] - origin[axis]) * inv;
        float sign = -1.0f;
        if (a > b) { std::swap(a, b); sign = 1.0f; }
        if (a > tmin) {
            tmin = a;
            normal = glm::vec3(0.0f);
            normal[axis] = sign;
        }
        tmax = std::min(tmax, b);
        if (tmin > tmax) return false;
    }
    distance = tmin;
    return distance >= 0.0f;
}

} // namespace WeaponFire
