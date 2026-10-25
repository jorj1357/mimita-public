// 2026-10-23
#include "gamemode/payload-runtime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glm/gtc/matrix_transform.hpp>

namespace MimitaGamemode {
namespace {

constexpr float kTickSeconds = 1.0f / 60.0f;
constexpr float kDistanceEpsilon = 0.0001f;

int overtimeTicksForSeconds(float seconds)
{
    return std::max(1, (int)std::lround(std::max(0.0f, seconds) * 60.0f));
}

glm::vec3 routeDirectionAt(const PayloadRuntime& runtime, float distance)
{
    if (runtime.routePoints.size() < 2)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    const float d = std::clamp(distance, 0.0f, runtime.totalDistance);
    size_t segment = 1;
    while (segment + 1 < runtime.cumulativeDistances.size() &&
           runtime.cumulativeDistances[segment] < d)
        ++segment;
    const glm::vec3 delta =
        runtime.routePoints[segment] - runtime.routePoints[segment - 1];
    const float length = glm::length(delta);
    if (length <= kDistanceEpsilon)
        return glm::vec3(1.0f, 0.0f, 0.0f);
    return delta / length;
}

bool sampleInsideBox(const glm::mat4& inverse,
                     const glm::vec3& half,
                     const glm::vec3& worldPoint,
                     float radius)
{
    const glm::vec3 local = glm::vec3(inverse * glm::vec4(worldPoint, 1.0f));
    const glm::vec3 closest(
        std::clamp(local.x, -half.x, half.x),
        std::clamp(local.y, -half.y, half.y),
        std::clamp(local.z, -half.z, half.z));
    return glm::length(local - closest) <= radius;
}

} // namespace

bool payloadRuntimeInit(PayloadRuntime& runtime,
                        const PayloadConfig& mapPayload,
                        const GamemodePayload& rules)
{
    runtime = PayloadRuntime{};
    if (!mapPayload.present || !mapPayload.hasCartOrigin)
        return false;
    if (mapPayload.route.size() < 2)
        return false;

    runtime.active = true;
    runtime.cartOrigin = mapPayload.cartOrigin;
    runtime.cartYaw = glm::radians(mapPayload.cartYaw);
    runtime.cartFixedFacing = mapPayload.cartFixedFacing;
    runtime.cartSize = glm::max(mapPayload.cartSize, glm::vec3(0.1f));
    runtime.interactionSize = glm::max(mapPayload.interactionSize, glm::vec3(0.1f));
    runtime.interactionOffset = mapPayload.interactionOffset;
    runtime.interactionRotationEuler = mapPayload.interactionRotationEuler;

    runtime.attackerTeam = std::max(0, rules.attackerTeamIndex);
    runtime.defenderTeam = std::max(0, rules.defenderTeamIndex);
    runtime.pushMetersPerSecondPerAttacker =
        std::max(0.0f, rules.pushMetersPerSecondPerAttacker);
    runtime.maxPushAttackers = std::max(1, rules.maxPushAttackers);
    runtime.unattendedRollbackDelaySeconds =
        std::max(0.0f, rules.unattendedRollbackDelaySeconds);
    runtime.rollbackMetersPerSecond = std::max(0.0f, rules.rollbackMetersPerSecond);
    runtime.overtimeSeconds = std::max(0.0f, rules.overtimeSeconds);
    runtime.healHpPer10Ticks = std::max(0, rules.healHpPer10Ticks);
    runtime.healDefenders = rules.healDefenders;

    std::vector<float> routeRadii;
    std::vector<int> routeWaitTicks;
    runtime.routePoints.reserve(mapPayload.route.size());
    runtime.cumulativeDistances.reserve(mapPayload.route.size());
    routeRadii.reserve(mapPayload.route.size());
    routeWaitTicks.reserve(mapPayload.route.size());
    for (const PayloadRoutePoint& point : mapPayload.route) {
        if (!point.enabled) continue;
        runtime.routePoints.push_back(point.position);
        routeRadii.push_back(std::max(0.1f, point.radius));
        routeWaitTicks.push_back(std::max(0, point.waitTicks));
    }
    if (runtime.routePoints.size() < 2)
        return false;

    runtime.cumulativeDistances.push_back(0.0f);
    for (size_t i = 1; i < runtime.routePoints.size(); ++i) {
        runtime.cumulativeDistances.push_back(
            runtime.cumulativeDistances.back() +
            glm::length(runtime.routePoints[i] - runtime.routePoints[i - 1]));
    }
    runtime.totalDistance = runtime.cumulativeDistances.back();
    if (runtime.totalDistance <= kDistanceEpsilon)
        return false;

    runtime.waypoints.reserve(runtime.routePoints.size());
    for (size_t i = 0; i < runtime.routePoints.size(); ++i) {
        PayloadWaypoint waypoint;
        waypoint.routeIndex = (int)i;
        waypoint.distance = runtime.cumulativeDistances[i];
        waypoint.radius = routeRadii[i];
        waypoint.waitTicks = routeWaitTicks[i];
        runtime.waypoints.push_back(waypoint);
    }

    // Route point order matches the filtered runtime route. Build the milestone
    // list from the authored flags, always treating the last point as final.
    size_t authoredIndex = 0;
    bool hasFinal = false;
    for (const PayloadRoutePoint& point : mapPayload.route) {
        if (!point.enabled) continue;
        const bool isLast = authoredIndex + 1 == runtime.routePoints.size();
        const bool mark = point.checkpoint || point.final || isLast;
        if (mark) {
            PayloadCheckpoint cp;
            cp.routeIndex = (int)authoredIndex;
            cp.distance = runtime.cumulativeDistances[authoredIndex];
            cp.radius = routeRadii[authoredIndex];
            cp.waitTicks = routeWaitTicks[authoredIndex];
            cp.timeBonusSeconds = point.final ? 0.0f : point.timeBonusSeconds;
            cp.final = point.final || (!hasFinal && isLast);
            if (cp.final) hasFinal = true;
            runtime.checkpoints.push_back(cp);
        }
        ++authoredIndex;
    }
    if (!hasFinal && !runtime.checkpoints.empty())
        runtime.checkpoints.back().final = true;

    payloadRuntimeReset(runtime);
    return true;
}

void payloadRuntimeReset(PayloadRuntime& runtime)
{
    runtime.distanceTravelled = 0.0f;
    runtime.lastCheckpointDistance = 0.0f;
    runtime.reachedCheckpoints.assign(runtime.checkpoints.size(), 0);
    runtime.overtimeTicksRemaining = 0;
    runtime.overtimeActive = false;
    runtime.finished = false;
    runtime.unattendedSeconds = 0.0f;
    runtime.waitTicksRemaining = 0;
    runtime.waiting = false;
    for (PayloadWaypoint& waypoint : runtime.waypoints)
        waypoint.reached = false;
    runtime.attackers = 0;
    runtime.defenders = 0;
    runtime.advancing = false;
    runtime.blocked = false;
    runtime.rollingBack = false;
    runtime.healAccumulators.clear();
    if (!runtime.checkpoints.empty() &&
        runtime.checkpoints.front().distance <= kDistanceEpsilon) {
        runtime.reachedCheckpoints.front() = 1;
        runtime.lastCheckpointDistance = runtime.checkpoints.front().distance;
    }
    payloadRuntimeUpdateTransforms(runtime);
    runtime.previousCartPosition = runtime.cartPosition;
    runtime.previousCartTransform = runtime.cartTransform;
    runtime.cartDelta = glm::vec3(0.0f);
}

glm::vec3 payloadRoutePositionAtDistance(const PayloadRuntime& runtime, float distance)
{
    if (runtime.routePoints.empty())
        return runtime.cartOrigin;
    if (runtime.routePoints.size() == 1)
        return runtime.cartOrigin;

    const float d = std::clamp(distance, 0.0f, runtime.totalDistance);
    size_t segment = 1;
    while (segment + 1 < runtime.cumulativeDistances.size() &&
           runtime.cumulativeDistances[segment] < d)
        ++segment;
    const float start = runtime.cumulativeDistances[segment - 1];
    const float end = runtime.cumulativeDistances[segment];
    const float span = std::max(end - start, kDistanceEpsilon);
    const float t = std::clamp((d - start) / span, 0.0f, 1.0f);
    const glm::vec3 routePos =
        runtime.routePoints[segment - 1] +
        (runtime.routePoints[segment] - runtime.routePoints[segment - 1]) * t;
    return runtime.cartOrigin +
           (routePos - runtime.routePoints.front());
}

void payloadRuntimeUpdateTransforms(PayloadRuntime& runtime)
{
    runtime.cartPosition = payloadRoutePositionAtDistance(runtime,
                                                          runtime.distanceTravelled);
    const glm::vec3 direction = routeDirectionAt(runtime, runtime.distanceTravelled);
    const float planarLength =
        std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (runtime.cartFixedFacing) {
        const glm::quat yawRotation = glm::angleAxis(
            runtime.cartYaw, glm::vec3(0.0f, 0.0f, 1.0f));
        runtime.cartOrientation = glm::normalize(yawRotation);
        runtime.cartTransform =
            glm::translate(glm::mat4(1.0f), runtime.cartPosition) *
            glm::mat4_cast(runtime.cartOrientation);
    } else {
        const float yaw = planarLength > kDistanceEpsilon
            ? std::atan2(direction.y, direction.x)
            : runtime.cartYaw;
        const float pitch = std::atan2(direction.z, std::max(planarLength, kDistanceEpsilon));

        const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::quat pitchRotation = glm::angleAxis(-pitch, glm::vec3(0.0f, 1.0f, 0.0f));
        runtime.cartOrientation = glm::normalize(yawRotation * pitchRotation);
        runtime.cartTransform =
            glm::translate(glm::mat4(1.0f), runtime.cartPosition) *
            glm::mat4_cast(runtime.cartOrientation);
    }

    glm::mat4 box = glm::translate(glm::mat4(1.0f), runtime.interactionOffset);
    box = glm::rotate(box, glm::radians(runtime.interactionRotationEuler.z),
                      glm::vec3(0.0f, 0.0f, 1.0f));
    box = glm::rotate(box, glm::radians(runtime.interactionRotationEuler.y),
                      glm::vec3(0.0f, 1.0f, 0.0f));
    box = glm::rotate(box, glm::radians(runtime.interactionRotationEuler.x),
                      glm::vec3(1.0f, 0.0f, 0.0f));
    runtime.interactionTransform = runtime.cartTransform * box;
}

bool payloadActorIntersectsInteractionBox(const PayloadRuntime& runtime,
                                          const PayloadActorView& actor)
{
    if (!runtime.active || !actor.alive || actor.team < 0)
        return false;
    if (runtime.interactionSize.x <= 0.0f || runtime.interactionSize.y <= 0.0f ||
        runtime.interactionSize.z <= 0.0f)
        return false;

    const glm::mat4 inverse = glm::inverse(runtime.interactionTransform);
    const glm::vec3 half = runtime.interactionSize * 0.5f;
    const float radius = std::max(0.05f, actor.radius);
    const float height = std::max(radius * 2.0f, actor.height);
    const float bottom = actor.position.z - height * 0.5f + radius;
    const float top = actor.position.z + height * 0.5f - radius;

    constexpr int kSamples = 5;
    for (int i = 0; i < kSamples; ++i) {
        const float t = kSamples == 1 ? 0.5f : (float)i / (float)(kSamples - 1);
        const glm::vec3 sample(actor.position.x, actor.position.y,
                               bottom + (top - bottom) * t);
        if (sampleInsideBox(inverse, half, sample, radius))
            return true;
    }
    return false;
}

bool payloadResolveActorAgainstCart(const PayloadRuntime& runtime,
                                    const glm::vec3& actorPos,
                                    const glm::vec3& actorVel,
                                    bool onGround,
                                    float radius,
                                    float height,
                                    glm::vec3& outPos,
                                    glm::vec3& outVel,
                                    bool& outOnGround)
{
    outPos = actorPos;
    outVel = actorVel;
    outOnGround = onGround;
    if (!runtime.active)
        return false;
    if (runtime.cartSize.x <= 0.0f || runtime.cartSize.y <= 0.0f ||
        runtime.cartSize.z <= 0.0f)
        return false;

    radius = std::max(0.05f, radius);
    height = std::max(radius * 2.0f, height);
    const glm::vec3 half = runtime.cartSize * 0.5f;
    constexpr float kSupportSlop = 0.35f;

    auto supportLocal = [&](const glm::mat4& basis, glm::vec3& outLocal) {
        const glm::mat4 inverse = glm::inverse(basis);
        const glm::vec3 local = glm::vec3(inverse * glm::vec4(actorPos, 1.0f));
        const float foot = local.z - height * 0.5f;
        if (std::abs(foot - half.z) > kSupportSlop ||
            std::abs(local.x) > half.x + radius ||
            std::abs(local.y) > half.y + radius)
            return false;
        outLocal = local;
        outLocal.z = half.z + height * 0.5f;
        return true;
    };

    glm::vec3 supportLocalPos;
    // Previous-pose support first: an actor that was standing on the cart must
    // be carried by the full delta, even when the current pose is still within
    // the support slop. New landings are handled by the current-pose fallback.
    if (supportLocal(runtime.previousCartTransform, supportLocalPos)) {
        outPos = glm::vec3(runtime.cartTransform * glm::vec4(supportLocalPos, 1.0f));
        if (outVel.z < 0.0f)
            outVel.z = 0.0f;
        outOnGround = true;
        return true;
    }
    // The cart may have moved further than the support slop in one tick (for
    // example a high-speed test or a long frame). Recover support against the
    // current pose so a newly landed actor is not left inside the cart.
    if (supportLocal(runtime.cartTransform, supportLocalPos)) {
        outPos = glm::vec3(runtime.cartTransform * glm::vec4(supportLocalPos, 1.0f));
        if (outVel.z < 0.0f)
            outVel.z = 0.0f;
        outOnGround = true;
        return true;
    }

    const glm::mat4 inverse = glm::inverse(runtime.cartTransform);
    glm::vec3 local = glm::vec3(inverse * glm::vec4(actorPos, 1.0f));
    const float foot = local.z - height * 0.5f;
    const float head = local.z + height * 0.5f;
    const bool overlapsX = std::abs(local.x) <= half.x + radius;
    const bool overlapsY = std::abs(local.y) <= half.y + radius;
    const bool overlapsZ = foot <= half.z && head >= -half.z;
    if (!overlapsX || !overlapsY || !overlapsZ)
        return false;

    const float pushX = (half.x + radius) - std::abs(local.x);
    const float pushY = (half.y + radius) - std::abs(local.y);
    if (pushX < pushY) {
        const float sign = local.x < 0.0f ? -1.0f : 1.0f;
        local.x = sign * (half.x + radius);
        outVel.x = 0.0f;
    } else {
        const float sign = local.y < 0.0f ? -1.0f : 1.0f;
        local.y = sign * (half.y + radius);
        outVel.y = 0.0f;
    }
    outPos = glm::vec3(runtime.cartTransform * glm::vec4(local, 1.0f));
    return true;
}

void payloadRuntimeTick(PayloadRuntime& runtime,
                        const PayloadActorView* actors,
                        int actorCount,
                        uint32_t tick,
                        uint32_t roundEndTick,
                        PayloadTickResult& result)
{
    result = PayloadTickResult{};
    if (!runtime.active || runtime.finished)
        return;

    runtime.cartDelta = glm::vec3(0.0f);
    const glm::vec3 oldCartPosition = runtime.cartPosition;
    const glm::mat4 oldCartTransform = runtime.cartTransform;

    runtime.attackers = 0;
    runtime.defenders = 0;
    for (int i = 0; i < actorCount; ++i) {
        const PayloadActorView& actor = actors[i];
        if (!payloadActorIntersectsInteractionBox(runtime, actor))
            continue;
        if (actor.team == runtime.attackerTeam)
            ++runtime.attackers;
        else if (actor.team == runtime.defenderTeam)
            ++runtime.defenders;
    }
    result.attackers = runtime.attackers;
    result.defenders = runtime.defenders;

    const float oldDistance = runtime.distanceTravelled;
    runtime.advancing = false;
    runtime.blocked = false;
    runtime.rollingBack = false;
    runtime.waiting = runtime.waitTicksRemaining > 0;

    if (runtime.waiting) {
        // Authored wait: the cart holds its current route distance. Attackers
        // inside the box still count for overtime below; they just cannot move
        // the cart until the pause expires.
        --runtime.waitTicksRemaining;
        runtime.unattendedSeconds = 0.0f;
    } else if (runtime.defenders > 0) {
        runtime.blocked = true;
        runtime.unattendedSeconds = 0.0f;
    } else if (runtime.attackers > 0) {
        runtime.advancing = true;
        runtime.unattendedSeconds = 0.0f;
        const int pushCount = std::min(runtime.attackers, runtime.maxPushAttackers);
        const float speed =
            (float)pushCount * runtime.pushMetersPerSecondPerAttacker;
        runtime.distanceTravelled += speed * kTickSeconds;
    } else {
        runtime.unattendedSeconds += kTickSeconds;
        if (runtime.unattendedSeconds >= runtime.unattendedRollbackDelaySeconds) {
            runtime.rollingBack = true;
            runtime.distanceTravelled -= runtime.rollbackMetersPerSecond * kTickSeconds;
        }
    }

    runtime.distanceTravelled = std::clamp(
        runtime.distanceTravelled, runtime.lastCheckpointDistance, runtime.totalDistance);
    result.distanceMoved = runtime.distanceTravelled - oldDistance;
    result.cartMoved = std::abs(result.distanceMoved) > kDistanceEpsilon;
    payloadRuntimeUpdateTransforms(runtime);
    runtime.previousCartPosition = oldCartPosition;
    runtime.previousCartTransform = oldCartTransform;
    runtime.cartDelta = runtime.cartPosition - oldCartPosition;

    // Route points have a real world-space arrival radius. Entering the sphere
    // triggers the point; `waitTicks` pauses the cart once on first entry.
    for (PayloadWaypoint& waypoint : runtime.waypoints) {
        if (waypoint.reached || waypoint.routeIndex < 0 ||
            waypoint.routeIndex >= (int)runtime.routePoints.size())
            continue;
        const glm::vec3 waypointWorld = payloadRoutePositionAtDistance(
            runtime, waypoint.distance);
        const float distanceToPoint = glm::distance(runtime.cartPosition, waypointWorld);
        if (distanceToPoint > waypoint.radius) continue;
        waypoint.reached = true;
        if (waypoint.waitTicks > 0) {
            runtime.waitTicksRemaining = waypoint.waitTicks;
            runtime.waiting = true;
            result.waitStarted = true;
        }
    }
    // Checkpoints are once-only. A final point ends the round; a checkpoint
    // extends the server clock by its configured bonus. The same authored radius
    // governs when the checkpoint triggers.
    for (size_t i = 0; i < runtime.checkpoints.size(); ++i) {
        if (runtime.reachedCheckpoints[i]) continue;
        const PayloadCheckpoint& cp = runtime.checkpoints[i];
        if (cp.routeIndex < 0 || cp.routeIndex >= (int)runtime.routePoints.size())
            continue;
        const glm::vec3 checkpointWorld = payloadRoutePositionAtDistance(
            runtime, cp.distance);
        const float distanceToPoint = glm::distance(runtime.cartPosition, checkpointWorld);
        if (distanceToPoint > cp.radius) continue;
        runtime.reachedCheckpoints[i] = 1;
        if (cp.waitTicks > 0 && runtime.waitTicksRemaining <= 0) {
            runtime.waitTicksRemaining = cp.waitTicks;
            runtime.waiting = true;
            result.waitStarted = true;
        }
        if (cp.final) {
            runtime.finished = true;
            runtime.advancing = false;
            runtime.blocked = false;
            runtime.rollingBack = false;
            runtime.overtimeActive = false;
            runtime.overtimeTicksRemaining = 0;
            result.finished = true;
            result.checkpointRouteIndex = cp.routeIndex;
            break;
        }
        result.checkpointReached = true;
        result.checkpointRouteIndex = cp.routeIndex;
        result.checkpointBonusSeconds += cp.timeBonusSeconds;
        // A radius may trigger before the authored distance. The stable rollback
        // boundary is where the cart actually was, never a forward snap.
        runtime.lastCheckpointDistance = std::max(
            runtime.lastCheckpointDistance,
            std::min(cp.distance, runtime.distanceTravelled));
        if (runtime.overtimeActive) {
            runtime.overtimeActive = false;
            runtime.overtimeTicksRemaining = 0;
            result.checkpointEndedOvertime = true;
        }
    }
    result.waitTicksRemaining = runtime.waitTicksRemaining;

    if (!runtime.finished && roundEndTick > 0) {
        if (!runtime.overtimeActive && tick >= roundEndTick) {
            runtime.overtimeActive = true;
            runtime.overtimeTicksRemaining = overtimeTicksForSeconds(runtime.overtimeSeconds);
            result.overtimeStarted = true;
        }
        if (runtime.overtimeActive) {
            if (runtime.attackers > 0) {
                runtime.overtimeTicksRemaining =
                    overtimeTicksForSeconds(runtime.overtimeSeconds);
                result.overtimeRefreshed = true;
            } else {
                --runtime.overtimeTicksRemaining;
                if (runtime.overtimeTicksRemaining <= 0) {
                    runtime.overtimeTicksRemaining = 0;
                    runtime.overtimeActive = false;
                    result.overtimeExpired = true;
                }
            }
        }
    }

    // Healing: eligible actors in the box gain 1 HP per 10 ticks. The
    // remainder is kept per actor so an interrupted cadence is not lost.
    if (runtime.healHpPer10Ticks > 0) {
        for (int i = 0; i < actorCount; ++i) {
            const PayloadActorView& actor = actors[i];
            if (!actor.alive) continue;
            const bool attacker = actor.team == runtime.attackerTeam;
            const bool defender = actor.team == runtime.defenderTeam;
            if (!attacker && !(runtime.healDefenders && defender)) continue;
            if (!payloadActorIntersectsInteractionBox(runtime, actor)) continue;
            float& accumulator = runtime.healAccumulators[actor.id];
            accumulator += 1.0f;
            if (accumulator >= 10.0f) {
                accumulator -= 10.0f;
                if (result.healedCount < (uint8_t)64)
                    result.healedIds[result.healedCount++] = actor.id;
            }
        }
    }

    // Drop dead/absent actors from the remainder map so ids do not accumulate.
    for (auto it = runtime.healAccumulators.begin();
         it != runtime.healAccumulators.end();) {
        bool found = false;
        for (int i = 0; i < actorCount; ++i) {
            if (actors[i].id == it->first) {
                found = actors[i].alive;
                break;
            }
        }
        if (!found) it = runtime.healAccumulators.erase(it);
        else ++it;
    }
}

bool payloadRuntimeSelfTest(std::string& report)
{
    bool ok = true;
    auto fail = [&](const std::string& why) {
        ok = false;
        report += "FAIL: " + why + "\n";
    };

    PayloadConfig map;
    map.present = true;
    map.hasCartOrigin = true;
    map.cartOrigin = glm::vec3(0.0f);
    map.cartSize = glm::vec3(4.0f, 3.0f, 2.0f);
    map.interactionSize = glm::vec3(6.0f, 4.5f, 3.0f);
    map.interactionOffset = glm::vec3(0.0f, 0.0f, 1.5f);
    map.route = {
        {"cp1", glm::vec3(0.0f), 6.0f, 0, 0, true, true, false, 0.0f},
        {"r1", glm::vec3(10.0f, 0.0f, 0.0f), 2.0f, 1, 0, true, false, false, 0.0f},
        {"cp2", glm::vec3(20.0f, 0.0f, 0.0f), 6.0f, 2, 0, true, true, false, 300.0f},
        {"cp3", glm::vec3(30.0f, 0.0f, 0.0f), 6.0f, 3, 0, true, true, true, 0.0f},
    };

    GamemodePayload rules;
    rules.configured = true;
    rules.attackerTeamIndex = 0;
    rules.defenderTeamIndex = 1;
    rules.pushMetersPerSecondPerAttacker = 0.2f;
    rules.maxPushAttackers = 6;
    rules.unattendedRollbackDelaySeconds = 30.0f;
    rules.rollbackMetersPerSecond = 0.2f;
    rules.overtimeSeconds = 10.0f;
    rules.healHpPer10Ticks = 1;

    PayloadRuntime runtime;
    if (!payloadRuntimeInit(runtime, map, rules))
        fail("runtime init failed");
    if (!runtime.active || runtime.totalDistance <= 0.0f)
        fail("runtime route should be active and nonzero");
    if (runtime.routePoints.size() != 4)
        fail("runtime should keep all route points");
    if (runtime.checkpoints.size() != 3)
        fail("runtime should keep three milestones");

    PayloadActorView attacker;
    attacker.id = 1;
    attacker.team = 0;
    attacker.alive = true;
    attacker.position = runtime.cartPosition;
    if (!payloadActorIntersectsInteractionBox(runtime, attacker))
        fail("actor at cart origin should intersect the interaction box");
    PayloadActorView farActor = attacker;
    farActor.position = glm::vec3(1000.0f);
    if (payloadActorIntersectsInteractionBox(runtime, farActor))
        fail("far actor should not intersect the interaction box");

    PayloadTickResult tickResult;
    payloadRuntimeReset(runtime);
    payloadRuntimeTick(runtime, &attacker, 1, 1, 0, tickResult);
    if (tickResult.attackers != 1)
        fail("one attacker should be counted");
    if (std::abs(tickResult.distanceMoved - 0.2f / 60.0f) > 0.0005f)
        fail("one attacker should move at 0.2 units/s");

    PayloadActorView attackers[6];
    for (int i = 0; i < 6; ++i) {
        attackers[i] = attacker;
        attackers[i].id = (uint32_t)(i + 1);
    }
    payloadRuntimeReset(runtime);
    payloadRuntimeTick(runtime, attackers, 6, 1, 0, tickResult);
    if (tickResult.attackers != 6)
        fail("six attackers should be counted");
    if (std::abs(tickResult.distanceMoved - 1.2f / 60.0f) > 0.0005f)
        fail("six attackers should move at the 1.2 units/s cap");

    PayloadActorView defender = attacker;
    defender.id = 20;
    defender.team = 1;
    payloadRuntimeReset(runtime);
    payloadRuntimeTick(runtime, &defender, 1, 1, 0, tickResult);
    if (tickResult.defenders != 1 || !runtime.blocked || tickResult.distanceMoved != 0.0f)
        fail("one defender should block forward progress");

    payloadRuntimeReset(runtime);
    runtime.unattendedSeconds = runtime.unattendedRollbackDelaySeconds;
    runtime.distanceTravelled = 5.0f;
    payloadRuntimeUpdateTransforms(runtime);
    payloadRuntimeTick(runtime, nullptr, 0, 1, 0, tickResult);
    if (!runtime.rollingBack || tickResult.distanceMoved >= 0.0f)
        fail("unattended cart should roll back after the delay");

    payloadRuntimeReset(runtime);
    runtime.distanceTravelled = 20.0f;
    runtime.unattendedSeconds = 0.0f;
    payloadRuntimeUpdateTransforms(runtime);
    payloadRuntimeTick(runtime, nullptr, 0, 1, 0, tickResult);
    if (!tickResult.checkpointReached || tickResult.checkpointBonusSeconds != 300.0f)
        fail("crossing a checkpoint should award its bonus once");
    payloadRuntimeTick(runtime, nullptr, 0, 2, 0, tickResult);
    if (tickResult.checkpointReached)
        fail("checkpoint bonus should not repeat");

    // Arrival radius is real: the cart triggers the point when it enters the
    // authored sphere, not only when it reaches the exact route distance.
    payloadRuntimeReset(runtime);
    runtime.distanceTravelled = 16.0f;
    payloadRuntimeUpdateTransforms(runtime);
    payloadRuntimeTick(runtime, nullptr, 0, 1, 0, tickResult);
    if (!tickResult.checkpointReached || tickResult.checkpointBonusSeconds != 300.0f)
        fail("checkpoint radius should trigger before the exact route distance");

    // waitTicks is real: the cart holds still for the authored pause.
    payloadRuntimeReset(runtime);
    runtime.checkpoints[1].waitTicks = 5;
    runtime.distanceTravelled = 20.0f;
    payloadRuntimeUpdateTransforms(runtime);
    payloadRuntimeTick(runtime, &attacker, 1, 1, 0, tickResult);
    if (!tickResult.waitStarted || runtime.waitTicksRemaining <= 0)
        fail("a checkpoint waitTicks pause should start on entry");
    const float distanceBeforeWait = runtime.distanceTravelled;
    payloadRuntimeTick(runtime, &attacker, 1, 2, 0, tickResult);
    if (!runtime.waiting ||
        std::abs(runtime.distanceTravelled - distanceBeforeWait) > 0.0001f)
        fail("the cart should hold its distance during waitTicks");

    // A non-checkpoint waypoint can also pause the cart.
    payloadRuntimeReset(runtime);
    runtime.waypoints[1].waitTicks = 3;
    runtime.distanceTravelled = 10.0f;
    payloadRuntimeUpdateTransforms(runtime);
    payloadRuntimeTick(runtime, nullptr, 0, 1, 0, tickResult);
    if (!tickResult.waitStarted || runtime.waitTicksRemaining != 3)
        fail("a non-checkpoint waypoint waitTicks should start on entry");

    payloadRuntimeReset(runtime);
    runtime.distanceTravelled = runtime.totalDistance;
    payloadRuntimeUpdateTransforms(runtime);
    payloadRuntimeTick(runtime, nullptr, 0, 1, 0, tickResult);
    if (!runtime.finished || !tickResult.finished)
        fail("reaching the final point should finish the round");

    payloadRuntimeReset(runtime);
    payloadRuntimeTick(runtime, &attacker, 1, 100, 100, tickResult);
    if (!runtime.overtimeActive || !tickResult.overtimeStarted)
        fail("timer expiry with an attacker should start overtime");
    if (!tickResult.overtimeRefreshed)
        fail("an attacker in the box should refresh overtime");
    runtime.overtimeTicksRemaining = 1;
    payloadRuntimeTick(runtime, nullptr, 0, 101, 100, tickResult);
    if (!tickResult.overtimeExpired)
        fail("overtime should expire without an attacker in the box");

    payloadRuntimeReset(runtime);
    runtime.healAccumulators[attacker.id] = 9.0f;
    payloadRuntimeTick(runtime, &attacker, 1, 1, 0, tickResult);
    if (tickResult.healedCount != 1 || tickResult.healedIds[0] != attacker.id)
        fail("ten healing ticks should heal the attacker once");

    payloadRuntimeReset(runtime);
    {
        glm::vec3 standPos(0.0f, 0.0f, 1.9f);
        glm::vec3 standVel(0.0f);
        glm::vec3 resolvedPos;
        glm::vec3 resolvedVel;
        bool resolvedGround = false;
        const bool hit = payloadResolveActorAgainstCart(
            runtime, standPos, standVel, false, 0.4f, 1.8f,
            resolvedPos, resolvedVel, resolvedGround);
        if (!hit || !resolvedGround ||
            std::abs(resolvedPos.z - 1.9f) > 0.001f)
            fail("actor on the cart top should be supported");

        runtime.cartDelta = glm::vec3(0.1f, 0.0f, 0.0f);
        runtime.previousCartTransform = runtime.cartTransform;
        runtime.cartTransform[3].x += 0.1f;
        runtime.cartPosition.x += 0.1f;
        const bool carried = payloadResolveActorAgainstCart(
            runtime, standPos, standVel, false, 0.4f, 1.8f,
            resolvedPos, resolvedVel, resolvedGround);
        if (!carried || std::abs(resolvedPos.x - 0.1f) > 0.001f)
            fail("cart movement should carry a supported actor");

        payloadRuntimeReset(runtime);
        glm::vec3 sidePos(2.2f, 0.0f, 0.9f);
        const bool pushed = payloadResolveActorAgainstCart(
            runtime, sidePos, standVel, false, 0.4f, 1.8f,
            resolvedPos, resolvedVel, resolvedGround);
        if (!pushed || std::abs(resolvedPos.x - 2.4f) > 0.001f)
            fail("actor inside the cart side should be pushed out");

        glm::vec3 farPos(100.0f, 0.0f, 0.0f);
        if (payloadResolveActorAgainstCart(runtime, farPos, standVel, false,
                                           0.4f, 1.8f, resolvedPos,
                                           resolvedVel, resolvedGround))
            fail("far actor should not collide with the cart");
    }

    report += ok ? "PASS\n" : "FAIL\n";
    return ok;
}

} // namespace MimitaGamemode
