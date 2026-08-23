// 08 17 2026, 14 20
/* purpose
* Spawns blood particles and surface decals from confirmed entity impacts.
* Owns blood spray sampling, cheap surface projection, staggering, and aging data.
* Does NOT own hit detection, damage authority, or decal rendering.
*/
#include "effect-part.h"
#include "world/world.h"
#include "audio/audio.h"
#include "effects/hit-effects.h"
#include "config/impact-decals-config.h"
#include "debug/debug-log.h"
#include "config.h"
#include "replay/replay.h"
#include "physics/movement/physics-collision.h"
#include "perf/perf-spike.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

    AABB segBounds;
    segBounds.min = glm::min(from, to);
    segBounds.max = glm::max(from, to);
    // Expand by the maximum possible block/sphere half-size to avoid false negatives
    constexpr float PAD = 2.0f;
    segBounds.min -= glm::vec3(PAD);
    segBounds.max += glm::vec3(PAD);

    static std::vector<int> sCandidates;
    sCandidates.clear();
    appendChunkTrianglesForAABB(world, segBounds, 0.1f, sCandidates, "bloodTrace");

    for (int triIdx : sCandidates) {
        const CollisionTriangle& triangle = world.collisionMesh.triangles[triIdx];
        float distance = 0.0f;
        if (!rayTriangleSegment(from, direction, nearest, triangle, distance))
            continue;
        nearest = distance;
        hit.normal = triangle.normal;
        hit.surfaceType = "triangle";
        found = true;
    }

    // Spatial culling: only test blocks whose AABB overlaps the trace segment bounds.
    // This avoids iterating blocks that are nowhere near the blood trace.
    for (const Block& block : world.blocks) {
        const glm::vec3 halfSize = block.size * 0.5f;
        const glm::vec3 bmin = block.pos - halfSize;
        const glm::vec3 bmax = block.pos + halfSize;
        // AABB overlap test between block and trace segment bounds
        if (bmax.x < segBounds.min.x || bmin.x > segBounds.max.x ||
            bmax.y < segBounds.min.y || bmin.y > segBounds.max.y ||
            bmax.z < segBounds.min.z || bmin.z > segBounds.max.z)
            continue;
        float distance = 0.0f;
        glm::vec3 normal(0.0f);
        if (!rayAabbSegment(
                from, direction, nearest,
                bmin, bmax,
                distance, normal))
            continue;
        nearest = distance;
        hit.normal = normal;
        hit.surfaceType = "block";
        found = true;
    }

    // Spatial culling: only test spheres whose bounding box overlaps the trace segment bounds.
    for (const Sphere& sphere : world.spheres) {
        const glm::vec3 smin = sphere.pos - glm::vec3(sphere.radius);
        const glm::vec3 smax = sphere.pos + glm::vec3(sphere.radius);
        if (smax.x < segBounds.min.x || smin.x > segBounds.max.x ||
            smax.y < segBounds.min.y || smin.y > segBounds.max.y ||
            smax.z < segBounds.min.z || smin.z > segBounds.max.z)
            continue;
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
    const std::string& targetActorId,
    float directness,
    float hitDistance)
{
    MIMITA_PERF_SCOPE("EffectPart::SpawnBloodEffect");
    const auto& cfg = ImpactDecalsConfig::instance().data();
    const auto& bloodCfg = cfg.blood;
    if (!cfg.enabled || !bloodCfg.enabled) return;
    damage = std::max(0.0f, damage);
    directness = std::clamp(directness, 0.0f, 1.0f);

    // Impact force (0..1): close + straight + damaging hits land hardest.
    // Distant or glancing shots fall to a minimum force.
    float force = directness;
    if (hitDistance >= 0.0f) {
        const float falloffSpan = std::max(0.01f,
            bloodCfg.force.minDistance - bloodCfg.force.maxDistance);
        const float distFactor = std::clamp(
            (bloodCfg.force.minDistance - hitDistance) / falloffSpan,
            0.0f, 1.0f);
        force *= 0.35f + 0.65f * distFactor;
    }
    const float damageFactor = std::clamp(damage / 100.0f, 0.0f, 1.0f);
    force *= 0.45f + 0.55f * damageFactor;
    force = std::clamp(force, bloodCfg.force.minForce, 1.0f);

    const glm::vec3 forward = glm::length(sprayDirection) > 0.001f
        ? glm::normalize(sprayDirection)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 tangent = glm::normalize(
        std::fabs(forward.z) < 0.9f
            ? glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f))
            : glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 bitangent = glm::normalize(glm::cross(forward, tangent));
    const float damageScale = std::clamp(damage / 100.0f, 0.0f, 2.0f);

    const auto& spray = bloodCfg.spray;
    int particleCount = spray.enabled
        ? (int)std::round(spray.minCount + (spray.maxCount - spray.minCount) * force)
        : 0;
    particleCount = std::clamp(particleCount, 0, 200);

    const float coneDegrees = spray.coneDegreesMin +
        (spray.coneDegreesMax - spray.coneDegreesMin) * force;
    const float bloodConeRadius = std::tan(glm::radians(coneDegrees));

    const float baseSpeed = spray.speedMin + (spray.speedMax - spray.speedMin) * force;
    const float speedVariation = 4.0f;

    const float baseLifetime = spray.lifetimeMin +
        (spray.lifetimeMax - spray.lifetimeMin) * force;

    if (mBloodParticles.size() + (size_t)particleCount > MAX_BLOOD_PARTICLES) {
        const size_t removeCount =
            mBloodParticles.size() + (size_t)particleCount - MAX_BLOOD_PARTICLES;
        mBloodParticles.erase(
            mBloodParticles.begin(),
            mBloodParticles.begin() + (std::min)(removeCount, mBloodParticles.size()));
    }

    const int bloodCount = (particleCount * 2) / 3;
    const int debrisCount = particleCount - bloodCount;

    const float colorJitter = bloodCfg.colorVariation;

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

        // Size mix: at high force, bigFraction of the drops are big, the rest
        // range medium -> small. Weak hits shrink everything toward sizeMin.
        float sizeT;
        const float roll = (float)(rand() % 1001) / 1000.0f;
        if (roll < spray.bigFraction) {
            sizeT = 0.6f + 0.4f * ((float)(rand() % 1001) / 1000.0f);
        } else {
            sizeT = ((float)(rand() % 1001) / 1000.0f) * 0.6f;
        }
        const float size = spray.sizeMin +
            (spray.sizeMax - spray.sizeMin) * (0.2f + 0.8f * sizeT) * force;

        const float jr = 1.0f + colorJitter * ((float)(rand() % 2001) / 1000.0f - 1.0f);
        const float jg = 1.0f + colorJitter * ((float)(rand() % 2001) / 1000.0f - 1.0f);
        const float jb = 1.0f + colorJitter * ((float)(rand() % 2001) / 1000.0f - 1.0f);

        BloodParticle particle;
        particle.position = hitPoint + direction * 0.05f;
        particle.velocity = direction * speed;
        particle.color = glm::vec3(
            std::clamp(bloodCfg.color.x * jr, 0.0f, 1.0f),
            std::clamp(bloodCfg.color.y * jg, 0.0f, 1.0f),
            std::clamp(bloodCfg.color.z * jb, 0.0f, 1.0f));
        particle.size = std::max(0.01f, size);
        particle.lifetime = baseLifetime + (float)(rand() % 1001) / 1000.0f;
        particle.alpha = spray.alphaMin +
            (spray.alphaMax - spray.alphaMin) * force;
        particle.rotation = (float)(rand() % 6284) / 1000.0f;
        particle.stretch = 0.7f + (float)(rand() % 601) / 1000.0f;
        mBloodParticles.push_back(particle);
    }

    for (int i = 0; i < debrisCount; ++i) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float radial = std::sqrt((float)(rand() % 1001) / 1000.0f) * bloodConeRadius;
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
        printf("[BLOOD] force=%.2f blood=%d debris=%d speed=%.2f cone=%.0f\n",
               force, bloodCount, debrisCount, baseSpeed, coneDegrees);
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

    spawnBloodSurfaceDecals(hitPoint, forward, tangent, bitangent, damageScale, force,
                            sourceActorId, targetActorId);
}

void EffectPartSystem::spawnBloodSurfaceDecals(
    const glm::vec3& hitPoint,
    const glm::vec3& forward,
    const glm::vec3& tangent,
    const glm::vec3& bitangent,
    float damageScale,
    float force,
    const std::string& sourceActorId,
    const std::string& targetActorId)
{
    // Blood surface splats: deferred ray-tracing approach.
    // Instead of ray-tracing ALL decals immediately (which causes frame spikes),
    // we queue the requests and process them 2 per frame in updatePendingBloodDecals().
    const auto& cfg = ImpactDecalsConfig::instance().data();
    const auto& bloodCfg = cfg.blood;
    if (!cfg.enabled || !bloodCfg.enabled || !mWorld)
        return;

    const int decalCount = std::max(1, (int)std::round(
        bloodCfg.minCount + (bloodCfg.count - bloodCfg.minCount) * force));
    const float decalRadius = bloodCfg.minRadius +
        (bloodCfg.radius - bloodCfg.minRadius) * force;
    const float coneDist = std::max(0.5f, bloodCfg.coneDistance);
    const float decalConeRadius = std::tan(glm::radians(std::max(1.0f, bloodCfg.coneDegrees)));
    mBloodDebugSegmentCount = 0;

    // Queue all decal requests for deferred processing
    for (int dec = 0; dec < decalCount; ++dec) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float radial = std::sqrt((float)(rand() % 1001) / 1000.0f) * decalConeRadius;
        const float dist = 0.5f + ((float)(rand() % 1001) / 1000.0f) * (coneDist - 0.5f);
        const glm::vec3 coneDir = glm::normalize(
            forward +
            tangent * std::cos(angle) * radial +
            bitangent * std::sin(angle) * radial);

        BloodDecalRequest req;
        req.hitPoint = hitPoint + coneDir * dist;
        req.forward = coneDir;
        req.tangent = tangent;
        req.bitangent = bitangent;
        req.damageScale = damageScale;
        req.force = force;
        req.sourceActorId = sourceActorId;
        req.targetActorId = targetActorId;

        // Cap pending requests to avoid unbounded growth
        if ((int)mBloodDecalRequests.size() < bloodCfg.maxCount)
            mBloodDecalRequests.push_back(std::move(req));
    }

    if (DebugConfig::DEBUG_BLOOD_HITS) {
        Debug::log(Debug::Category::NpcCombat,
            "[BLOOD DECAL] queued=%d pending=%zu\n", decalCount, mBloodDecalRequests.size());
    }
}

// Process deferred blood decal requests: ray-trace at most rayBudgetPerFrame per frame.
// Called from update() to spread the expensive ray-trace cost across multiple frames.
void EffectPartSystem::processDeferredBloodDecals()
{
    const auto& cfg = ImpactDecalsConfig::instance().data();
    const auto& bloodCfg = cfg.blood;
    if (!cfg.enabled || !bloodCfg.enabled || !mWorld) {
        mBloodDecalRequests.clear();
        return;
    }

    const int rayBudget = std::max(1, bloodCfg.stagger.rayBudgetPerFrame);
    int raysCast = 0;

    auto it = mBloodDecalRequests.begin();
    while (it != mBloodDecalRequests.end() && raysCast < rayBudget) {
        const auto& req = *it;

        // Do the ray-trace (the expensive part) - only rayBudgetPerFrame per frame
        BloodWorldHit surfaceHit;
        bool foundSurface = false;

        BloodWorldHit downHit;
        const float downLen = 2.0f + req.damageScale * 1.0f;
        if (traceBloodSegment(*mWorld, req.hitPoint, req.hitPoint + glm::vec3(0, 0, -downLen), downHit)) {
            downHit.position += downHit.normal * 0.01f;
            surfaceHit = downHit;
            foundSurface = true;
        }

        if (!foundSurface) {
            BloodWorldHit fwdHit;
            if (traceBloodSegment(*mWorld, req.hitPoint, req.hitPoint + req.forward * 2.0f, fwdHit)) {
                fwdHit.position += fwdHit.normal * 0.01f;
                surfaceHit = fwdHit;
                foundSurface = true;
            }
        }

        if (!foundSurface) {
            const glm::vec3 sideDir = glm::normalize(glm::cross(req.forward, glm::vec3(0, 0, 1)));
            BloodWorldHit sideHit;
            if (traceBloodSegment(*mWorld, req.hitPoint, req.hitPoint + sideDir * 2.0f, sideHit)) {
                sideHit.position += sideHit.normal * 0.01f;
                surfaceHit = sideHit;
                foundSurface = true;
            }
        }

        raysCast++;

        if (!foundSurface) {
            it = mBloodDecalRequests.erase(it);
            continue;
        }

        // Create the SurfaceDecal from the ray-trace result
        const float decalRadius = bloodCfg.minRadius +
            (bloodCfg.radius - bloodCfg.minRadius) * req.force;

        SurfaceDecal decal;
        decal.position = surfaceHit.position;
        decal.normal = surfaceHit.normal;
        const float jr = 1.0f + bloodCfg.colorVariation * ((float)(rand() % 2001) / 1000.0f - 1.0f);
        const float jg = 1.0f + bloodCfg.colorVariation * ((float)(rand() % 2001) / 1000.0f - 1.0f);
        const float jb = 1.0f + bloodCfg.colorVariation * ((float)(rand() % 2001) / 1000.0f - 1.0f);
        decal.color = glm::vec3(
            std::clamp(bloodCfg.color.x * jr, 0.0f, 1.0f),
            std::clamp(bloodCfg.color.y * jg, 0.0f, 1.0f),
            std::clamp(bloodCfg.color.z * jb, 0.0f, 1.0f));
        decal.kind = SurfaceDecalKind::Blood;
        decal.radius = std::max(0.005f, decalRadius);
        decal.height = std::max(0.005f, bloodCfg.height);
        decal.lifetime = bloodCfg.lifetime;
        decal.fadeTime = bloodCfg.fadeTime;
        decal.alpha = bloodCfg.alpha;
        decal.baseAlpha = bloodCfg.alpha;
        decal.darkenOverLifetime = bloodCfg.colorOverLifetime.enabled;
        decal.colorStart = bloodCfg.colorOverLifetime.startColor;
        decal.colorEnd = bloodCfg.colorOverLifetime.endColor;
        decal.darkenStartSeconds = bloodCfg.colorOverLifetime.darkenStartSeconds;
        decal.darkenEndSeconds = bloodCfg.colorOverLifetime.darkenEndSeconds;
        pushSurfaceDecal(decal, bloodCfg.maxCount);

        // Record replay event
        ReplayEffectEvent decalEvent;
        decalEvent.type = "blood_splatter";
        decalEvent.position = decal.position;
        decalEvent.normal = decal.normal;
        decalEvent.scale = glm::vec3(decal.radius, decal.radius, decal.height);
        decalEvent.color = glm::vec4(decal.color, decal.alpha);
        decalEvent.lifetime = decal.lifetime;
        decalEvent.sourceActorId = req.sourceActorId;
        decalEvent.targetActorId = req.targetActorId;
        captureReplayEffect(decalEvent);

        it = mBloodDecalRequests.erase(it);
    }

    // Cap queue size to prevent unbounded growth
    const int maxQueueSize = bloodCfg.maxCount * 2;
    while ((int)mBloodDecalRequests.size() > maxQueueSize)
        mBloodDecalRequests.erase(mBloodDecalRequests.begin());
}
