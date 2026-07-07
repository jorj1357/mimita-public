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

    {
        glm::vec3 aimEnd = camera.pos + camera.front * cameraNearest;
        AABB aimBounds;
        aimBounds.min = glm::min(camera.pos, aimEnd) - glm::vec3(0.1f);
        aimBounds.max = glm::max(camera.pos, aimEnd) + glm::vec3(0.1f);
        std::vector<int> candidates;
        appendChunkTrianglesForAABB(world, aimBounds, 0.1f, candidates);
        for (int triIndex : candidates) {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            float d = 0.0f;
            if (rayTriangle(camera.pos, camera.front, tri, d) && d < cameraNearest) {
                cameraNearest = d;
                hitKind = AimHitKind::World;
            }
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

// =====================================================
// Swept sphere helpers (for beamThickness > 0)
// =====================================================

static bool sweptSpherePoint(
    const glm::vec3& origin, const glm::vec3& direction,
    float radius, const glm::vec3& point,
    float maxDist, float& hitDist, glm::vec3& hitNormal)
{
    glm::vec3 rel = origin - point;
    float a = glm::dot(direction, direction);
    float b = 2.0f * glm::dot(rel, direction);
    float c = glm::dot(rel, rel) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;
    float sqrtDisc = sqrtf(disc);
    float t0 = (-b - sqrtDisc) / (2.0f * a);
    float t1 = (-b + sqrtDisc) / (2.0f * a);
    float tHit = (t0 >= 0.0f) ? t0 : t1;
    if (tHit < 0.0f || tHit > maxDist) return false;
    hitDist = tHit;
    glm::vec3 centerAtT = origin + direction * tHit;
    glm::vec3 n = centerAtT - point;
    float nLen = glm::length(n);
    if (nLen < 0.000001f)
        n = -direction;
    else
        n /= nLen;
    hitNormal = n;
    return true;
}

static bool sweptSphereEdge(
    const glm::vec3& origin, const glm::vec3& direction, float radius,
    const glm::vec3& edgeA, const glm::vec3& edgeB, float maxDist,
    float& hitDist, glm::vec3& hitNormal, glm::vec3& hitPoint)
{
    glm::vec3 edgeDir = edgeB - edgeA;
    float edgeLen = glm::length(edgeDir);
    if (edgeLen < 0.000001f) return false;
    edgeDir /= edgeLen;

    glm::vec3 rel = origin - edgeA;
    float proj = glm::dot(rel, edgeDir);
    glm::vec3 relPerp = rel - edgeDir * proj;
    glm::vec3 movePerp = direction - edgeDir * glm::dot(direction, edgeDir);

    float a = glm::dot(movePerp, movePerp);
    if (a < 0.0000001f) return false;

    float b = 2.0f * glm::dot(relPerp, movePerp);
    float c = glm::dot(relPerp, relPerp) - radius * radius;
    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return false;

    float tHit = (-b - sqrtf(disc)) / (2.0f * a);
    if (tHit < 0.0f || tHit > maxDist) return false;

    glm::vec3 centerAtT = origin + direction * tHit;
    glm::vec3 relAtT = centerAtT - edgeA;
    float projAtT = glm::dot(relAtT, edgeDir);
    if (projAtT < 0.0f || projAtT > edgeLen) return false;

    glm::vec3 closestOnEdge = edgeA + edgeDir * projAtT;
    glm::vec3 n = centerAtT - closestOnEdge;
    float nLen = glm::length(n);
    if (nLen < 0.000001f) return false;
    n /= nLen;

    hitDist = tHit;
    hitNormal = n;
    hitPoint = closestOnEdge;
    return true;
}

static bool pointInTri(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    glm::vec3 v0 = c - a;
    glm::vec3 v1 = b - a;
    glm::vec3 v2 = p - a;
    float dot00 = glm::dot(v0, v0);
    float dot01 = glm::dot(v0, v1);
    float dot02 = glm::dot(v0, v2);
    float dot11 = glm::dot(v1, v1);
    float dot12 = glm::dot(v1, v2);
    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    return (u >= -0.000001f) && (v >= -0.000001f) && (u + v <= 1.0f + 0.000001f);
}

static bool sweptSphereTriangle(
    const glm::vec3& origin, const glm::vec3& direction, float radius,
    const CollisionTriangle& tri, float maxDist,
    float& hitDist, glm::vec3& hitNormal, glm::vec3& hitPoint)
{
    float bestT = maxDist;
    glm::vec3 bestN(0.0f);
    glm::vec3 bestP(0.0f);
    bool hit = false;

    glm::vec3 n = tri.normal;
    float dist = glm::dot(origin - tri.a, n);
    if (dist < 0.0f) {
        n = -n;
        dist = -dist;
    }

    float denom = glm::dot(direction, n);
    if (denom < -0.000001f) {
        float t = (radius - dist) / denom;
        if (t >= 0.0f && t < bestT) {
            glm::vec3 centerAtT = origin + direction * t;
            glm::vec3 planePoint = centerAtT - n * radius;
            if (pointInTri(planePoint, tri.a, tri.b, tri.c)) {
                bestT = t;
                bestN = n;
                bestP = planePoint;
                hit = true;
            }
        }
    }

    glm::vec3 edgePairs[3][2] = {{tri.a, tri.b}, {tri.b, tri.c}, {tri.c, tri.a}};
    for (auto& ep : edgePairs) {
        float t = maxDist;
        glm::vec3 en(0.0f);
        glm::vec3 epPt(0.0f);
        if (sweptSphereEdge(origin, direction, radius, ep[0], ep[1], maxDist, t, en, epPt) && t < bestT) {
            bestT = t;
            bestN = en;
            bestP = epPt;
            hit = true;
        }
    }

    glm::vec3 verts[3] = {tri.a, tri.b, tri.c};
    for (auto& v : verts) {
        float t = maxDist;
        glm::vec3 vn(0.0f);
        if (sweptSpherePoint(origin, direction, radius, v, maxDist, t, vn) && t < bestT) {
            bestT = t;
            bestN = vn;
            bestP = v;
            hit = true;
        }
    }

    if (!hit) return false;
    hitDist = bestT;
    hitNormal = bestN;
    hitPoint = bestP;
    return true;
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
    const Player* targetPlayer)
{
    BeamCollisionResult result;
    result.nearest = maxDistance;
    result.hitWorld = false;
    result.worldNormal = -direction;
    result.victim = nullptr;
    result.remoteVictim = nullptr;
    result.remoteTargetId = 0;
    result.hitPart.clear();

    bool useSphereCast = (beamThickness > 0.0f);

    {
        glm::vec3 rayEnd = origin + direction * maxDistance;
        AABB rayBounds;
        rayBounds.min = glm::min(origin, rayEnd);
        rayBounds.max = glm::max(origin, rayEnd);
        float expansion = useSphereCast ? (beamThickness + 0.1f) : 0.1f;
        rayBounds.min -= glm::vec3(expansion);
        rayBounds.max += glm::vec3(expansion);
        std::vector<int> candidates;
        appendChunkTrianglesForAABB(world, rayBounds, expansion, candidates);
        for (int triIndex : candidates) {
            const CollisionTriangle& tri = world.collisionMesh.triangles[triIndex];
            if (useSphereCast) {
                float d = 0.0f;
                glm::vec3 n, p;
                if (sweptSphereTriangle(origin, direction, beamThickness, tri, maxDistance, d, n, p) && d < result.nearest) {
                    result.nearest = d;
                    result.hitWorld = true;
                    result.worldNormal = tri.normal;
                }
            } else {
                float d = 0.0f;
                if (rayTriangle(origin, direction, tri, d) && d < result.nearest) {
                    result.nearest = d;
                    result.hitWorld = true;
                    result.worldNormal = tri.normal;
                }
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
                    result.hitPart = part.name;
                    result.hitNormal = nml;
                    glm::vec3 hit = origin + direction * d;
                    result.localHeight = std::clamp((hit.z - (center.z - half.z)) / (half.z * 2.0f), 0.0f, 1.0f);
                }
            }
        }
    }

    if (remotePlayers) {
        for (const auto& entry : *remotePlayers) {
            const Player& remote = entry.second;
            if (remote.dead || remote.currentHp <= 0) continue;
            const Capsule capsule = remote.getCapsule();
            const glm::vec3 mn(
                remote.pos.x - capsule.r,
                remote.pos.y - capsule.r,
                capsule.a.z - capsule.r);
            const glm::vec3 mx(
                remote.pos.x + capsule.r,
                remote.pos.y + capsule.r,
                capsule.b.z + capsule.r);
            float d = 0.0f;
            glm::vec3 nml;
            bool h = false;
            if (useSphereCast) {
                h = sweptSphereAabb(origin, direction, beamThickness, mn, mx, result.nearest, d, nml);
            } else {
                h = rayAabb(origin, direction, mn, mx, d, nml);
            }
            if (h && d < result.nearest) {
                result.nearest = d;
                result.hitWorld = false;
                result.victim = nullptr;
                result.remoteVictim = &remote;
                result.remoteTargetId = entry.first;
                result.hitNormal = nml;
                glm::vec3 hit = origin + direction * d;
                result.localHeight = std::clamp(
                    (hit.z - mn.z) / std::max(mx.z - mn.z, 0.001f), 0.0f, 1.0f);
                result.hitPart = result.localHeight > 0.78f ? "head" :
                    result.localHeight > 0.32f ? "torso" : "leg";
            }
        }
    }

    if (targetPlayer && !targetPlayer->dead && targetPlayer->currentHp > 0) {
        Capsule cap = targetPlayer->getCapsule();
        glm::vec3 mn(cap.a.x - cap.r, cap.a.y - cap.r, cap.a.z - cap.r);
        glm::vec3 mx(cap.b.x + cap.r, cap.b.y + cap.r, cap.b.z + cap.r);
        float d = 0.0f;
        glm::vec3 nml;
        bool h = false;
        if (useSphereCast) {
            h = sweptSphereAabb(origin, direction, beamThickness, mn, mx, result.nearest, d, nml);
        } else {
            h = rayAabb(origin, direction, mn, mx, d, nml);
        }
        if (h && d < result.nearest) {
            result.nearest = d;
            result.hitWorld = false;
            result.victim = nullptr;
            result.remoteVictim = nullptr;
            result.hitNormal = nml;
            glm::vec3 hit = origin + direction * d;
            result.localHeight = std::clamp(
                (hit.z - mn.z) / std::max(mx.z - mn.z, 0.001f), 0.0f, 1.0f);
            result.hitPart = result.localHeight > 0.78f ? "head" :
                result.localHeight > 0.68f ? "torso" :
                result.localHeight > 0.32f ? "torso" : "leg";
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
