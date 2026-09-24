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

#include "combat/hitscan-model.h"
#include "hot-reload/hot-geometry.h"

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
    // Geometry primitive is hot-editable (net.geometry).
    MimitaNet::GameGeometryQueryV1 q{};
    q.primitiveId = MimitaNet::GAME_GEOM_CLOSEST_POINT_SEGMENT;
    q.pointA[0] = a.x; q.pointA[1] = a.y; q.pointA[2] = a.z;
    q.pointB[0] = b.x; q.pointB[1] = b.y; q.pointB[2] = b.z;
    q.point[0] = p.x; q.point[1] = p.y; q.point[2] = p.z;
    MimitaNet::runGeometryPrimitive(q);
    return glm::vec3(q.outClosest[0], q.outClosest[1], q.outClosest[2]);
}

static bool rayAabb(const glm::vec3& origin,
                    const glm::vec3& direction,
                    const glm::vec3& bmin,
                    const glm::vec3& bmax,
                    float maxDistance,
                    float& outDistance)
{
    // Geometry primitive is hot-editable (net.geometry).
    MimitaNet::GameGeometryQueryV1 q{};
    q.primitiveId = MimitaNet::GAME_GEOM_RAY_AABB;
    q.origin[0] = origin.x; q.origin[1] = origin.y; q.origin[2] = origin.z;
    q.direction[0] = direction.x; q.direction[1] = direction.y; q.direction[2] = direction.z;
    q.boxMin[0] = bmin.x; q.boxMin[1] = bmin.y; q.boxMin[2] = bmin.z;
    q.boxMax[0] = bmax.x; q.boxMax[1] = bmax.y; q.boxMax[2] = bmax.z;
    q.maxDistance = maxDistance;
    MimitaNet::runGeometryPrimitive(q);
    outDistance = q.outDistance;
    return q.hit != 0;
}

static bool sweptPointSphere(const glm::vec3& previous,
                             const glm::vec3& current,
                             float radius,
                             const PlayerTarget& target,
                             PhysicalContactHit& outHit)
{
    // Geometry primitive is hot-editable (net.geometry).
    MimitaNet::GameGeometryQueryV1 q{};
    q.primitiveId = MimitaNet::GAME_GEOM_SWEPT_POINT_SPHERE;
    q.pointA[0] = previous.x; q.pointA[1] = previous.y; q.pointA[2] = previous.z;
    q.pointB[0] = current.x; q.pointB[1] = current.y; q.pointB[2] = current.z;
    q.radius = radius;
    q.sphereCenter[0] = target.position.x;
    q.sphereCenter[1] = target.position.y;
    q.sphereCenter[2] = target.position.z;
    q.targetRadius = target.radius;
    MimitaNet::runGeometryPrimitive(q);
    if (q.hit == 0)
        return false;

    outHit.hit = true;
    outHit.targetPlayerId = target.playerId;
    outHit.targetSpawnGeneration = target.spawnGeneration;
    outHit.distance = q.outDistance;
    outHit.normal = glm::vec3(q.outNormal[0], q.outNormal[1], q.outNormal[2]);
    outHit.hitPosition = glm::vec3(q.outPoint[0], q.outPoint[1], q.outPoint[2]);
    return true;
}

} // namespace

float paramOr(const WeaponDefinition& def, const char* key, float fallback)
{
    auto it = def.customParams.find(key);
    return it != def.customParams.end() ? it->second : fallback;
}

float hitscanFalloffFactor(const WeaponDefinition& def, float distance)
{
    HitscanDamageParams p;
    p.distanceFalloffStart = paramOr(def, "distanceFalloffStart", 0.0f);
    p.minDamageFraction = paramOr(def, "minDamageFraction", 0.1f);
    p.falloffExponent = paramOr(def, "falloffExponent", 1.0f);
    return ::hitscanFalloffFactor(p, distance);
}

float hitscanPartMultiplier(const WeaponDefinition& def, const std::string& bodyPart)
{
    HitscanDamageParams p;
    p.headshotMultiplier = def.headshotMultiplier;
    p.limbDamageMultiplier = paramOr(def, "limbDamageMultiplier", 0.75f);
    const bool head = bodyPart == "head";
    const bool leg = !head && bodyPart.find("leg") != std::string::npos;
    return ::hitscanPartMultiplier(p, head, leg);
}

int computeHitscanDamage(const WeaponDefinition& def, const std::string& bodyPart,
                         float distance, float angleFactor)
{
    HitscanDamageParams p;
    p.baseDamage = def.damage;
    p.headshotMultiplier = def.headshotMultiplier;
    p.limbDamageMultiplier = paramOr(def, "limbDamageMultiplier", 0.75f);
    p.distanceFalloffStart = paramOr(def, "distanceFalloffStart", 0.0f);
    p.minDamageFraction = paramOr(def, "minDamageFraction", 0.1f);
    p.falloffExponent = paramOr(def, "falloffExponent", 1.0f);
    const bool head = bodyPart == "head";
    const bool leg = !head && bodyPart.find("leg") != std::string::npos;
    return ::computeHitscanDamage(p, head, leg, distance, angleFactor);
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
    (void)seed; // Deterministic fixed grid; the seed is retained for wire compatibility only.
    float spreadDegrees = def.spread;
    auto grid = def.customParams.find("gridSpreadDegrees");
    if (grid != def.customParams.end() && grid->second > 0.0f)
        spreadDegrees = grid->second;
    return buildFixedPelletDirections(
        safeNormalize(aimDirection, glm::vec3(1.0f, 0.0f, 0.0f)),
        std::max(1, def.pelletCount), spreadDegrees, outDirections, capacity);
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

    const glm::vec3 dir = safeNormalize(direction, glm::vec3(1.0f, 0.0f, 0.0f));
    const float grow = std::max(beamRadius, 0.0f);

    // ── Body-part trace (matches the client's rendered hitboxes) ───────
    // The target MUST carry per-part AABBs (reconstructed at the rewound
    // pose). A target without body parts is not hittable — no invisible
    // capsule is ever used for damage. Each part is tested exactly like the
    // client's beam, so a shot that hits an arm/head/leg on the shooter's
    // screen registers the same part here. The damage model then applies the
    // same multiplier the client predicted (head = headshot, leg = limb,
    // torso/arm = 1x).
    if (target.bodyParts.empty())
        return false;
    {
        HitscanPelletHit best;
        best.distance = std::numeric_limits<float>::max();
        bool hitAny = false;
        for (const PlayerTarget::BodyPartBox& part : target.bodyParts)
        {
            const glm::vec3 ebmin = part.center - part.half - glm::vec3(grow);
            const glm::vec3 ebmax = part.center + part.half + glm::vec3(grow);
            float distance = 0.0f;
            if (!rayAabb(origin, dir, ebmin, ebmax, maxDistance, distance))
                continue;
            if (distance >= best.distance)
                continue;
            const glm::vec3 sweepCenter = origin + dir * distance;
            const glm::vec3 fromCenter = sweepCenter - part.center;
            HitscanPelletHit hit;
            hit.hit = true;
            hit.targetPlayerId = target.playerId;
            hit.targetSpawnGeneration = target.spawnGeneration;
            hit.distance = distance;
            hit.direction = dir;
            hit.hitNormal = safeNormalize(fromCenter, -dir);
            hit.hitPosition = grow > 0.0f
                ? sweepCenter - hit.hitNormal * grow
                : sweepCenter;
            hit.bodyPart = part.bodyPart;
            hit.headshot = part.bodyPart == HitBodyPart::Head;
            best = hit;
            hitAny = true;
        }
        if (!hitAny)
            return false;
        outHit = best;
        return true;
    }
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
        // falloff = pow(clamp(1 - distance/falloffStart, minFraction, 1), exponent).
        // The angle factor is NOT applied here: the server re-trace's AABB
        // normal is center-derived (not a surface normal), and client remote
        // prediction passes angleFactor = 1.0 too, so damage stays consistent.
        std::string bodyPart = "torso";
        if (closest.bodyPart == HitBodyPart::Head)
            bodyPart = "head";
        else if (closest.bodyPart == HitBodyPart::Leg)
            bodyPart = "leg";
        closest.damage = (float)computeHitscanDamage(def, bodyPart, closest.distance, 1.0f);
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
