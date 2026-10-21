// 2026-10-20
/* purpose
* Implement the first shared actor-pair collision slice: broadphase grid,
* swept capsule contact, flat-top support/carry, and impact-angle push.
* One path for every actor pairing; no player/NPC/monster special cases.
* Does NOT own world collision, damage, rendering, or network transport.
*/
#include "physics/movement/actor-pair-collision.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

namespace {

constexpr float kContactMargin = 0.002f;
constexpr float kSupportUpwardTolerance = 0.5f;
constexpr float kVelocityEpsilon = 1e-5f;

struct CellEntry
{
    std::uint64_t key = 0;
    std::uint32_t rank = 0;
};

struct PairEntry
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
};

struct Scratch
{
    std::vector<std::uint32_t> order;
    std::vector<std::uint64_t> keys;
    std::vector<CellEntry> cells;
    std::vector<PairEntry> pairs;
};

Scratch& scratch()
{
    static thread_local Scratch s;
    return s;
}

std::uint64_t makeCellKey(int x, int y, int z)
{
    constexpr std::uint64_t mask = (1ull << 21) - 1ull;
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) & mask) |
           ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) & mask) << 21) |
           ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) & mask) << 42);
}

int clampedCellCoord(float value, float cellSize)
{
    const double raw = std::floor(static_cast<double>(value) / static_cast<double>(cellSize));
    if (!std::isfinite(raw))
        return 0;
    constexpr int limit = (1 << 20) - 1;
    return std::clamp(static_cast<int>(raw), -limit, limit);
}

Capsule makeActorCapsule(const ActorPairCollisionActor& actor,
                         const glm::vec3& pos)
{
    const ActorCollisionProfile* profile = actor.profile;
    if (!profile)
        return Capsule{pos, pos, 0.7f};

    const float scale = std::max(actor.sizeScale, 0.001f);
    const float radius = std::max(profile->capsuleRadiusM * scale, 0.01f);
    const float height = std::max(profile->capsuleHeightM * scale, radius * 2.0f);

    if (actor.dead)
    {
        const float feetZ = pos.z - height * 0.5f;
        const float centerZ = feetZ + radius;
        const float length = std::max(profile->deadCapsuleLengthM * scale, radius * 2.0f);
        const float halfAxis = std::max(0.0f, length * 0.5f - radius);
        const glm::vec3 axis(std::cos(actor.yaw) * halfAxis,
                             std::sin(actor.yaw) * halfAxis,
                             0.0f);
        const glm::vec3 center(pos.x, pos.y, centerZ);
        Capsule cap;
        cap.a = center - axis;
        cap.b = center + axis;
        cap.r = radius;
        return cap;
    }

    const float half = height * 0.5f;
    Capsule cap;
    cap.r = radius;
    cap.a = pos - glm::vec3(0.0f, 0.0f, std::max(0.0f, half - radius));
    cap.b = pos + glm::vec3(0.0f, 0.0f, std::max(0.0f, half - radius));
    return cap;
}

Capsule translated(const Capsule& cap, const glm::vec3& move)
{
    Capsule out = cap;
    out.a += move;
    out.b += move;
    return out;
}

float capsuleBottom(const Capsule& cap)
{
    return std::min(cap.a.z, cap.b.z) - cap.r;
}

float capsuleTop(const Capsule& cap)
{
    return std::max(cap.a.z, cap.b.z) + cap.r;
}

glm::vec2 capsuleCenterXY(const Capsule& cap)
{
    return glm::vec2((cap.a.x + cap.b.x) * 0.5f,
                     (cap.a.y + cap.b.y) * 0.5f);
}

float horizontalDistanceToCapsule(const glm::vec2& point, const Capsule& cap)
{
    const glm::vec2 a(cap.a.x, cap.a.y);
    const glm::vec2 b(cap.b.x, cap.b.y);
    const glm::vec2 ab = b - a;
    const float length2 = glm::dot(ab, ab);
    if (length2 <= 1e-10f)
        return glm::length(point - a);
    const float t = glm::clamp(glm::dot(point - a, ab) / length2, 0.0f, 1.0f);
    return glm::length(point - (a + ab * t));
}

void closestSegmentSegment(const glm::vec3& p1, const glm::vec3& q1,
                           const glm::vec3& p2, const glm::vec3& q2,
                           glm::vec3& c1, glm::vec3& c2)
{
    const glm::vec3 d1 = q1 - p1;
    const glm::vec3 d2 = q2 - p2;
    const glm::vec3 r = p1 - p2;
    const float a = glm::dot(d1, d1);
    const float e = glm::dot(d2, d2);
    const float f = glm::dot(d2, r);

    float s = 0.0f;
    float t = 0.0f;

    if (a <= 1e-12f && e <= 1e-12f)
    {
        c1 = p1;
        c2 = p2;
        return;
    }
    if (a <= 1e-12f)
    {
        s = 0.0f;
        t = glm::clamp(f / e, 0.0f, 1.0f);
    }
    else
    {
        const float c = glm::dot(d1, r);
        if (e <= 1e-12f)
        {
            t = 0.0f;
            s = glm::clamp(-c / a, 0.0f, 1.0f);
        }
        else
        {
            const float b = glm::dot(d1, d2);
            const float denom = a * e - b * b;
            if (denom != 0.0f)
                s = glm::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
            else
                s = 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f)
            {
                t = 0.0f;
                s = glm::clamp(-c / a, 0.0f, 1.0f);
            }
            else if (t > 1.0f)
            {
                t = 1.0f;
                s = glm::clamp((b - c) / a, 0.0f, 1.0f);
            }
        }
    }

    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

float capsuleDistance(const Capsule& a, const Capsule& b,
                      glm::vec3* outA = nullptr, glm::vec3* outB = nullptr)
{
    glm::vec3 ca;
    glm::vec3 cb;
    closestSegmentSegment(a.a, a.b, b.a, b.b, ca, cb);
    if (outA) *outA = ca;
    if (outB) *outB = cb;
    return glm::length(ca - cb);
}

bool sweepCapsulePair(const Capsule& startA, const Capsule& startB,
                      const glm::vec3& relativeMove, float combinedRadius,
                      float& outT)
{
    const float startDistance = capsuleDistance(startA, startB);
    if (startDistance <= combinedRadius)
    {
        outT = 0.0f;
        return true;
    }

    const float moveLength = glm::length(relativeMove);
    if (moveLength <= kVelocityEpsilon)
        return false;

    const float stepLength = std::max(
        0.02f, std::min(combinedRadius * 0.5f, 0.25f));
    const int steps = std::clamp(
        static_cast<int>(std::ceil(moveLength / stepLength)), 1, 128);

    float previousT = 0.0f;
    for (int i = 1; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        if (capsuleDistance(translated(startA, relativeMove * t), startB) <=
            combinedRadius)
        {
            float lo = previousT;
            float hi = t;
            for (int iteration = 0; iteration < 8; ++iteration)
            {
                const float mid = (lo + hi) * 0.5f;
                if (capsuleDistance(translated(startA, relativeMove * mid),
                                    startB) <= combinedRadius)
                    hi = mid;
                else
                    lo = mid;
            }
            outT = hi;
            return true;
        }
        previousT = t;
    }
    return false;
}

glm::vec3 fallbackNormal(const Capsule& a, const Capsule& b,
                         const glm::vec3& relativeMove)
{
    const glm::vec3 delta = (a.a + a.b) * 0.5f - (b.a + b.b) * 0.5f;
    const float deltaLength2 = glm::dot(delta, delta);
    if (deltaLength2 > 1e-10f)
        return delta / std::sqrt(deltaLength2);
    const float moveLength2 = glm::dot(relativeMove, relativeMove);
    if (moveLength2 > 1e-10f)
        return relativeMove / std::sqrt(moveLength2);
    return glm::vec3(0.0f, 0.0f, 1.0f);
}

float pushSpeedFor(const ActorCollisionPushTuning& p, float impactSpeed,
                   float directness)
{
    const float weakSpeed = p.weakHitDistanceM /
        std::max(0.01f, p.weakHitMotionSeconds);
    const float denominator = std::max(1e-6f, p.maxImpactSpeedMps - p.minImpactSpeedMps);
    const float u = glm::clamp((impactSpeed - p.minImpactSpeedMps) / denominator,
                               0.0f, 1.0f);
    const float speedCurve = std::pow(u, std::max(0.01f, p.impactSpeedCurveExponent));
    const float angleScale = p.impactAngleMinScale +
        (1.0f - p.impactAngleMinScale) *
            std::pow(glm::clamp(directness, 0.0f, 1.0f),
                     std::max(0.01f, p.impactAngleCurveExponent));
    const float knockback = std::max(weakSpeed, p.maxKnockbackSpeedMps * speedCurve) *
                            angleScale;
    return std::min(knockback, p.maxKnockbackSpeedMps);
}

bool addPush(ActorPairCollisionOutput& target,
             const ActorPairCollisionActor& targetActor,
             const glm::vec3& direction, float impactSpeed, float directness)
{
    if (targetActor.staticCollider || targetActor.dead || !targetActor.profile)
        return false;
    const float speed = pushSpeedFor(targetActor.profile->push,
                                     impactSpeed, directness);
    if (speed <= kVelocityEpsilon)
        return false;
    target.externalImpulse += direction * speed;
    target.pushed = true;
    return true;
}

bool trySetSupport(std::uint32_t upperRank, std::uint32_t lowerRank,
                   const Capsule& upper, const Capsule& lower,
                   const ActorPairCollisionActor& upperActor,
                   const ActorPairCollisionActor& lowerActor,
                   ActorPairCollisionOutput& upperOutput,
                   ActorPairCollisionOutput& lowerOutput)
{
    if (upperActor.staticCollider || upperActor.dead || !upperActor.profile)
        return false;
    const std::uint64_t supportKey = actorPairKey(lowerActor.kind, lowerActor.id);
    const bool continuingSupport = upperActor.previousSupportKey == supportKey;
    if (upperOutput.onGround && !continuingSupport)
        return false;

    const float upperBottom = capsuleBottom(upper);
    const float lowerTop = capsuleTop(lower);
    const float tolerance = upperActor.profile->push.supportTopToleranceM;
    const float snapMax = upperActor.profile->push.supportSnapMaxM;
    if (upperBottom < lowerTop - tolerance || upperBottom > lowerTop + snapMax)
        return false;
    if (upperBottom <= capsuleBottom(lower))
        return false;

    const glm::vec2 upperCenter = capsuleCenterXY(upper);
    if (horizontalDistanceToCapsule(upperCenter, lower) >= upper.r + lower.r)
        return false;

    const float relativeVertical = upperOutput.vel.z - lowerOutput.vel.z;
    if (relativeVertical > kSupportUpwardTolerance)
        return false;
    const float approachSpeed = std::max(0.0f, -relativeVertical);
    if (approachSpeed > upperActor.profile->push.supportMaxImpactSpeedMps)
        return false;

    upperOutput.pos.z += (lowerTop - upperBottom);
    upperOutput.vel.z = std::max(upperOutput.vel.z, 0.0f) +
                        std::max(lowerOutput.vel.z, 0.0f);
    upperOutput.onGround = true;
    upperOutput.stableOnGround = true;
    upperOutput.supportKey = supportKey;
    upperOutput.supportVelocity = lowerOutput.vel;
    upperOutput.corrected = true;
    (void)upperRank;
    (void)lowerRank;
    return true;
}

void resolvePair(std::uint32_t rankA, std::uint32_t rankB,
                 const ActorPairCollisionActor* actors,
                 const std::vector<std::uint32_t>& order,
                 ActorPairCollisionOutput* outputs,
                 ActorPairCollisionStats& stats)
{
    const std::uint32_t indexA = order[rankA];
    const std::uint32_t indexB = order[rankB];
    const ActorPairCollisionActor& actorA = actors[indexA];
    const ActorPairCollisionActor& actorB = actors[indexB];
    if (actorA.staticCollider && actorB.staticCollider)
        return;

    ++stats.narrowPairs;

    const Capsule startA = makeActorCapsule(actorA, actorA.prevPos);
    const Capsule startB = makeActorCapsule(actorB, actorB.prevPos);
    const glm::vec3 endMoveA = outputs[indexA].pos - actorA.prevPos;
    const glm::vec3 endMoveB = outputs[indexB].pos - actorB.prevPos;
    const glm::vec3 relativeMove = endMoveA - endMoveB;
    const float combinedRadius = startA.r + startB.r;

    float t = 0.0f;
    if (!sweepCapsulePair(startA, startB, relativeMove, combinedRadius, t))
        return;

    ++stats.contacts;
    const bool aPlayer = actorA.kind == ActorPairKind::Player;
    const bool bPlayer = actorB.kind == ActorPairKind::Player;
    if (aPlayer && bPlayer)
        ++stats.playerPlayerContacts;
    else if (!aPlayer && !bPlayer)
        ++stats.npcNpcContacts;
    else
        ++stats.playerNpcContacts;

    glm::vec3 contactPosA = actorA.prevPos + endMoveA * t;
    glm::vec3 contactPosB = actorB.prevPos + endMoveB * t;
    Capsule capA = makeActorCapsule(actorA, contactPosA);
    Capsule capB = makeActorCapsule(actorB, contactPosB);
    outputs[indexA].pos = contactPosA;
    outputs[indexB].pos = contactPosB;

    if (trySetSupport(rankA, rankB, capA, capB, actorA, actorB,
                      outputs[indexA], outputs[indexB]))
    {
        ++stats.supports;
        return;
    }
    if (trySetSupport(rankB, rankA, capB, capA, actorB, actorA,
                      outputs[indexB], outputs[indexA]))
    {
        ++stats.supports;
        return;
    }

    glm::vec3 closestA;
    glm::vec3 closestB;
    const float distance = capsuleDistance(capA, capB, &closestA, &closestB);
    glm::vec3 normal = distance > kVelocityEpsilon
        ? (closestA - closestB) / distance
        : fallbackNormal(capA, capB, relativeMove);

    const float penetration = std::max(0.0f, combinedRadius - distance);
    if (penetration <= kVelocityEpsilon && t <= 0.0f)
        return;

    const glm::vec3 velocityA = outputs[indexA].vel;
    const glm::vec3 velocityB = outputs[indexB].vel;
    const glm::vec3 relativeVelocity = velocityA - velocityB;

    const bool mutableA = !actorA.staticCollider;
    const bool mutableB = !actorB.staticCollider;
    const float shareA = mutableA ? (mutableB ? 0.5f : 1.0f) : 0.0f;
    const float shareB = mutableB ? (mutableA ? 0.5f : 1.0f) : 0.0f;
    const float correction = penetration + kContactMargin;
    outputs[indexA].pos += normal * (correction * shareA);
    outputs[indexB].pos -= normal * (correction * shareB);
    outputs[indexA].corrected = outputs[indexA].corrected || shareA > 0.0f;
    outputs[indexB].corrected = outputs[indexB].corrected || shareB > 0.0f;

    if (mutableA)
    {
        const float into = glm::dot(outputs[indexA].vel, normal);
        if (into < 0.0f)
            outputs[indexA].vel -= normal * into;
    }
    if (mutableB)
    {
        const float into = glm::dot(outputs[indexB].vel, -normal);
        if (into < 0.0f)
            outputs[indexB].vel += normal * into;
    }

    const float normalSpeedA = std::max(0.0f, -glm::dot(relativeVelocity, normal));
    const float normalSpeedB = std::max(0.0f, glm::dot(relativeVelocity, normal));
    const float impactSpeed = glm::length(relativeVelocity);
    if (impactSpeed <= kVelocityEpsilon)
        return;

    const float directnessA = glm::clamp(normalSpeedA / impactSpeed, 0.0f, 1.0f);
    const float directnessB = glm::clamp(normalSpeedB / impactSpeed, 0.0f, 1.0f);

    if (normalSpeedA > 0.0f &&
        addPush(outputs[indexB], actorB, -normal, impactSpeed, directnessA))
        ++stats.pushes;
    if (normalSpeedB > 0.0f &&
        addPush(outputs[indexA], actorA, normal, impactSpeed, directnessB))
        ++stats.pushes;
}

} // namespace

void resolveActorPairCollisions(
    const ActorPairCollisionActor* actors,
    std::size_t count,
    float dt,
    const ActorCollisionData& data,
    ActorPairCollisionOutput* outputs,
    ActorPairCollisionStats* outStats)
{
    (void)dt;
    ActorPairCollisionStats stats;
    stats.actorCount = static_cast<std::uint32_t>(count);

    const auto startTime = std::chrono::steady_clock::now();

    if (!actors || !outputs || count == 0)
    {
        if (outStats) *outStats = stats;
        return;
    }

    for (std::size_t i = 0; i < count; ++i)
    {
        outputs[i] = ActorPairCollisionOutput{};
        outputs[i].pos = actors[i].pos;
        outputs[i].vel = actors[i].vel;
        outputs[i].externalImpulse = actors[i].externalImpulse;
        outputs[i].onGround = actors[i].onGround;
        outputs[i].stableOnGround = actors[i].stableOnGround;
    }

    Scratch& s = scratch();
    s.order.resize(count);
    s.keys.resize(count);
    for (std::size_t i = 0; i < count; ++i)
        s.order[i] = static_cast<std::uint32_t>(i);
    std::sort(s.order.begin(), s.order.end(),
        [&](std::uint32_t a, std::uint32_t b) {
            const std::uint64_t keyA = actorPairKey(actors[a].kind, actors[a].id);
            const std::uint64_t keyB = actorPairKey(actors[b].kind, actors[b].id);
            if (keyA != keyB)
                return keyA < keyB;
            return a < b;
        });
    for (std::size_t rank = 0; rank < count; ++rank)
        s.keys[rank] = actorPairKey(actors[s.order[rank]].kind,
                                    actors[s.order[rank]].id);

    const float cellSize = std::max(data.broadphaseCellSizeM, 0.25f);
    s.cells.clear();

    for (std::size_t rank = 0; rank < count; ++rank)
    {
        const ActorPairCollisionActor& actor = actors[s.order[rank]];
        const Capsule start = makeActorCapsule(actor, actor.prevPos);
        const Capsule end = makeActorCapsule(actor, outputs[s.order[rank]].pos);

        glm::vec3 min = glm::min(start.a, start.b) - glm::vec3(start.r);
        glm::vec3 max = glm::max(start.a, start.b) + glm::vec3(start.r);
        min = glm::min(min, glm::min(end.a, end.b) - glm::vec3(end.r));
        max = glm::max(max, glm::max(end.a, end.b) + glm::vec3(end.r));

        const int minX = clampedCellCoord(min.x, cellSize);
        const int minY = clampedCellCoord(min.y, cellSize);
        const int minZ = clampedCellCoord(min.z, cellSize);
        const int maxX = clampedCellCoord(max.x, cellSize);
        const int maxY = clampedCellCoord(max.y, cellSize);
        const int maxZ = clampedCellCoord(max.z, cellSize);

        for (int z = minZ; z <= maxZ; ++z)
            for (int y = minY; y <= maxY; ++y)
                for (int x = minX; x <= maxX; ++x)
                    s.cells.push_back({makeCellKey(x, y, z),
                                       static_cast<std::uint32_t>(rank)});
    }
    stats.cellEntries = static_cast<std::uint32_t>(s.cells.size());

    std::sort(s.cells.begin(), s.cells.end(),
        [](const CellEntry& a, const CellEntry& b) {
            if (a.key != b.key) return a.key < b.key;
            return a.rank < b.rank;
        });

    s.pairs.clear();
    for (std::size_t begin = 0; begin < s.cells.size(); )
    {
        std::size_t end = begin + 1;
        while (end < s.cells.size() && s.cells[end].key == s.cells[begin].key)
            ++end;

        for (std::size_t i = begin; i < end; ++i)
        {
            const std::uint32_t rankA = s.cells[i].rank;
            for (std::size_t j = i + 1; j < end; ++j)
            {
                const std::uint32_t rankB = s.cells[j].rank;
                if (rankA == rankB)
                    continue;
                if (s.pairs.size() >= static_cast<std::size_t>(data.maxCandidatePairs))
                {
                    stats.truncated = true;
                    break;
                }
                s.pairs.push_back({std::min(rankA, rankB),
                                   std::max(rankA, rankB)});
            }
            if (stats.truncated)
                break;
        }
        if (stats.truncated)
            break;
        begin = end;
    }

    std::sort(s.pairs.begin(), s.pairs.end(),
        [](const PairEntry& a, const PairEntry& b) {
            if (a.a != b.a) return a.a < b.a;
            return a.b < b.b;
        });
    s.pairs.erase(std::unique(s.pairs.begin(), s.pairs.end(),
        [](const PairEntry& a, const PairEntry& b) {
            return a.a == b.a && a.b == b.b;
        }), s.pairs.end());
    stats.candidatePairs = static_cast<std::uint32_t>(s.pairs.size());
    for (const PairEntry& pair : s.pairs)
    {
        const bool aPlayer = actors[s.order[pair.a]].kind == ActorPairKind::Player;
        const bool bPlayer = actors[s.order[pair.b]].kind == ActorPairKind::Player;
        if (aPlayer && bPlayer)
            ++stats.playerPlayerCandidates;
        else if (!aPlayer && !bPlayer)
            ++stats.npcNpcCandidates;
        else
            ++stats.playerNpcCandidates;
    }

    const auto broadphaseEnd = std::chrono::steady_clock::now();
    stats.broadphaseMs = std::chrono::duration<float, std::milli>(
        broadphaseEnd - startTime).count();

    for (const PairEntry& pair : s.pairs)
        resolvePair(pair.a, pair.b, actors, s.order, outputs, stats);

    // Carry supported actors by the actual displacement of their support this
    // tick. This is additive to their own movement and does not add the support
    // velocity again while support is continuous.
    for (std::size_t rank = 0; rank < count; ++rank)
    {
        const std::uint32_t index = s.order[rank];
        const ActorPairCollisionActor& actor = actors[index];
        ActorPairCollisionOutput& out = outputs[index];
        if (actor.staticCollider)
            continue;

        if (out.supportKey != 0)
        {
            auto keyIt = std::lower_bound(s.keys.begin(), s.keys.end(), out.supportKey);
            if (keyIt == s.keys.end() || *keyIt != out.supportKey)
            {
                out.supportKey = 0;
                out.supportVelocity = glm::vec3(0.0f);
            }
            else
            {
                const std::size_t supportRank =
                    static_cast<std::size_t>(keyIt - s.keys.begin());
                const std::uint32_t supportIndex = s.order[supportRank];
                out.pos += outputs[supportIndex].pos - actors[supportIndex].prevPos;
                out.supportVelocity = outputs[supportIndex].vel;
                out.onGround = true;
                out.stableOnGround = true;
            }
        }

        if (out.supportKey == 0 && actor.previousSupportKey != 0)
            out.vel += actor.previousSupportVelocity;
    }

    const auto endTime = std::chrono::steady_clock::now();
    stats.narrowphaseMs = std::chrono::duration<float, std::milli>(
        endTime - broadphaseEnd).count();
    stats.totalMs = std::chrono::duration<float, std::milli>(
        endTime - startTime).count();

    if (outStats)
        *outStats = stats;
}
