#include "combat/weapon-fire.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include "camera.h"
#include "config/gameplay-config.h"
#include "debug/debug-log.h"
#include "entities/player.h"
#include "world/world.h"
#include "npc/npc.h"
#include "network/multiplayer-context.h"

namespace WeaponFire {

namespace {
constexpr float kMaxShotDistance = 100.0f;
}

const char* aimHitKindName(AimHitKind kind)
{
    switch (kind) {
        case AimHitKind::World: return "world";
        case AimHitKind::Npc: return "npc";
        case AimHitKind::RemotePlayer: return "remote_player";
        case AimHitKind::None:
        default: return "none";
    }
}

AimSolution computeAim(
    const Camera& camera,
    const World& world,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    const std::unordered_map<uint32_t, Player>* remotePlayers)
{
    AimSolution result;
    result.origin = muzzlePos;
    result.cameraDistance = kMaxShotDistance;

    const GameplayAimMode mode = GameplayConfig::instance().aimMode();
    if (mode == GameplayAimMode::Crosshair) {
        AimTarget target = computeAimTarget(camera, world, npcs, remotePlayers);
        result.aimPoint = target.worldPoint;
        result.cameraDistance = target.cameraDistance;
        result.modeName = "crosshair";
        result.cameraHitKind = target.hitKind;
        result.usesCameraTarget = true;
    } else {
        result.aimPoint = camera.pos + camera.front * kMaxShotDistance;
        result.modeName = "world_hit";
        result.cameraHitKind = AimHitKind::None;
        result.usesCameraTarget = false;
    }

    glm::vec3 direction = result.aimPoint - muzzlePos;
    if (glm::length(direction) <= 0.001f)
        direction = camera.front;
    result.direction = glm::normalize(direction);
    return result;
}

void logAimDebug(const char* label, const Camera& camera, const AimSolution& aim)
{
    Debug::warn(Debug::Category::Weapons,
        "[AIM] %s\n"
        "Aim Mode: %s\n"
        "Camera Position: (%.2f, %.2f, %.2f)\n"
        "Camera Forward: (%.4f, %.4f, %.4f)\n"
        "Camera Hit: %s at distance %.2f\n"
        "Camera Hit Position: (%.2f, %.2f, %.2f)\n"
        "Computed Target Point: (%.2f, %.2f, %.2f)\n"
        "Muzzle Position: (%.2f, %.2f, %.2f)\n"
        "Computed Direction: (%.4f, %.4f, %.4f)\n",
        label,
        aim.modeName,
        camera.pos.x, camera.pos.y, camera.pos.z,
        camera.front.x, camera.front.y, camera.front.z,
        aimHitKindName(aim.cameraHitKind), aim.cameraDistance,
        aim.aimPoint.x, aim.aimPoint.y, aim.aimPoint.z,
        aim.aimPoint.x, aim.aimPoint.y, aim.aimPoint.z,
        aim.origin.x, aim.origin.y, aim.origin.z,
        aim.direction.x, aim.direction.y, aim.direction.z);
}

AimTarget computeAimTarget(
    const Camera& camera,
    const World& world,
    NpcSystem& npcs,
    const std::unordered_map<uint32_t, Player>* remotePlayers)
{
    float cameraNearest = kMaxShotDistance;
    AimHitKind hitKind = AimHitKind::None;

    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        float d = 0.0f;
        if (rayTriangle(camera.pos, camera.front, tri, d) && d < cameraNearest) {
            cameraNearest = d;
            hitKind = AimHitKind::World;
        }
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
            if (rayAabb(camera.pos, camera.front, center - half, center + half, d, nml) && d < cameraNearest) {
                cameraNearest = d;
                hitKind = AimHitKind::Npc;
            }
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
            if (rayAabb(camera.pos, camera.front, mn, mx, d, nml) && d < cameraNearest) {
                cameraNearest = d;
                hitKind = AimHitKind::RemotePlayer;
            }
        }
    }

    AimTarget result;
    result.worldPoint = camera.pos + camera.front * cameraNearest;
    result.cameraDistance = cameraNearest;
    result.hitKind = hitKind;
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
