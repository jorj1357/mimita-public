// 07 21 2026, 21 00
/* purpose
* Implements shared generic hitscan tracing and physical-contact shape helpers.
* Keeps deterministic pellet spread, body-part damage, swept contact, and episode batching testable.
* Provides transport-neutral primitives used by authoritative server weapon execution.
* Does NOT apply health damage, mutate weapon runtime, send packets, or render presentation effects.
* Does NOT own projectile behavior, config loading, player input collection, or world triangle storage.
* Does NOT trust client target, damage, death, health, or knockback claims.
*/

#include "combat/weapon-execution.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace WeaponExecution {
namespace {

static bool finiteVec3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static glm::vec3 safeNormalize(const glm::vec3& v, const glm::vec3& fallback)
{
    if (!finiteVec3(v) || glm::length(v) <= 0.0001f)
        return fallback;
    return glm::normalize(v);
}

static glm::vec3 closestPointOnSegment(const glm::vec3& a,
                                       const glm::vec3& b,
                                       const glm::vec3& p)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    if (len2 <= 0.000001f)
        return a;
    const float t = glm::clamp(glm::dot(p - a, ab) / len2, 0.0f, 1.0f);
    return a + ab * t;
}

static bool rayAabb(const glm::vec3& origin,
                    const glm::vec3& direction,
                    const glm::vec3& bmin,
                    const glm::vec3& bmax,
                    float maxDistance,
                    float& outDistance)
{
    float tmin = 0.0f;
    float tmax = maxDistance;
    for (int axis = 0; axis < 3; ++axis)
    {
        if (std::fabs(direction[axis]) < 0.000001f)
        {
            if (origin[axis] < bmin[axis] || origin[axis] > bmax[axis])
                return false;
            continue;
        }

        const float inv = 1.0f / direction[axis];
        float t1 = (bmin[axis] - origin[axis]) * inv;
        float t2 = (bmax[axis] - origin[axis]) * inv;
        if (t1 > t2)
            std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax)
            return false;
    }
    outDistance = tmin;
    return outDistance >= 0.0f && outDistance <= maxDistance;
}

static bool sweptPointSphere(const glm::vec3& previous,
                             const glm::vec3& current,
                             float radius,
                             const PlayerTarget& target,
                             PhysicalContactHit& outHit)
{
    const glm::vec3 targetCenter = target.position;
    const float targetRadius = std::max(target.radius, 0.01f);
    const float sumRadius = radius + targetRadius;
    const glm::vec3 closest = closestPointOnSegment(previous, current, targetCenter);
    const glm::vec3 delta = targetCenter - closest;
    const float distance = glm::length(delta);
    if (distance > sumRadius)
        return false;

    outHit.hit = true;
    outHit.targetPlayerId = target.playerId;
    outHit.targetSpawnGeneration = target.spawnGeneration;
    outHit.distance = distance;
    outHit.normal = distance > 0.0001f
        ? glm::normalize(delta)
        : glm::vec3(0.0f, 0.0f, 1.0f);
    outHit.hitPosition = closest + outHit.normal * radius;
    return true;
}

} // namespace

float paramOr(const WeaponDefinition& def, const char* key, float fallback)
{
    auto it = def.customParams.find(key);
    return it != def.customParams.end() ? it->second : fallback;
}

WeaponExecutionType executionTypeForBehavior(WeaponBehaviorType behavior)
{
    return weaponExecutionTypeForBehavior(behavior);
}

int buildPelletDirections(const WeaponDefinition& def,
                          const glm::vec3& aimDirection,
                          uint32_t seed,
                          glm::vec3* outDirections,
                          int capacity)
{
    PelletPatternConfig config;
    config.pelletCount = std::max(1, def.pelletCount);
    config.spreadDegrees = def.spread;
    auto grid = def.customParams.find("gridSpreadDegrees");
    if (grid != def.customParams.end() && grid->second > 0.0f)
        config.spreadDegrees = grid->second;
    config.spreadSeed = seed;
    return generatePelletDirections(
        safeNormalize(aimDirection, glm::vec3(1.0f, 0.0f, 0.0f)),
        config, outDirections, capacity);
}

bool rayPlayerTarget(const glm::vec3& origin,
                     const glm::vec3& direction,
                     const PlayerTarget& target,
                     float maxDistance,
                     float beamRadius,
                     HitscanPelletHit& outHit)
{
    if (target.dead || target.playerId == 0 || !finiteVec3(target.position))
        return false;

    const float radius = std::max(target.radius, 0.01f);
    const float halfHeight = std::max(target.height, radius * 2.0f) * 0.5f;
    const glm::vec3 bmin = target.position + glm::vec3(-radius, -radius, -halfHeight);
    const glm::vec3 bmax = target.position + glm::vec3(radius, radius, halfHeight);
    const glm::vec3 dir = safeNormalize(direction, glm::vec3(1.0f, 0.0f, 0.0f));

    // Swept-sphere vs AABB == thin ray vs AABB grown by the beam radius. The
    // sphere center at contact drives body-part classification; the stored hit
    // position is projected back to the target surface for effect placement.
    const float grow = std::max(beamRadius, 0.0f);
    const glm::vec3 ebmin = bmin - glm::vec3(grow);
    const glm::vec3 ebmax = bmax + glm::vec3(grow);
    float distance = 0.0f;
    if (!rayAabb(origin, dir, ebmin, ebmax, maxDistance, distance))
        return false;

    outHit.hit = true;
    outHit.targetPlayerId = target.playerId;
    outHit.targetSpawnGeneration = target.spawnGeneration;
    outHit.distance = distance;
    outHit.direction = dir;
    const glm::vec3 sweepCenter = origin + dir * distance;
    const glm::vec3 fromCenter = sweepCenter - target.position;
    outHit.hitNormal = safeNormalize(fromCenter, -dir);
    outHit.hitPosition = grow > 0.0f
        ? sweepCenter - outHit.hitNormal * grow
        : sweepCenter;
    // Body-part classification by sphere-center height over the original target
    // box (matches the client's localHeight thresholds): top ~22% head, bottom
    // ~32% limbs, middle torso. `headshot` stays the flag for effects/killfeed.
    const float localHeight = (sweepCenter.z - bmin.z) / (bmax.z - bmin.z);
    if (localHeight > 0.78f)
    {
        outHit.bodyPart = HitBodyPart::Head;
        outHit.headshot = true;
    }
    else if (localHeight > 0.32f)
    {
        outHit.bodyPart = HitBodyPart::Torso;
    }
    else
    {
        outHit.bodyPart = HitBodyPart::Leg;
    }
    return true;
}

HitscanTraceResult traceHitscan(const WeaponDefinition& def,
                                const glm::vec3& origin,
                                const glm::vec3& direction,
                                const HitscanTraceConfig& config,
                                const std::vector<PlayerTarget>& targets)
{
    HitscanTraceResult result;
    glm::vec3 pelletDirs[MAX_PELLETS_PER_BLAST]{};
    WeaponDefinition copy = def;
    copy.pelletCount = config.pelletCount > 0 ? config.pelletCount : def.pelletCount;
    copy.spread = config.spreadDegrees >= 0.0f ? config.spreadDegrees : def.spread;
    const int pelletCount = buildPelletDirections(
        copy, direction, config.deterministicSeed, pelletDirs, MAX_PELLETS_PER_BLAST);
    result.pelletCount = pelletCount;

    const float maxRange = std::max(0.01f, config.maxRange);
    const float worldBlockDistance = std::clamp(config.worldBlockDistance, 0.0f, maxRange);
    for (int i = 0; i < pelletCount; ++i)
    {
        HitscanPelletHit closest;
        closest.distance = std::numeric_limits<float>::max();

        for (const PlayerTarget& target : targets)
        {
            HitscanPelletHit hit;
            if (!rayPlayerTarget(origin, pelletDirs[i], target, maxRange,
                                 config.beamThickness, hit))
                continue;
            if (hit.distance < closest.distance)
                closest = hit;
        }

        if (!closest.hit || closest.distance > worldBlockDistance)
            continue;

        // Damage model: base x body-part multiplier x range falloff.
        // head = headshotMultiplier, torso = 1x, limbs = limbDamageMultiplier.
        // falloff = clamp(1 - distance/falloffStart, minFraction, 1); 0 disables.
        float partMultiplier = 1.0f;
        if (closest.bodyPart == HitBodyPart::Head)
            partMultiplier = config.headshotMultiplier;
        else if (closest.bodyPart == HitBodyPart::Leg)
            partMultiplier = config.limbDamageMultiplier;
        float falloff = 1.0f;
        if (config.distanceFalloffStart > 0.0f)
            falloff = std::clamp(1.0f - closest.distance / config.distanceFalloffStart,
                                 config.minDamageFraction, 1.0f);
        closest.damage = config.damage * partMultiplier * falloff;
        result.pellets[i] = closest;

        auto aggregateIt = std::find_if(result.aggregates.begin(), result.aggregates.end(),
            [&](const HitscanDamageAggregate& aggregate) {
                return aggregate.targetPlayerId == closest.targetPlayerId &&
                    aggregate.targetSpawnGeneration == closest.targetSpawnGeneration;
            });
        if (aggregateIt == result.aggregates.end())
        {
            HitscanDamageAggregate aggregate;
            aggregate.targetPlayerId = closest.targetPlayerId;
            aggregate.targetSpawnGeneration = closest.targetSpawnGeneration;
            aggregate.hitPosition = closest.hitPosition;
            aggregate.hitNormal = closest.hitNormal;
            aggregate.headshot = closest.headshot;
            result.aggregates.push_back(aggregate);
            aggregateIt = result.aggregates.end() - 1;
        }

        aggregateIt->damage += std::max(1, (int)std::round(closest.damage));
        aggregateIt->pelletHits += 1;
        aggregateIt->knockback += closest.direction * (closest.damage * config.knockbackPerDamage);
        if (closest.headshot)
            aggregateIt->headshot = true;
    }

    return result;
}

glm::vec3 physicalShapeCenter(const PhysicalContactShape& shape)
{
    if (shape.kind == PhysicalShapeKind::Sphere)
        return shape.currentA;
    return (shape.currentA + shape.currentB) * 0.5f;
}

float physicalShapeTravelDistance(const PhysicalContactShape& shape)
{
    if (shape.kind == PhysicalShapeKind::Sphere)
        return glm::length(shape.currentA - shape.previousA);
    const glm::vec3 prevCenter = (shape.previousA + shape.previousB) * 0.5f;
    const glm::vec3 curCenter = (shape.currentA + shape.currentB) * 0.5f;
    return glm::length(curCenter - prevCenter);
}

bool testPhysicalContact(const PhysicalContactShape& shape,
                         const PlayerTarget& target,
                         PhysicalContactHit& outHit)
{
    if (target.dead || target.playerId == 0)
        return false;

    if (shape.kind == PhysicalShapeKind::Sphere)
        return sweptPointSphere(shape.previousA, shape.currentA, shape.radius, target, outHit);

    PhysicalContactHit best;
    best.distance = std::numeric_limits<float>::max();
    bool hit = false;
    for (int i = 0; i <= 4; ++i)
    {
        const float t = (float)i / 4.0f;
        const glm::vec3 prev = shape.previousA + (shape.previousB - shape.previousA) * t;
        const glm::vec3 cur = shape.currentA + (shape.currentB - shape.currentA) * t;
        PhysicalContactHit sample;
        if (!sweptPointSphere(prev, cur, shape.radius, target, sample))
            continue;
        if (sample.distance < best.distance)
            best = sample;
        hit = true;
    }
    if (hit)
        outHit = best;
    return hit;
}

bool episodeShouldConfirm(const PhysicalContactEpisode& episode,
                          bool ending,
                          uint8_t batchSize)
{
    if (!episode.active || episode.pendingConfirmationDamage <= 0)
        return false;
    if (ending)
        return true;
    return episode.samplesSinceConfirmation >= std::max<uint8_t>(1, batchSize);
}

} // namespace WeaponExecution
