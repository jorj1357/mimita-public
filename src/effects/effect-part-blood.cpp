#include "effect-part.h"
#include "world/world.h"
#include "audio/audio.h"
#include "effects/hit-effects.h"
#include "debug/debug-log.h"
#include "config.h"
#include "replay/replay.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>

namespace {

struct BloodWorldHit {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    const char* surfaceType = "none";
    float distance = 0.0f;
};

bool rayTriangleSegment(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const CollisionTriangle& tri,
    float& distance)
{
    const glm::vec3 e1 = tri.b - tri.a;
    const glm::vec3 e2 = tri.c - tri.a;
    const glm::vec3 p = glm::cross(direction, e2);
    const float determinant = glm::dot(e1, p);
    if (std::fabs(determinant) < 0.000001f)
        return false;

    const float inverseDeterminant = 1.0f / determinant;
    const glm::vec3 offset = origin - tri.a;
    const float u = glm::dot(offset, p) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f)
        return false;

    const glm::vec3 q = glm::cross(offset, e1);
    const float v = glm::dot(direction, q) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f)
        return false;

    distance = glm::dot(e2, q) * inverseDeterminant;
    return distance >= 0.0f && distance <= maxDistance;
}

bool rayAabbSegment(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const glm::vec3& minimum,
    const glm::vec3& maximum,
    float& distance,
    glm::vec3& normal)
{
    float minimumTime = 0.0f;
    float maximumTime = maxDistance;
    normal = glm::vec3(0.0f);

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) < 0.000001f) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                return false;
            continue;
        }

        const float inverseDirection = 1.0f / direction[axis];
        float nearTime = (minimum[axis] - origin[axis]) * inverseDirection;
        float farTime = (maximum[axis] - origin[axis]) * inverseDirection;
        float normalSign = -1.0f;
        if (nearTime > farTime) {
            std::swap(nearTime, farTime);
            normalSign = 1.0f;
        }
        if (nearTime > minimumTime) {
            minimumTime = nearTime;
            normal = glm::vec3(0.0f);
            normal[axis] = normalSign;
        }
        maximumTime = std::min(maximumTime, farTime);
        if (minimumTime > maximumTime)
            return false;
    }

    distance = minimumTime;
    return distance >= 0.0f && distance <= maxDistance;
}

bool raySphereSegment(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const Sphere& sphere,
    float& distance,
    glm::vec3& normal)
{
    const glm::vec3 offset = origin - sphere.pos;
    const float b = glm::dot(offset, direction);
    const float c = glm::dot(offset, offset) - sphere.radius * sphere.radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f)
        return false;

    distance = -b - std::sqrt(discriminant);
    if (distance < 0.0f || distance > maxDistance)
        return false;
    normal = glm::normalize(origin + direction * distance - sphere.pos);
    return true;
}

bool traceBloodSegment(
    const World& world,
    const glm::vec3& from,
    const glm::vec3& to,
    BloodWorldHit& hit)
{
    const glm::vec3 delta = to - from;
    const float segmentLength = glm::length(delta);
    if (segmentLength < 0.0001f)
        return false;

    const glm::vec3 direction = delta / segmentLength;
    float nearest = segmentLength;
    bool found = false;

    for (const CollisionTriangle& triangle : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (!rayTriangleSegment(from, direction, nearest, triangle, distance))
            continue;
        nearest = distance;
        hit.normal = triangle.normal;
        hit.surfaceType = "triangle";
        found = true;
    }

    for (const Block& block : world.blocks) {
        float distance = 0.0f;
        glm::vec3 normal(0.0f);
        const glm::vec3 halfSize = block.size * 0.5f;
        if (!rayAabbSegment(
                from, direction, nearest,
                block.pos - halfSize, block.pos + halfSize,
                distance, normal))
            continue;
        nearest = distance;
        hit.normal = normal;
        hit.surfaceType = "block";
        found = true;
    }

    for (const Sphere& sphere : world.spheres) {
        float distance = 0.0f;
        glm::vec3 normal(0.0f);
        if (!raySphereSegment(from, direction, nearest, sphere, distance, normal))
            continue;
        nearest = distance;
        hit.normal = normal;
        hit.surfaceType = "sphere";
        found = true;
    }

    if (!found)
        return false;

    hit.position = from + direction * nearest;
    hit.distance = nearest;
    if (glm::dot(hit.normal, direction) > 0.0f)
        hit.normal = -hit.normal;
    hit.normal = glm::normalize(hit.normal);
    return true;
}

}

void EffectPartSystem::spawnBloodEffect(
    glm::vec3 hitPoint,
    glm::vec3 sprayDirection,
    float damage,
    const std::string& sourceActorId,
    const std::string& targetActorId)
{
    if (!isBloodFXEnabled()) return;
    damage = std::max(0.0f, damage);
    const glm::vec3 forward = glm::length(sprayDirection) > 0.001f
        ? glm::normalize(sprayDirection)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 tangent = glm::normalize(
        std::fabs(forward.z) < 0.9f
            ? glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f))
            : glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 bitangent = glm::normalize(glm::cross(forward, tangent));
    const float damageScale = std::clamp(damage / 100.0f, 0.0f, 2.0f);

    int particleCount = 12;
    if (damage < 20.0f) particleCount = 12 + (int)(damage * 0.8f);
    else if (damage < 50.0f) particleCount = 28 + (int)((damage - 20.0f) * 1.2f);
    else particleCount = 64 + (int)((damage - 50.0f) * 0.7f);
    particleCount = std::clamp(particleCount, 12, 110);

    const float bloodConeDegrees = 15.0f + damageScale * 5.0f;
    const float bloodConeRadius = std::tan(glm::radians(bloodConeDegrees));

    const float debrisConeDegrees = 35.0f + damageScale * 25.0f;
    const float debrisConeRadius = std::tan(glm::radians(debrisConeDegrees));

    const float baseSpeed = 6.0f + damageScale * 10.0f;
    const float speedVariation = 4.0f;

    const float baseLifetime = 2.5f + damageScale * 1.0f;

    if (mBloodParticles.size() + (size_t)particleCount > MAX_BLOOD_PARTICLES) {
        const size_t removeCount =
            mBloodParticles.size() + (size_t)particleCount - MAX_BLOOD_PARTICLES;
        mBloodParticles.erase(
            mBloodParticles.begin(),
            mBloodParticles.begin() + (std::min)(removeCount, mBloodParticles.size()));
    }

    const int bloodCount = (particleCount * 2) / 3;
    const int debrisCount = particleCount - bloodCount;

    for (int i = 0; i < bloodCount; ++i) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float radial = std::sqrt((float)(rand() % 1001) / 1000.0f) * bloodConeRadius;
        const glm::vec3 direction = glm::normalize(
            forward +
            tangent * std::cos(angle) * radial +
            bitangent * std::sin(angle) * radial);

        const float centerBias = 1.0f - radial / std::max(bloodConeRadius, 0.001f);
        const float speed = (baseSpeed * (0.4f + centerBias * 0.6f)) +
            (float)(rand() % (int)(speedVariation * 1000.0f + 1.0f)) / 1000.0f;

        BloodParticle particle;
        particle.position = hitPoint + direction * 0.05f;
        particle.velocity = direction * speed;
        particle.size = 0.02f +
            (float)(rand() % 601) / 20000.0f +
            damageScale * 0.015f;
        particle.lifetime = baseLifetime + (float)(rand() % 1001) / 1000.0f;
        particle.alpha = 0.85f;
        particle.rotation = (float)(rand() % 6284) / 1000.0f;
        particle.stretch = 0.7f + (float)(rand() % 601) / 1000.0f;
        mBloodParticles.push_back(particle);
    }

    for (int i = 0; i < debrisCount; ++i) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float radial = std::sqrt((float)(rand() % 1001) / 1000.0f) * debrisConeRadius;
        const glm::vec3 direction = glm::normalize(
            forward +
            tangent * std::cos(angle) * radial +
            bitangent * std::sin(angle) * radial);

        const float speed = 2.0f + (float)(rand() % 2001) / 1000.0f;

        EffectPart deb;
        deb.position = hitPoint + direction * 0.05f;
        deb.velocity = direction * speed;
        deb.velocity.z += 1.0f + (float)(rand() % 1001) / 1000.0f;
        deb.halfSize = glm::vec3(0.02f + (float)(rand() % 301) / 10000.0f);
        deb.color = glm::vec3(0.35f, 0.3f, 0.25f);
        deb.alpha = 0.7f;
        deb.maxLifetime = 1.0f + (float)(rand() % 1001) / 1000.0f;
        deb.rotation = glm::vec3(
            (float)(rand() % 6284) / 1000.0f,
            (float)(rand() % 6284) / 1000.0f,
            (float)(rand() % 6284) / 1000.0f);
        deb.angularVelocity = glm::vec3(
            (float)(rand() % 628) / 100.0f,
            (float)(rand() % 628) / 100.0f,
            (float)(rand() % 628) / 100.0f);
        deb.box = true;
        deb.gravity = 3.0f;
        deb.affectedByGravity = true;
        deb.billboardText = false;
        deb.replayType = "debris";
        spawn(deb);
    }

    if (DebugConfig::DEBUG_BLOOD_HITS) {
        printf("[BLOOD] blood=%d debris=%d speed=%.2f bloodCone=%.0f debrisCone=%.0f\n",
               bloodCount, debrisCount, baseSpeed, bloodConeDegrees, debrisConeDegrees);
    }

    ReplayEffectEvent emitter;
    emitter.type = "blood_spurt_emitter";
    emitter.position = hitPoint;
    emitter.direction = forward;
    emitter.lifetime = 0.6f;
    emitter.color = glm::vec4(0.95f, 0.02f, 0.04f, 1.0f);
    emitter.sourceActorId = sourceActorId;
    emitter.targetActorId = targetActorId;
    captureReplayEffect(emitter);

    if (!mWorld) {
        if (DebugConfig::DEBUG_BLOOD_HITS)
            printf("[BLOOD DECAL] skipped world=null\n");
        return;
    }

    const int decalCount = std::clamp(8 + (int)std::round(damage * 0.3f), 8, 40);
    const float coneDist = std::clamp(3.0f + damage * 0.08f, 3.0f, 16.0f);
    mBloodDebugSegmentCount = 0;

    for (int dec = 0; dec < decalCount; ++dec) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float radial = std::sqrt((float)(rand() % 1001) / 1000.0f) * debrisConeRadius;
        const float dist = 0.5f + ((float)(rand() % 1001) / 1000.0f) * (coneDist - 0.5f);
        const glm::vec3 coneDir = glm::normalize(
            forward +
            tangent * std::cos(angle) * radial +
            bitangent * std::sin(angle) * radial);

        glm::vec3 conePoint = hitPoint + coneDir * dist;

        BloodWorldHit surfaceHit;
        bool foundSurface = false;

        BloodWorldHit downHit;
        float downLen = 2.0f + damageScale * 1.0f;
        if (traceBloodSegment(*mWorld, conePoint, conePoint + glm::vec3(0,0,-downLen), downHit)) {
            downHit.position += downHit.normal * 0.01f;
            surfaceHit = downHit;
            foundSurface = true;
        }

        if (!foundSurface) {
            BloodWorldHit fwdHit;
            if (traceBloodSegment(*mWorld, conePoint, conePoint + coneDir * 2.0f, fwdHit)) {
                fwdHit.position += fwdHit.normal * 0.01f;
                surfaceHit = fwdHit;
                foundSurface = true;
            }
        }

        if (!foundSurface) {
            glm::vec3 sideDir = glm::normalize(glm::cross(coneDir, glm::vec3(0,0,1)));
            BloodWorldHit sideHit;
            if (traceBloodSegment(*mWorld, conePoint, conePoint + sideDir * 2.0f, sideHit)) {
                sideHit.position += sideHit.normal * 0.01f;
                surfaceHit = sideHit;
                foundSurface = true;
            }
        }

        if (mBloodDebugSegmentCount < MAX_BLOOD_DEBUG_SEGMENTS) {
            BloodDebugSegment& debug = mBloodDebugSegments[mBloodDebugSegmentCount++];
            debug.from = conePoint;
            debug.to = foundSurface ? surfaceHit.position : conePoint;
            debug.normal = foundSurface ? surfaceHit.normal : glm::vec3(0,0,1);
            debug.hit = foundSurface;
        }

        if (!foundSurface)
            continue;

        const float variation = 0.8f + (float)(rand() % 401) / 1000.0f;
        const float impactAngle = std::clamp(1.0f - std::fabs(coneDir.z), 0.15f, 1.0f);

        BloodDecal decal;
        decal.position = surfaceHit.position;
        decal.normal = surfaceHit.normal;
        decal.radius = std::clamp(
            (0.25f + damage * 0.022f) * (0.8f + impactAngle * 0.5f) * variation,
            0.25f, 4.5f);
        decal.lifetime = 60.0f;
        decal.rotation = (float)(rand() % 6284) / 1000.0f;
        decal.stretch = 1.0f + (1.0f - impactAngle) * 0.35f;
        decal.alpha = 0.78f + (float)(rand() % 181) / 1000.0f;

        if (mBloodDecals.size() >= MAX_BLOOD_DECALS)
            mBloodDecals.erase(mBloodDecals.begin());
        mBloodDecals.push_back(decal);

        ReplayEffectEvent decalEvent;
        decalEvent.type = "blood_splatter";
        decalEvent.position = decal.position;
        decalEvent.normal = decal.normal;
        decalEvent.scale = glm::vec3(decal.radius, decal.radius * decal.stretch, 0.01f);
        decalEvent.rotation = glm::vec3(0.0f, 0.0f, decal.rotation);
        decalEvent.color = glm::vec4(0.82f, 0.015f, 0.025f, decal.alpha);
        decalEvent.lifetime = decal.lifetime;
        decalEvent.sourceActorId = sourceActorId;
        decalEvent.targetActorId = targetActorId;
        captureReplayEffect(decalEvent);
    }

    if (DebugConfig::DEBUG_BLOOD_HITS) {
        printf("[BLOOD SPAWN] particles=%d damage=%.1f bloodCone=%.1f debrisCone=%.1f\n",
               particleCount, damage, bloodConeDegrees, debrisConeDegrees);
        printf("[BLOOD DECAL] decals=%d active=%zu\n",
               decalCount, mBloodDecals.size());
    }
}
