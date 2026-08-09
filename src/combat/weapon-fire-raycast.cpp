// 07 31 2026, 15 38
/* purpose
* Computes weapon aim solutions and performs hitscan ray/AABB collision.
* Owns aim-mode behavior (crosshair, camforward, farpoint) and shared beam collision.
* Does NOT apply damage, spawn hit effects, or render tracers.
* Does NOT own server weapon authority, packet send/receive, or damage validation.
*/
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
#include "network/server.h"
#include "physics/movement/physics-collision.h"

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
        case AimHitKind::RemoteNpc: return "remote_npc";
        case AimHitKind::None:
        default: return "none";
    }
}

AimSolution computeAim(
    const Camera& camera,
    const World& world,
    NpcSystem& npcs,
    const glm::vec3& muzzlePos,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    std::unordered_map<uint32_t, Player>* remoteNpcs)
{
    AimSolution result;
    result.origin = muzzlePos;
    result.cameraDistance = kMaxShotDistance;

    const GameplayConfig& cfg = GameplayConfig::instance();
    const GameplayAimMode mode = cfg.aimMode();
    if (mode == GameplayAimMode::Crosshair) {
        AimTarget target = computeAimTarget(camera, world, npcs, remotePlayers, remoteNpcs);
        result.aimPoint = target.worldPoint;
        result.cameraDistance = target.cameraDistance;
        result.modeName = "crosshair";
        result.cameraHitKind = target.hitKind;
        result.usesCameraTarget = true;
    } else if (mode == GameplayAimMode::Farpoint) {
        float farDist = cfg.farpointDistance();
        result.aimPoint = camera.pos + camera.front * farDist;
        result.cameraDistance = farDist;
        result.modeName = "farpoint";
        result.cameraHitKind = AimHitKind::None;
        result.usesCameraTarget = false;
    } else if (mode == GameplayAimMode::CamForward) {
        // Beam travels exactly along camera forward from the muzzle, parallel to
        // the camera. It never aims at the crosshair/world point; world and entity
        // collision still run in collideBeam (usesCameraTarget = false).
        result.aimPoint = camera.pos + camera.front * kMaxShotDistance;
        result.cameraDistance = kMaxShotDistance;
        result.modeName = "camforward";
        result.cameraHitKind = AimHitKind::None;
        result.usesCameraTarget = false;
    } else {
        result.aimPoint = camera.pos + camera.front * kMaxShotDistance;
        result.modeName = "world_hit";
        result.cameraHitKind = AimHitKind::None;
        result.usesCameraTarget = false;
    }

    if (mode == GameplayAimMode::CamForward) {
        // 1:1 parallel to the camera look direction, full 3D, never aimed at a point.
        result.direction = camera.front;
    } else {
        glm::vec3 direction = result.aimPoint - muzzlePos;
        if (glm::length(direction) <= 0.001f)
            direction = camera.front;
        result.direction = glm::normalize(direction);
    }
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
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    std::unordered_map<uint32_t, Player>* remoteNpcs)
{
    float cameraNearest = kMaxShotDistance;
    AimHitKind hitKind = AimHitKind::None;
    glm::vec3 worldNormal{0.0f, 0.0f, 1.0f};

    {
        float hitDist = 0.0f;
        glm::vec3 hitNml;
        if (rayTraverseGridCells(world, camera.pos, camera.front, cameraNearest, hitDist, &hitNml) && hitDist < cameraNearest) {
            cameraNearest = hitDist;
            hitKind = AimHitKind::World;
            worldNormal = hitNml;
        }
    }
    for (Npc& npc : npcs.all()) {
        if (npc.body.currentHp <= 0) continue;
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
            // Aim at the real body parts only — never an invisible capsule.
            if (remote.physicalBody.parts.empty())
                continue;
            const_cast<Player&>(remote).updateModelWorldTransforms();
            for (const PhysicalBodyPart& part : remote.physicalBody.parts) {
                glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                float d = 0.0f;
                glm::vec3 nml;
                if (rayAabb(camera.pos, camera.front, center - half, center + half, d, nml) && d < cameraNearest) {
                    cameraNearest = d;
                    hitKind = AimHitKind::RemotePlayer;
                }
            }
        }
    }
    if (remoteNpcs) {
        for (auto& entry : *remoteNpcs) {
            Player& remote = entry.second;
            if (remote.dead || remote.currentHp <= 0)
                continue;
            // Remote NPC replicas are rendered at their interpolated pose, so
            // aim at the same body-part representation the beam uses.
            if (remote.physicalBody.parts.empty())
                continue;
            remote.updateModelWorldTransforms();
            for (const PhysicalBodyPart& part : remote.physicalBody.parts) {
                glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                float d = 0.0f;
                glm::vec3 nml;
                if (rayAabb(camera.pos, camera.front, center - half, center + half, d, nml) && d < cameraNearest) {
                    cameraNearest = d;
                    hitKind = AimHitKind::RemoteNpc;
                }
            }
        }
    }

    AimTarget result;
    result.worldPoint = camera.pos + camera.front * cameraNearest;
    result.cameraDistance = cameraNearest;
    result.hitKind = hitKind;
    result.worldNormal = worldNormal;
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

static bool sweptSphereAabb(
    const glm::vec3& origin, const glm::vec3& direction, float radius,
    const glm::vec3& mn, const glm::vec3& mx, float maxDist,
    float& hitDist, glm::vec3& hitNormal)
{
    glm::vec3 emn = mn - glm::vec3(radius);
    glm::vec3 emx = mx + glm::vec3(radius);
    return rayAabb(origin, direction, emn, emx, hitDist, hitNormal) && hitDist <= maxDist;
}

// =====================================================
// Shared hitscan collision (used by all hitscan paths)
// =====================================================

BeamCollisionResult collideBeam(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    float beamThickness,
    const World& world,
    NpcSystem* npcs,
    const std::unordered_map<uint32_t, Player>* remotePlayers,
    const Player* targetPlayer,
    bool skipWorldCollision,
    std::unordered_map<uint32_t, Player>* remoteNpcs,
    float beamWorldThickness)
{
    BeamCollisionResult result;
    result.nearest = maxDistance;
    result.hitWorld = false;
    result.worldNormal = -direction;
    result.victim = nullptr;
    result.remoteVictim = nullptr;
    result.remoteTargetId = 0;
    result.remoteNpcVictim = nullptr;
    result.remoteNpcTargetId = 0;
    result.hitPart.clear();
    result.hitPosition = origin + direction * maxDistance;
    result.sweepCenterPosition = result.hitPosition;

    bool useSphereCast = (beamThickness > 0.0f);

    if (!skipWorldCollision) {
        // World/occlusion radius is separate from the entity-hit radius so a
        // thick beam never clips wall edges and breaks the pellet pattern.
        if (beamWorldThickness > 0.0f) {
            glm::vec3 hNml(0.0f);
            float hDist = 0.0f;
            glm::vec3 hPt(0.0f);
            if (sweptSphereTraverseGridCells(world, origin, direction, maxDistance,
                                              beamWorldThickness, hDist, hNml, hPt) && hDist < result.nearest) {
                result.nearest = hDist;
                result.hitWorld = true;
                result.worldNormal = hNml;
                result.sweepCenterPosition = origin + direction * hDist;
                result.hitPosition = hPt;
            }
        } else {
            float hDist = 0.0f;
            glm::vec3 hNml(0.0f);
            if (rayTraverseGridCells(world, origin, direction, result.nearest, hDist, &hNml) && hDist < result.nearest) {
                result.nearest = hDist;
                result.hitWorld = true;
                result.worldNormal = hNml;
                glm::vec3 absolutePoint = origin + direction * hDist;
                result.sweepCenterPosition = absolutePoint;
                result.hitPosition = absolutePoint;
            }
        }
    }

    if (npcs) {
        for (Npc& npc : npcs->all()) {
            if (npc.body.currentHp <= 0) continue;
            npc.body.updateModelWorldTransforms();
            for (const PhysicalBodyPart& part : npc.body.physicalBody.parts) {
                glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                float d = 0.0f;
                glm::vec3 nml;
                bool h = false;
                if (useSphereCast) {
                    h = sweptSphereAabb(origin, direction, beamThickness, center - half, center + half, result.nearest, d, nml);
                } else {
                    h = rayAabb(origin, direction, center - half, center + half, d, nml);
                }
                if (h && d < result.nearest) {
                    result.nearest = d;
                    result.hitWorld = false;
                    result.victim = &npc;
                    result.remoteVictim = nullptr;
                    result.remoteTargetId = 0;
                    result.remoteNpcVictim = nullptr;
                    result.remoteNpcTargetId = 0;
                    result.hitPart = part.name;
                    result.hitNormal = nml;
                    glm::vec3 sweepCenter = origin + direction * d;
                    result.sweepCenterPosition = sweepCenter;
                    result.hitPosition = useSphereCast
                        ? sweepCenter - nml * beamThickness
                        : sweepCenter;
                    result.localHeight = std::clamp((sweepCenter.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
                }
            }
        }
    }

    if (remotePlayers) {
        for (const auto& entry : *remotePlayers) {
            const Player& remote = entry.second;
            if (remote.dead || remote.currentHp <= 0) continue;
            // No capsule fallback: a remote player without a loaded body model
            // is simply not hittable — only real body-part intersections count.
            if (remote.physicalBody.parts.empty()) continue;
            const_cast<Player&>(remote).updateModelWorldTransforms();
            for (const PhysicalBodyPart& part : remote.physicalBody.parts) {
                glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                float d = 0.0f;
                glm::vec3 nml;
                bool h = false;
                if (useSphereCast) {
                    h = sweptSphereAabb(origin, direction, beamThickness, center - half, center + half, result.nearest, d, nml);
                } else {
                    h = rayAabb(origin, direction, center - half, center + half, d, nml);
                }
                if (h && d < result.nearest) {
                    result.nearest = d;
                    result.hitWorld = false;
                    result.victim = nullptr;
                    // The replica is mutated after collideBeam returns
                    // (predicted death); the map owns mutable Player values
                    // despite the const trace facade.
                    result.remoteVictim = const_cast<Player*>(&remote);
                    result.remoteTargetId = entry.first;
                    result.remoteNpcVictim = nullptr;
                    result.remoteNpcTargetId = 0;
                    result.hitPart = part.name;
                    result.hitNormal = nml;
                    glm::vec3 sweepCenter = origin + direction * d;
                    result.sweepCenterPosition = sweepCenter;
                    result.hitPosition = useSphereCast
                        ? sweepCenter - nml * beamThickness
                        : sweepCenter;
                    result.localHeight = std::clamp(
                        (sweepCenter.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
                }
            }
        }
    }

    if (remoteNpcs) {
        for (auto& entry : *remoteNpcs) {
            Player& remote = entry.second;
            if (remote.dead || remote.currentHp <= 0) continue;
            // No capsule fallback — only real body-part intersections count.
            if (remote.physicalBody.parts.empty())
            {
                Debug::logThrottled(Debug::Category::NpcCombat,
                    "remote-npc-no-parts", 1.0f,
                    "[REMOTE NPC TRACE] entityId=%u SKIPPED parts=empty pos=(%.2f,%.2f,%.2f) "
                    "hp=%d dead=%d\n",
                    entry.first, remote.pos.x, remote.pos.y, remote.pos.z,
                    remote.currentHp, (int)remote.dead);
                continue;
            }
            remote.updateModelWorldTransforms();
            for (const PhysicalBodyPart& part : remote.physicalBody.parts) {
                glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
                glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
                glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
                float d = 0.0f;
                glm::vec3 nml;
                bool h = false;
                if (useSphereCast) {
                    h = sweptSphereAabb(origin, direction, beamThickness, center - half, center + half, result.nearest, d, nml);
                } else {
                    h = rayAabb(origin, direction, center - half, center + half, d, nml);
                }
                if (h && d < result.nearest) {
                    result.nearest = d;
                    result.hitWorld = false;
                    result.victim = nullptr;
                    result.remoteVictim = nullptr;
                    result.remoteTargetId = 0;
                    result.remoteNpcVictim = &remote;
                    result.remoteNpcTargetId = entry.first;
                    result.hitPart = part.name;
                    result.hitNormal = nml;
                    glm::vec3 sweepCenter = origin + direction * d;
                    result.sweepCenterPosition = sweepCenter;
                    result.hitPosition = useSphereCast
                        ? sweepCenter - nml * beamThickness
                        : sweepCenter;
                    result.localHeight = std::clamp(
                        (sweepCenter.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
                }
            }
        }
    }

    if (targetPlayer && !targetPlayer->dead && targetPlayer->currentHp > 0) {
        // No capsule fallback — only real body-part intersections count.
        // The server's mirror player has no loaded model, so reconstruct its
        // real body-part boxes from the standard template at (pos, yaw).
        if (targetPlayer->physicalBody.parts.empty())
        {
            if (const auto* tpl = MimitaNet::standardPlayerBodyTemplate())
            {
                const float yaw = targetPlayer->yaw;
                const float c = std::cos(yaw);
                const float s = std::sin(yaw);
                for (const auto& t : *tpl)
                {
                    const glm::vec3 off(t.offset.x * c - t.offset.y * s,
                                        t.offset.x * s + t.offset.y * c,
                                        t.offset.z);
                    const glm::vec3 center = targetPlayer->pos + off;
                    const glm::vec3 half = t.half;
                    float d = 0.0f;
                    glm::vec3 nml;
                    bool h = false;
                    if (useSphereCast) {
                        h = sweptSphereAabb(origin, direction, beamThickness,
                                            center - half, center + half, result.nearest, d, nml);
                    } else {
                        h = rayAabb(origin, direction, center - half, center + half, d, nml);
                    }
                    if (h && d < result.nearest) {
                        result.nearest = d;
                        result.hitWorld = false;
                        result.victim = nullptr;
                        result.remoteVictim = nullptr;
                        result.hitPart = t.bodyPart == 1 ? "head"
                            : (t.bodyPart == 2 ? "leg" : "torso");
                        result.hitNormal = nml;
                        glm::vec3 sweepCenter = origin + direction * d;
                        result.sweepCenterPosition = sweepCenter;
                        result.hitPosition = useSphereCast
                            ? sweepCenter - nml * beamThickness
                            : sweepCenter;
                        result.localHeight = std::clamp(
                            (sweepCenter.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
                    }
                }
            }
            return result;
        }
        const_cast<Player*>(targetPlayer)->updateModelWorldTransforms();
        for (const PhysicalBodyPart& part : targetPlayer->physicalBody.parts) {
            glm::vec3 localCenter = (part.collider.localMin + part.collider.localMax) * 0.5f;
            glm::vec3 center = glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            glm::vec3 half = glm::max((part.collider.localMax - part.collider.localMin) * 0.5f, glm::vec3(0.12f));
            float d = 0.0f;
            glm::vec3 nml;
            bool h = false;
            if (useSphereCast) {
                h = sweptSphereAabb(origin, direction, beamThickness, center - half, center + half, result.nearest, d, nml);
            } else {
                h = rayAabb(origin, direction, center - half, center + half, d, nml);
            }
            if (h && d < result.nearest) {
                result.nearest = d;
                result.hitWorld = false;
                result.victim = nullptr;
                result.remoteVictim = nullptr;
                result.hitPart = part.name;
                result.hitNormal = nml;
                glm::vec3 sweepCenter = origin + direction * d;
                result.sweepCenterPosition = sweepCenter;
                result.hitPosition = useSphereCast
                    ? sweepCenter - nml * beamThickness
                    : sweepCenter;
                result.localHeight = std::clamp(
                    (sweepCenter.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
            }
        }
    }

    return result;
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
