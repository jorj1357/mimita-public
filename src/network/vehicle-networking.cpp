// 2026-11-03
/* purpose
* Implement server-authoritative vehicle networking: map-authored spawning,
* the shared fixed-tick vehicle simulation, seat requests, and replication.
* Does NOT add a second vehicle physics solver, render, or own client input
* prediction. Client mirrors never apply vehicle forces.
*/

#include "network/server.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "debug/structured-log.h"
#include "gamemode/arena-ball.h"
#include "gamemode/gamemode.h"
#include "gamemode/map-config.h"
#include "input/gamepad-input.h"
#include "input/input-router.h"
#include "network/multiplayer-context.h"
#include "network/server-gamemode.h"
#include "physics/movement/physics-collision.h"
#include "physics/physical-entity.h"
#include "physics/ray-utils.h"
#include "vehicles/vehicle-config.h"
#include "vehicles/vehicle-profile.h"
#include "vehicles/vehicle-system.h"
#include "void-death/void-death.h"
#include "world/world.h"

namespace MimitaNet {
namespace {

// Lift an authored vehicle spawn so every wheel starts with its resting contact
// patch just above the world surface. Without this, a roleplay/map spawn often
// puts the chassis inside a step or above a pit, and the car lands on its side
// before the wheel rays can establish contact.
glm::mat4 settleVehicleSpawnTransform(const World& world,
                                      const VehicleSpec& spec,
                                      glm::mat4 transform)
{
    const glm::quat orientation =
        glm::normalize(glm::quat_cast(glm::mat3(transform)));
    const glm::vec3 origin(transform[3]);
    float bestOriginZ = -std::numeric_limits<float>::infinity();
    bool found = false;
    std::vector<int> candidates;
    // Diagnostic: which world triangle the settle ray actually chose. A body
    // lifted metres above the authored spawn is otherwise indistinguishable
    // from a bad authored position.
    int bestWheel = -1;
    int bestTriangle = -1;
    float bestGroundZ = 0.0f;
    glm::vec3 bestTriangleCenter(0.0f);
    glm::vec3 bestTriangleNormal(0.0f);
    int wheelIndex = -1;
    for (const VehicleWheelSpec& wheel : spec.wheels)
    {
        ++wheelIndex;
        const glm::vec3 localHub = wheel.hub;
        const glm::vec3 hub = origin + orientation * localHub;
        const glm::vec3 start = hub + glm::vec3(0.0f, 0.0f, 8.0f);
        const glm::vec3 direction(0.0f, 0.0f, -1.0f);
        const float maxDistance = 20.0f;
        const glm::vec3 end = start + direction * maxDistance;
        AABB box;
        box.min = glm::min(start, end);
        box.max = glm::max(start, end);
        candidates.clear();
        appendChunkTrianglesForAABB(world, box, 0.0f, candidates,
                                    "vehicleSpawn");
        world.collisionTree.query(box, candidates);

        float nearest = maxDistance;
        bool hit = false;
        int hitIndex = -1;
        for (int index : candidates)
        {
            if (index < 0 ||
                index >= static_cast<int>(
                             world.collisionMesh.triangles.size()))
                continue;
            float distance = 0.0f;
            if (!rayTriangle(start, direction,
                             world.collisionMesh.triangles[index], distance))
                continue;
            if (distance < nearest)
            {
                nearest = distance;
                hit = true;
                hitIndex = index;
            }
        }
        if (!hit)
            continue;
        const float groundZ = start.z - nearest;
        const float wheelRestBottom =
            (orientation * localHub).z - wheel.restLength - wheel.radius;
        const float candidateOriginZ = groundZ - wheelRestBottom;
        if (candidateOriginZ > bestOriginZ)
        {
            bestOriginZ = candidateOriginZ;
            bestWheel = wheelIndex;
            bestTriangle = hitIndex;
            bestGroundZ = groundZ;
            if (hitIndex >= 0 &&
                hitIndex < static_cast<int>(
                               world.collisionMesh.triangles.size()))
            {
                const CollisionTriangle& triangle =
                    world.collisionMesh.triangles[hitIndex];
                bestTriangleCenter = (triangle.a + triangle.b + triangle.c) / 3.0f;
                bestTriangleNormal = triangle.normal;
            }
        }
        found = true;
    }
    if (found)
    {
        transform[3].z = bestOriginZ + 0.005f;
        const CollisionTriangle* triangle =
            bestTriangle >= 0 &&
                    bestTriangle < static_cast<int>(
                                        world.collisionMesh.triangles.size())
                ? &world.collisionMesh.triangles[bestTriangle]
                : nullptr;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.spawn.settle", spec.id,
            "vehicle spawn settle ray chose a ground surface", 0,
            nlohmann::json{
                {"template", spec.id},
                {"requested_z", origin.z},
                {"chosen_z", transform[3].z},
                {"ground_z", bestGroundZ},
                {"wheel", bestWheel},
                {"world_triangles", world.collisionMesh.triangles.size()},
                {"triangle_index", bestTriangle},
                {"triangle_center",
                 {bestTriangleCenter.x, bestTriangleCenter.y,
                  bestTriangleCenter.z}},
                {"triangle_normal",
                 {bestTriangleNormal.x, bestTriangleNormal.y,
                  bestTriangleNormal.z}},
                {"triangle", triangle
                     ? nlohmann::json{{"a", {triangle->a.x, triangle->a.y,
                                             triangle->a.z}},
                                      {"b", {triangle->b.x, triangle->b.y,
                                             triangle->b.z}},
                                      {"c", {triangle->c.x, triangle->c.y,
                                             triangle->c.z}}}
                     : nlohmann::json(nullptr)}},
            __FILE__, __LINE__, __FUNCTION__);
    }
    return transform;
}

const PhysicalEntity* findArenaBall()
{
    for (const PhysicalEntity& entity : PhysicalEntitySystem::instance().entities())
        if (entity.persistenceId == "arena-ball")
            return &entity;
    return nullptr;
}

// Free play shares the arena rules and the arena car, so both modes take the
// arena path. Kept in one place so a future arena variant does not need a new
// comparison at every call site.
bool isVehicleArenaMode(const std::string& modeId)
{
    return modeId == "vehicle_arena" || modeId == "vehicle_freeplay";
}

bool gamemodeAllowsScoreNpc(const std::string& modeId)
{
    const Gamemode& gm = GamemodeRegistry::instance().get(modeId);
    for (const GamemodeTeam& team : gm.teams)
        if (team.id == "orange" && team.npcs)
            return true;
    return false;
}

// Per-bot state for the arena probe controller.
struct ArenaBotState
{
    // A car with throttle applied that is not moving is pinned against
    // geometry; the only way out is to reverse, because a car cannot yaw at
    // zero speed.
    uint32_t stuckTicks = 0;
    uint32_t reverseTicks = 0;
    // Aerial program: jump, pitch the nose up, boost at the ball, air roll on
    // the way up, then dodge into it.
    bool aerial = false;
    uint32_t aerialTicks = 0;
    uint32_t rollsLeft = 0;
    bool aerialDodged = false;
    uint32_t aerialCooldown = 0;
    // Freestyle program: pop the ball up, fly above it, touch it with all four
    // wheels (flip reset), then dodge it at the goal.
    int freestylePhase = 0;   // 0 idle, 1 pop, 2 track, 3 reset, 4 shot
    uint32_t freestyleTicks = 0;
    uint32_t freestyleCooldown = 0;
    uint32_t resetsAtStart = 0;
    // The jump impulse is held for several ticks exactly like the aerial
    // program. One tick gave a weak pop and the car peaked below the ball.
    uint32_t freestyleJumpTicks = 0;
};
std::unordered_map<uint32_t, ArenaBotState> sArenaBotState;

// The ball is "in the air" for scoring evidence at this height above the floor.
constexpr float kAerialBallHeightM = 2.0f;

// The car's real world position. VehicleState::origin is still the zero vector
// before the first physics tick, so seating decisions made from it tied every
// car at the arena centre and the first bot could take the wrong team's car.
glm::vec3 arenaVehiclePosition(const VehicleState& vehicle)
{
    const PhysicalEntity* chassis =
        PhysicalEntitySystem::instance().find(vehicle.entityId);
    if (chassis)
        return glm::vec3(chassis->transform[3]);
    return vehicle.origin;
}

uint32_t arenaTeamOfVehicle(const VehicleState& vehicle)
{
    const uint32_t driver =
        static_cast<uint32_t>(std::max(0, vehicle.driverActorId));
    const ServerGamemodeState& d = serverGamemodeState();
    auto it = d.communityTeams.find(driver);
    if (it != d.communityTeams.end() && it->second >= 0)
        return static_cast<uint32_t>(it->second);
    return 0;
}

// Point the nose at a world-space target and boost along it. This is the one
// aerial steering law: rate-controlled pitch/yaw from the body-frame direction,
// boost when aligned, so aerials, saves and the freestyle share it.
VehicleInput flyAtTarget(const PhysicalEntity& chassis, const glm::vec3& target,
                         float boostAbove)
{
    VehicleInput air;
    const glm::vec3 carPosition(chassis.transform[3]);
    const glm::vec3 delta = target - carPosition;
    const float distance = glm::length(delta);
    if (distance < 1e-3f)
        return air;
    const glm::vec3 toTarget = delta / distance;
    const glm::mat3 bodyToWorld(chassis.orientation);
    const glm::vec3 local = glm::transpose(bodyToWorld) * toTarget;
    const glm::vec3 rightAxis = bodyToWorld * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 upAxis = bodyToWorld * glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 nose = glm::normalize(bodyToWorld * glm::vec3(1.0f, 0.0f, 0.0f));
    const float pitchError = std::atan2(local.z, std::max(0.08f, local.x));
    const float yawAim = std::atan2(-local.y, std::max(0.08f, local.x));
    const float wPitch = glm::dot(chassis.angularVelocity, rightAxis);
    const float wYaw = glm::dot(chassis.angularVelocity, upAxis);
    const float pitchRate = std::clamp(-pitchError * 4.0f, -3.4f, 3.4f);
    const float yawRate = std::clamp(yawAim * 3.0f, -2.5f, 2.5f);
    air.pitch = std::clamp((pitchRate - wPitch) * 3.0f, -1.0f, 1.0f);
    air.yaw = std::clamp((yawRate - wYaw) * 3.0f, -1.0f, 1.0f);
    const float aim = glm::dot(nose, toTarget);
    // Hold a low boost even while the nose is still turning. The aerial
    // program uses the same 0.25 floor; without it the freestyle car jumped to
    // 1.74 m and then fell because it only boosted when already aligned.
    air.boost = (aim > boostAbove && distance > 1.0f)
                    ? 1.0f
                    : (distance > 2.5f ? 0.25f : 0.0f);
    // Arrive instead of sailing past: near the target, reverse thrust along the
    // nose. Without this the car overshot to 7 m above a 1 m ball and never
    // came back inside the reset tolerance (run 20261126_231754).
    const float closing = glm::dot(chassis.velocity, toTarget);
    if (distance < 5.0f && closing > 2.0f)
    {
        air.boost = 0.0f;
        air.brake = std::clamp((closing - 2.0f) / 6.0f, 0.0f, 1.0f);
    }
    return air;
}

void driveArenaNpcCar(VehicleState& vehicle, const PhysicalEntity& ball,
                      const glm::vec3& targetGoal,
                      const glm::vec3& ownGoalCenter)
{
    const PhysicalEntity* chassis =
        PhysicalEntitySystem::instance().find(vehicle.entityId);
    if (!chassis)
        return;
    const glm::vec3 carPosition(chassis->transform[3]);
    const glm::vec3 ballPosition(ball.transform[3]);
    const uint32_t now = PhysicalEntitySystem::instance().simulationTick();

    // Approach from the side of the ball that faces the driver's own goal, so
    // driving through the ball sends it toward the opponent net instead of
    // nudging it back. This is the same input surface a human uses.
    glm::vec3 ownGoalToBall = ballPosition - targetGoal;
    ownGoalToBall.z = 0.0f;
    if (glm::dot(ownGoalToBall, ownGoalToBall) <= 1e-4f)
        ownGoalToBall = glm::vec3(1.0f, 0.0f, 0.0f);
    // travelDir points from the ball at the target goal. The car must arrive
    // on the OPPOSITE side of the ball, so driving through it sends the ball
    // along travelDir. The previous sign put the aim point past the ball, so
    // the bot approached from the goal side and pushed the ball away from it.
    const glm::vec3 travelDir = glm::normalize(-ownGoalToBall);
    glm::vec3 aimPoint = ballPosition - travelDir * 1.2f;
    aimPoint.z = carPosition.z;

    // Defending: the ball is close to this car's own goal and travelling at
    // it. The shot must be met, so the aim is the ball's own predicted
    // position instead of the "get behind it" point, and an aerial is allowed
    // from a lower ball.
    const glm::vec3 toOwnGoal = ownGoalCenter - ballPosition;
    const float ownGoalDistance = glm::length(toOwnGoal);
    const float ballSpeed = glm::length(ball.velocity);
    const bool defending =
        ownGoalDistance < 24.0f && ballSpeed > 3.0f &&
        glm::dot(ball.velocity / ballSpeed,
                 toOwnGoal / std::max(ownGoalDistance, 0.01f)) > 0.7f;
    if (defending)
    {
        aimPoint = ballPosition + ball.velocity * 0.18f;
        aimPoint.z = carPosition.z;
    }

    glm::vec3 toAim = aimPoint - carPosition;
    toAim.z = 0.0f;
    const float distance = glm::length(toAim);
    if (distance <= 1e-4f)
        return;
    const glm::vec3 forward = chassis->orientation * glm::vec3(1.0f, 0.0f, 0.0f);
    const float currentYaw = std::atan2(forward.y, forward.x);
    const float desiredYaw = std::atan2(toAim.y, toAim.x);
    const float yawError = std::remainder(desiredYaw - currentYaw,
                                          glm::two_pi<float>());

    VehicleInput input;
    input.steer = std::clamp(yawError * 1.8f, -1.0f, 1.0f);
    // A car cannot spin in place and driving forward when the target is behind
    // only pushes it further into the wall. Back toward it instead: reverse is
    // drive = -1, so brake with no throttle. Between the two cases, ease off
    // the throttle so a big correction does not overshoot.
    const bool targetBehind = std::fabs(yawError) > 2.0f;
    input.throttle = targetBehind ? 0.0f
                                  : (std::fabs(yawError) > 1.2f ? 0.55f : 1.0f);
    input.brake = targetBehind ? 1.0f : 0.0f;
    input.clutch = 0.0f;
    input.handbrake = 0.0f;
    // Boost when pointed at the ball and still some distance out.
    const float goalDistance = glm::length(ballPosition - targetGoal);
    input.boost = (!targetBehind && std::fabs(yawError) < 0.35f &&
                   goalDistance > 4.0f && vehicle.boostCharge > 5.0f)
                      ? 1.0f
                      : 0.0f;
    // Jump when the ball is close and above the chassis, so the NPC can head it.
    const float ballHeight = ballPosition.z - carPosition.z;
    input.jumpHeld = (goalDistance < 2.2f && ballHeight > 0.9f) ? true : false;

    // Stuck recovery: a car that wants to move and cannot is pinned nose-first
    // against a wall or goal frame, where steering cannot rotate it (yaw needs
    // speed). Reverse out, steering the opposite way to unwind the heading.
    ArenaBotState& bot = sArenaBotState[vehicle.id];
    {
        ArenaBotState& recovery = bot;
        const float speed = glm::length(chassis->velocity);
        if (recovery.reverseTicks > 0)
        {
            --recovery.reverseTicks;
            input.throttle = 0.0f;
            input.brake = 1.0f;
            input.boost = 0.0f;
            input.steer = yawError >= 0.0f ? -0.9f : 0.9f;
        }
        else if (speed < 0.8f && std::fabs(yawError) > 0.5f)
        {
            if (++recovery.stuckTicks > 45)
            {
                recovery.stuckTicks = 0;
                recovery.reverseTicks = 55;
            }
        }
        else
        {
            recovery.stuckTicks = 0;
        }
    }
    // ── Aerial program ──────────────────────────────────────────────────
    // A ball above the car that it can reach with boost turns this bot into an
    // aerial: jump, pitch the nose at the ball, boost, air roll on the way up,
    // then dodge through it. The same program is the save: the ground aim point
    // already puts the bot on the defensive side of the ball, so a ball falling
    // toward its own goal is intercepted from the goal side.
    if (bot.aerialCooldown > 0)
        --bot.aerialCooldown;
    const float ballAbove = ballPosition.z - carPosition.z;
    const float flatDistance = glm::length(
        glm::vec2(ballPosition.x - carPosition.x, ballPosition.y - carPosition.y));
    const float aerialGate =
        defending ? 1.1f : kAerialBallHeightM;
    // Attacking aerials only from behind the ball relative to the target goal.
    // Flying at the ball from the goal side headed it into the bot's own net
    // (run 20261126_195943: the 1-1 goal was an own goal by the scoring bot).
    const bool behindBall =
        glm::dot(carPosition - ballPosition, travelDir) < 0.5f;
    if (!bot.aerial && bot.aerialCooldown == 0 && carPosition.z < 0.8f &&
        ballAbove > aerialGate && flatDistance < 18.0f &&
        vehicle.boostCharge > 12.0f && (defending || behindBall))
    {
        bot.aerial = true;
        bot.aerialTicks = 0;
        bot.rollsLeft = 14;
        bot.aerialDodged = false;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.npc.aerial", std::to_string(vehicle.id),
            "bot started an aerial", now,
            {{"phase", "start"},
             {"vehicle_id", vehicle.id},
             {"ball_height_m", ballPosition.z},
             {"ball_above_m", ballAbove},
             {"flat_distance_m", flatDistance},
             {"boost_charge", vehicle.boostCharge}});
    }

    if (bot.aerial)
    {
        ++bot.aerialTicks;
        VehicleInput air;
        if (bot.aerialTicks <= 6)
        {
            // Plant the car, then jump. Holding throttle through the jump
            // wastes the first tick of climb.
            air.jumpHeld = true;
            air.throttle = 0.0f;
            air.brake = 1.0f;
            air.steer = input.steer;
        }
        else
        {
            const glm::vec3 aimBall =
                ballPosition + ball.velocity * 0.25f - carPosition;
            const float aimLength = glm::length(aimBall);
            const glm::vec3 toBall =
                aimLength > 1e-3f ? aimBall / aimLength : glm::vec3(1.0f, 0.0f, 0.0f);
            const glm::mat3 bodyToWorld(chassis->orientation);
            const glm::vec3 local = glm::transpose(bodyToWorld) * toBall;
            const glm::vec3 nose = glm::normalize(
                bodyToWorld * glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::vec3 rightAxis = bodyToWorld * glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 upAxis = bodyToWorld * glm::vec3(0.0f, 0.0f, 1.0f);
            // +pitch pitches the nose down, +yaw turns it to screen right
            // (car-local -y), so both errors are negated for the local frame.
            // The body rate is part of the law: a pure proportional angle
            // controller oscillated (measured pitch input -0.99, +0.82, -0.4
            // inside one climb) and never settled on the ball.
            const float pitchError = std::atan2(local.z, std::max(0.08f, local.x));
            const float yawAim = std::atan2(-local.y, std::max(0.08f, local.x));
            const float wPitch = glm::dot(chassis->angularVelocity, rightAxis);
            const float wYaw = glm::dot(chassis->angularVelocity, upAxis);
            // Rate controller, not an angle controller: the input drives the
            // body rate toward a rate that is proportional to the angle error.
            // An angle-only law overshot the target every time (measured pitch
            // input -1.0, +0.88, -1.0 within one climb) and the nose never
            // pointed at the ball long enough to boost at it.
            // A positive pitch rate is nose DOWN, so a ball above the nose
            // (positive pitchError) must command a negative rate. Getting this
            // sign wrong pointed the nose at the floor and the car never left
            // 2 m: measured pitch input +1.0 and a 1.96 m apex.
            const float pitchRateTarget =
                std::clamp(-pitchError * 4.0f, -3.4f, 3.4f);
            const float yawRateTarget =
                std::clamp(yawAim * 3.0f, -2.5f, 2.5f);
            air.pitch = std::clamp((pitchRateTarget - wPitch) * 3.0f, -1.0f, 1.0f);
            air.yaw = std::clamp((yawRateTarget - wYaw) * 3.0f, -1.0f, 1.0f);
            air.roll = 0.0f;
            if (bot.rollsLeft > 0 && bot.aerialTicks > 10 && aimLength > 10.0f)
            {
                // Air roll on the climb, only while the ball is far enough that
                // the roll cannot spoil the final approach.
                air.roll = -1.0f;
                --bot.rollsLeft;
            }
            const float aim = glm::dot(nose, toBall);
            air.boost = aim > 0.5f ? 1.0f : 0.25f;
            // The dodge must happen inside the double-jump window (1.25 s after
            // the jump). Waiting until the ball was 3.2 m away missed it: the
            // car was already past the window and the second press did nothing
            // (0 vehicle.dodge events in five aerial runs).
            if (!bot.aerialDodged && bot.aerialTicks > 12 &&
                bot.aerialTicks < 70 && aimLength < 6.0f && aim > 0.45f &&
                carPosition.z > 1.0f)
            {
                air.jumpHeld = true;
                air.boost = 1.0f;
                bot.aerialDodged = true;
            }
        }
        const bool landed =
            carPosition.z < 0.7f && bot.aerialTicks > 30;
        const bool spent = bot.aerialTicks > 210;
        if (landed || spent)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.npc.aerial", std::to_string(vehicle.id),
                "bot finished its aerial", now,
                {{"phase", "end"},
                 {"vehicle_id", vehicle.id},
                 {"reason", landed ? "landed" : "timeout"},
                 {"ticks", bot.aerialTicks},
                 {"dodged", bot.aerialDodged},
                 {"car_height_m", carPosition.z},
                 {"ball_height_m", ballPosition.z}});
            bot.aerial = false;
            bot.aerialTicks = 0;
            bot.aerialCooldown = 120;
        }
        else
        {
            // An aerial owns the car's input for its whole duration.
            input = air;
        }
    }
    // ── Freestyle program ───────────────────────────────────────────────
    // Pop the ball up, fly above it, let all four wheels touch it (the reset
    // re-arms the flip), then dodge it at the goal. Every step is the normal
    // input surface; the physics decides whether the reset and the goal happen.
    if (bot.freestyleCooldown > 0)
        --bot.freestyleCooldown;
    if (bot.freestylePhase == 0 && bot.freestyleCooldown == 0 && !bot.aerial &&
        behindBall && !defending && flatDistance < 26.0f &&
        vehicle.boostCharge > 35.0f && ballPosition.z < 4.0f &&
        carPosition.z < 0.9f)
    {
        // A ball already in the air needs no pop: go straight to the climb.
        bot.freestylePhase = ballPosition.z > 1.6f ? 2 : 1;
        bot.freestyleTicks = 0;
        bot.freestyleJumpTicks = 0;
        bot.aerialDodged = false;
        bot.resetsAtStart = static_cast<uint32_t>(vehicle.flipResetCount);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.npc.freestyle", std::to_string(vehicle.id),
            "bot started a freestyle", now,
            {{"phase", "start"},
             {"vehicle_id", vehicle.id},
             {"ball_height_m", ballPosition.z},
             {"flat_distance_m", flatDistance},
             {"boost_charge", vehicle.boostCharge},
             {"flip_resets", vehicle.flipResetCount}});
    }
    if (bot.freestylePhase != 0)
    {
        ++bot.freestyleTicks;
        VehicleInput fs;
        const glm::vec3 aboveBall =
            ballPosition + glm::vec3(0.0f, 0.0f, 0.98f);
        switch (bot.freestylePhase)
        {
        case 1: // pop: close the last metres and jump into the ball
            if (flatDistance > 2.4f)
            {
                fs = input;
            }
            else
            {
                fs = flyAtTarget(*chassis, ballPosition +
                                              glm::vec3(0.0f, 0.0f, 0.7f),
                                 0.35f);
                fs.jumpHeld = true;
            }
            if (ballPosition.z > 1.8f || bot.freestyleTicks > 100)
                bot.freestylePhase = 2;
            break;
        case 2: // track: climb above the ball
            if (bot.freestyleJumpTicks < 6 && carPosition.z < 0.9f)
            {
                // The ball is already airborne, so the pop phase was skipped.
                // A ground car cannot fly: hold the jump for the same six
                // ticks the aerial program uses. A single tick produced a weak
                // pop (car apex 2.01 m against a 2.44 m ball top).
                fs.jumpHeld = true;
                fs.throttle = 0.0f;
                fs.brake = 1.0f;
                ++bot.freestyleJumpTicks;
            }
            else
            {
                fs = flyAtTarget(*chassis,
                                 aboveBall + glm::vec3(0.0f, 0.0f, 0.6f),
                                 0.35f);
            }
            if (carPosition.z > ballPosition.z + 0.35f)
                bot.freestylePhase = 3;
            break;
        case 3: // reset: settle onto the ball until the wheels touch it
            fs = flyAtTarget(*chassis, aboveBall, 0.2f);
            fs.boost = glm::dot(
                           glm::normalize(chassis->orientation *
                                          glm::vec3(1.0f, 0.0f, 0.0f)),
                           glm::normalize(aboveBall - carPosition)) > 0.9f
                           ? 0.35f
                           : 0.0f;
            // Close enough to touch: dodge through it. The wheels cross the
            // reset tolerance during the dodge, which is how a flip reset works
            // in play - waiting to settle exactly above the ball never
            // converged (run 20261126_231531: 5 s of chase, 0 resets).
            if (vehicle.flipResetCount > static_cast<int>(bot.resetsAtStart) ||
                glm::length(ballPosition - carPosition) < 2.4f)
                bot.freestylePhase = 4;
            break;
        case 4: // shot: dodge through the ball toward the goal
            fs = flyAtTarget(*chassis, ballPosition + travelDir * 0.4f, 0.4f);
            fs.boost = 1.0f;
            if (!bot.aerialDodged)
            {
                fs.jumpHeld = true;
                bot.aerialDodged = true;
            }
            break;
        default:
            break;
        }
        const bool done = bot.freestylePhase == 4 && bot.freestyleTicks > 40;
        const bool timeout = bot.freestyleTicks > 320;
        const bool landed = carPosition.z < 0.7f && bot.freestyleTicks > 60;
        if (done || timeout || landed)
        {
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "vehicle.npc.freestyle", std::to_string(vehicle.id),
                "bot finished its freestyle", now,
                {{"phase", "end"},
                 {"vehicle_id", vehicle.id},
                 {"reason", done ? "shot" : (landed ? "landed" : "timeout")},
                 {"ticks", bot.freestyleTicks},
                 {"flip_resets", vehicle.flipResetCount},
                 {"resets_this_run", vehicle.flipResetCount -
                                         static_cast<int>(bot.resetsAtStart)},
                 {"car_height_m", carPosition.z},
                 {"ball_height_m", ballPosition.z}});
            bot.freestylePhase = 0;
            bot.freestyleTicks = 0;
            bot.aerialDodged = false;
            bot.freestyleCooldown = 180;
        }
        else
        {
            input = fs;
        }
    }

    // ── Self-right ──────────────────────────────────────────────────────
    // A car on its side or roof cannot drive. A player air-rolls back onto the
    // wheels, and the bot uses the same air-control inputs: the world up axis
    // in the car's own frame says which way the body must rotate. Without this
    // a car that landed on its side jittered between grounded and airborne for
    // the rest of the match (run 20261126_195242: 400 landings, 3 on wheels,
    // and the car sat at up_dot -0.2 from 13 s while the ball went dead).
    {
        const glm::vec3 chassisUp =
            chassis->orientation * glm::vec3(0.0f, 0.0f, 1.0f);
        if (!bot.aerial && bot.freestylePhase == 0 && chassisUp.z < 0.75f)
        {
            const glm::vec3 worldUpInBody =
                glm::transpose(glm::mat3(chassis->orientation)) *
                glm::vec3(0.0f, 0.0f, 1.0f);
            input.roll = std::clamp(-worldUpInBody.y * 3.0f, -1.0f, 1.0f);
            input.pitch = std::clamp(-worldUpInBody.x * 3.0f, -1.0f, 1.0f);
            input.yaw = 0.0f;
            input.throttle = 0.0f;
            input.brake = 0.0f;
            input.boost = 0.0f;
            input.jumpHeld = false;
        }
    }
    VehicleSystem::instance().setInput(vehicle.id, input);

    // Bot aim diagnostic (1 Hz). A bot that never reaches the ball looks
    // identical to a broken ball or a broken score; this records the actual
    // geometry the controller saw.
    if (now % 60u == 0u)
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "vehicle.npc.aim", std::to_string(vehicle.id),
            "bot steering toward the ball", now,
            {{"vehicle_id", vehicle.id},
             {"distance_m", distance},
             {"yaw_error_rad", yawError},
             {"steer", input.steer},
             {"throttle", input.throttle},
             {"boost", input.boost},
             {"speed_mps", glm::length(chassis->velocity)},
             {"car", {carPosition.x, carPosition.y, carPosition.z}},
             {"ball", {ballPosition.x, ballPosition.y, ballPosition.z}},
             {"aim", {aimPoint.x, aimPoint.y, aimPoint.z}},
             {"goal", {targetGoal.x, targetGoal.y, targetGoal.z}}});
    }
}

int arenaTeamIndex(const std::string& team)
{
    if (team == "blue")
        return 0;
    if (team == "orange")
        return 1;
    return -1;
}

const ArenaKickoffConfig* arenaKickoffForTeam(const MapObjectiveConfig& map,
                                              int team, uint32_t ordinal)
{
    std::vector<const ArenaKickoffConfig*> matches;
    for (const ArenaKickoffConfig& kickoff : map.arena.kickoffs)
    {
        if (!kickoff.enabled)
            continue;
        if (arenaTeamIndex(kickoff.team) == team)
            matches.push_back(&kickoff);
    }
    if (matches.empty())
        return nullptr;
    return matches[ordinal % matches.size()];
}

void resetVehicleToKickoff(VehicleState& vehicle, const glm::vec3& position,
                           float yaw)
{
    PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(vehicle.entityId);
    if (!entity)
        return;
    entity->transform =
        glm::translate(glm::mat4(1.0f), position) *
        glm::mat4_cast(glm::angleAxis(yaw, glm::vec3(0.0f, 0.0f, 1.0f)));
    entity->previousTransform = entity->transform;
    entity->orientation =
        glm::normalize(glm::quat_cast(glm::mat3(entity->transform)));
    entity->velocity = glm::vec3(0.0f);
    entity->angularVelocity = glm::vec3(0.0f);
    entity->sleeping = false;
    entity->sleepTicks = 0;
    vehicle.origin = position;
    vehicle.originVelocity = glm::vec3(0.0f);
    vehicle.originAngularVelocity = glm::vec3(0.0f);
    vehicle.orientation = entity->orientation;
    vehicle.jumpCount = 0;
    vehicle.jumpHeld = false;
    vehicle.jumpHeldPrev = false;
    if (auto profile = VehicleSystem::instance().profileFor(vehicle))
        vehicle.boostCharge = profile->boost.max;
}

void resetArenaAfterGoal(
    ServerVehicleState& vehicleState,
    const std::unordered_map<uint32_t, ServerPlayer>& players,
    const MapObjectiveConfig& map, PhysicalEntity& ball, uint32_t tick,
    int scoringTeam)
{
    ArenaBall::reset(ball, map);
    ServerGamemodeState& d = serverGamemodeState();
    uint32_t blueOrdinal = 0;
    uint32_t orangeOrdinal = 0;
    for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
    {
        if (!vehicle.serverAuthoritative || vehicle.serverMirror)
            continue;
        int team = 1;
        const uint32_t driverId =
            static_cast<uint32_t>(std::max(0, vehicle.driverActorId));
        auto teamIt = d.communityTeams.find(driverId);
        if (teamIt != d.communityTeams.end() && teamIt->second >= 0)
            team = teamIt->second;
        const uint32_t ordinal =
            team == 0 ? blueOrdinal++ : orangeOrdinal++;
        const ArenaKickoffConfig* kickoff =
            arenaKickoffForTeam(map, team, ordinal);
        if (!kickoff)
            continue;
        resetVehicleToKickoff(vehicle, kickoff->position, kickoff->yaw);
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "arena.goal.reset", "arena", "arena goal reset ball and cars", tick,
        {{"scoring_team", scoringTeam},
         {"ball_network_id", static_cast<int>(ball.networkId)},
         {"ball_position", {ball.transform[3].x, ball.transform[3].y,
                            ball.transform[3].z}},
         {"blue_score", d.communityTeamScore[0]},
         {"orange_score", d.communityTeamScore[1]}});
}

// A save is a car touch that turns the ball away from that car's own goal while
// the ball was on its way in. It is computed from the ball's real previous
// velocity and the real touch, so it cannot be declared by a script.
void tickArenaSave(ServerVehicleState& vehicleState,
                   const MapObjectiveConfig& map, uint32_t tick)
{
    PhysicalEntity* ball = nullptr;
    for (PhysicalEntity& entity : PhysicalEntitySystem::instance().entities())
        if (entity.persistenceId == "arena-ball")
        {
            ball = &entity;
            break;
        }
    if (!ball)
        return;
    const glm::vec3 position(ball->transform[3]);
    const glm::vec3 velocity(ball->velocity);
    const uint32_t toucher = ball->lastTouchedVehicleNetworkId;
    const float speedBefore = glm::length(vehicleState.arenaPrevBallVelocity);

    if (toucher != 0 && toucher != vehicleState.arenaPrevBallToucher &&
        speedBefore > 5.0f)
    {
        for (const VehicleState& vehicle :
             VehicleSystem::instance().vehiclesMutable())
        {
            if (vehicle.chassisNetworkId != toucher)
                continue;
            const uint32_t team = arenaTeamOfVehicle(vehicle);
            const char* ownTeamName = team == 0 ? "blue" : "orange";
            const ArenaGoalConfig* ownGoal = nullptr;
            for (const ArenaGoalConfig& goal : map.arena.goals)
                if (goal.enabled && goal.team == ownTeamName)
                {
                    ownGoal = &goal;
                    break;
                }
            if (!ownGoal)
                break;
            const glm::vec3 toGoal = ownGoal->center - position;
            const float goalDistance = glm::length(toGoal);
            if (goalDistance > 16.0f)
                break;
            const glm::vec3 toGoalDir = toGoal / goalDistance;
            const glm::vec3 incoming =
                vehicleState.arenaPrevBallVelocity / speedBefore;
            const float aimed = glm::dot(incoming, toGoalDir);
            const float speedAfter = glm::length(velocity);
            const glm::vec3 out =
                speedAfter > 0.1f ? velocity / speedAfter : glm::vec3(0.0f);
            const float stillIn = glm::dot(out, toGoalDir);
            if (aimed > 0.55f && stillIn < 0.35f)
            {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "arena.save", ownGoal->id,
                    "car turned the ball away from its own goal", tick,
                    {{"goal_id", ownGoal->id},
                     {"team", static_cast<int>(team)},
                     {"vehicle_id", vehicle.id},
                     {"driver_id", vehicle.driverActorId},
                     {"ball_speed_before_mps", speedBefore},
                     {"ball_speed_after_mps", speedAfter},
                     {"distance_to_goal_m", goalDistance},
                     {"aimed_at_goal", aimed},
                     {"still_at_goal", stillIn},
                     {"ball_height_m", position.z},
                     {"aerial", ball->lastTouchedHeightM > 2.0f},
                     {"touch_height_m", ball->lastTouchedHeightM},
                     {"server_tick", tick}});
            }
            break;
        }
    }
    vehicleState.arenaPrevBallVelocity = velocity;
    vehicleState.arenaPrevBallToucher = toucher;
}

void tickArenaGoals(ServerVehicleState& vehicleState,
                    const std::unordered_map<uint32_t, ServerPlayer>& players,
                    const MapObjectiveConfig& map, uint32_t tick)
{
    if (!map.arena.present)
        return;
    ServerGamemodeState& arenaState = serverGamemodeState();
    if (arenaState.matchOver)
    {
        // The match is over (score limit or time limit). Keep the ball alive so
        // the scene stays valid, but a goal after the whistle does not count.
        ArenaBall::tick();
        return;
    }
    ArenaBall::tick();
    PhysicalEntity* ball = nullptr;
    for (PhysicalEntity& entity : PhysicalEntitySystem::instance().entities())
        if (entity.persistenceId == "arena-ball")
        {
            ball = &entity;
            break;
        }
    if (!ball)
        return;

    const glm::vec3 position(ball->transform[3]);
    std::string inside;
    const ArenaGoalConfig* insideGoal = nullptr;
    for (const ArenaGoalConfig& goal : map.arena.goals)
    {
        if (!goal.enabled)
            continue;
        // The ball is a sphere: its centre stops one radius short of the net,
        // so the test is on the ball's surface. A centre-only test made a
        // ball resting against the back net 0.6 m outside the goal volume
        // (run 20261126_173644: ball stopped at x = -49.6, goal volume starts
        // at x = -50.2) and a shot into the net scored nothing.
        const glm::vec3 delta = glm::abs(position - goal.center) -
                                glm::vec3(ball->halfExtents.x);
        if (delta.x <= goal.halfExtents.x &&
            delta.y <= goal.halfExtents.y &&
            delta.z <= goal.halfExtents.z)
        {
            inside = goal.id;
            insideGoal = &goal;
            break;
        }
    }

    if (inside.empty())
    {
        vehicleState.arenaGoalInside.clear();
        return;
    }
    if (inside == vehicleState.arenaGoalInside)
        return;
    vehicleState.arenaGoalInside = inside;

    const int goalTeam = arenaTeamIndex(insideGoal->team);
    const int scoringTeam = goalTeam < 0 ? -1 : 1 - goalTeam;
    if (scoringTeam < 0)
        return;

    ServerGamemodeState& d = serverGamemodeState();
    d.communityTeamScore[scoringTeam] += 1;
    if (scoringTeam == 0)
        d.scoreA = d.communityTeamScore[0];
    else
        d.scoreB = d.communityTeamScore[1];
    // The client must observe the score change itself: ask the generic
    // gamemode tick to publish a state packet, otherwise the arena score never
    // leaves the server and there is no client-side goal event.
    d.stateBroadcastPending = true;
    vehicleState.arenaLastGoalTick = tick;

    // Who scored: the car that last played the ball. Proximity at the
    // crossing named nobody once the shot had left the car (run
    // 20261126_173922 reported scorer_vehicle_id 0 for a real goal).
    uint32_t scorerVehicle = 0;
    int scorerTeam = -1;
    uint32_t scorerDriver = 0;
    int scorerFlipResets = 0;
    uint64_t scorerLastResetTick = 0;
    if (ball->lastTouchedVehicleNetworkId != 0)
    {
        for (const VehicleState& vehicle :
             VehicleSystem::instance().vehiclesMutable())
        {
            if (vehicle.chassisNetworkId != ball->lastTouchedVehicleNetworkId)
                continue;
            scorerVehicle = vehicle.id;
            scorerDriver = static_cast<uint32_t>(
                std::max(0, vehicle.driverActorId));
            scorerFlipResets = vehicle.flipResetCount;
            scorerLastResetTick = vehicle.lastFlipResetTick;
            auto teamIt = d.communityTeams.find(scorerDriver);
            if (teamIt != d.communityTeams.end())
                scorerTeam = teamIt->second;
            break;
        }
    }
    // Fallback: nothing recorded a car touch, so use the nearest car.
    if (scorerVehicle == 0)
    {
        float best = 3.0f;
        for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
        {
            if (!vehicle.serverAuthoritative || vehicle.serverMirror)
                continue;
            const PhysicalEntity* chassis =
                PhysicalEntitySystem::instance().find(vehicle.entityId);
            if (!chassis)
                continue;
            const float distance =
                glm::length(glm::vec3(chassis->transform[3]) - position);
            if (distance < best)
            {
                best = distance;
                scorerVehicle = vehicle.id;
                auto teamIt = d.communityTeams.find(
                    static_cast<uint32_t>(std::max(0, vehicle.driverActorId)));
                scorerTeam = teamIt != d.communityTeams.end() ? teamIt->second
                                                               : -1;
            }
        }
    }
    const bool matchRunning = d.matchStartTick > 0 && tick >= d.matchStartTick;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "arena.goal.detected", inside, "authoritative ball entered a goal",
        tick,
        {{"goal_id", inside},
         {"goal_team", insideGoal->team},
         {"scoring_team", scoringTeam},
         {"scorer_vehicle_id", scorerVehicle},
         {"scorer_team", scorerTeam},
         {"scorer_driver_id", scorerDriver},
         {"last_touch_network_id", ball->lastTouchedVehicleNetworkId},
         {"scorer_ball_height_m", ball->lastTouchedHeightM},
         {"aerial", ball->lastTouchedHeightM > kAerialBallHeightM},
         {"flip_resets_at_shot", scorerFlipResets},
         {"freestyle", scorerFlipResets > 0 && scorerLastResetTick != 0 &&
                           tick >= scorerLastResetTick &&
                           tick - scorerLastResetTick <= 360},
         {"match_seconds", matchRunning
                               ? static_cast<float>(tick - d.matchStartTick) / 60.0f
                               : static_cast<float>(tick) / 60.0f},
         {"remaining_seconds",
          d.matchTimeLimitTick > tick
              ? static_cast<float>(d.matchTimeLimitTick - tick) / 60.0f
              : 0.0f},
         {"match_start_tick", d.matchStartTick},
         {"time_limit_seconds", d.timeLimitSeconds},
         {"server_tick", tick},
         {"ball_network_id", static_cast<int>(ball->networkId)},
         {"ball_position", {position.x, position.y, position.z}},
         {"blue_score", d.communityTeamScore[0]},
         {"orange_score", d.communityTeamScore[1]},
         {"goal_value", d.goalValue}});
    resetArenaAfterGoal(vehicleState, players, map, *ball, tick, scoringTeam);

    if (d.goalValue > 0 && d.communityTeamScore[scoringTeam] >= d.goalValue)
    {
        d.matchOver = true;
        d.winnerTeam = scoringTeam;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Physics, StructuredLevel::Important,
            "arena.match.over", "arena", "arena score limit reached", tick,
            {{"reason", "score_limit"},
             {"winner_team", scoringTeam},
             {"blue_score", d.communityTeamScore[0]},
             {"orange_score", d.communityTeamScore[1]}});
    }
}

// The configured time limit ends an arena match. The countdown alone is not a
// limit: without this, a full 5-minute run kept playing past 0:00 (run
// 20261126_182629 reached tick 41062 after the 300 s limit at tick 18979).
void tickArenaTimeLimit(ServerVehicleState& vehicleState, uint32_t tick)
{
    (void)vehicleState;
    ServerGamemodeState& d = serverGamemodeState();
    if (d.matchOver || d.matchTimeLimitTick == 0 || tick < d.matchTimeLimitTick)
        return;
    const uint32_t blue = d.communityTeamScore[0];
    const uint32_t orange = d.communityTeamScore[1];
    d.matchOver = true;
    d.winnerTeam = blue == orange ? -1 : (blue > orange ? 0 : 1);
    d.stateBroadcastPending = true;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "arena.match.over", "arena", "arena time limit reached", tick,
        {{"reason", "time_limit"},
         {"winner_team", d.winnerTeam},
         {"blue_score", blue},
         {"orange_score", orange},
         {"time_limit_seconds", d.timeLimitSeconds},
         {"match_start_tick", d.matchStartTick},
         {"time_limit_tick", d.matchTimeLimitTick},
         {"match_seconds", d.matchStartTick > 0
                               ? static_cast<float>(tick - d.matchStartTick) /
                                     60.0f
                               : 0.0f},
         {"server_tick", tick}});
}

void tickArenaBoostPads(ServerVehicleState& vehicleState,
                        const MapObjectiveConfig& map, uint32_t tick)
{
    if (map.boostPads.empty())
        return;
    if (vehicleState.boostPadReadyTick.size() != map.boostPads.size())
        vehicleState.boostPadReadyTick.assign(map.boostPads.size(), 0);

    for (size_t i = 0; i < map.boostPads.size(); ++i)
    {
        const BoostPadConfig& pad = map.boostPads[i];
        if (!pad.enabled || tick < vehicleState.boostPadReadyTick[i])
            continue;
        for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
        {
            if (!vehicle.serverAuthoritative || vehicle.serverMirror)
                continue;
            auto profile = VehicleSystem::instance().profileFor(vehicle);
            if (!profile || profile->boost.max <= 0.0f)
                continue;
            const PhysicalEntity* entity =
                PhysicalEntitySystem::instance().find(vehicle.entityId);
            if (!entity)
                continue;
            const glm::vec3 delta = glm::vec3(entity->transform[3]) -
                                    pad.position;
            if (glm::length(delta) > pad.radius + 1.5f)
                continue;
            const float before = vehicle.boostCharge;
            const float amount = pad.kind == "big" ? profile->boost.max : 12.0f;
            vehicle.boostCharge =
                std::clamp(vehicle.boostCharge + amount, 0.0f,
                           profile->boost.max);
            vehicleState.boostPadReadyTick[i] =
                tick + static_cast<uint32_t>(std::max(1, pad.rechargeTicks));
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "arena.boostpad.collected", pad.id,
                "authoritative boost pad pickup", tick,
                {{"pad_id", pad.id},
                 {"pad_kind", pad.kind},
                 {"vehicle_id", vehicle.id},
                 {"boost_before", before},
                 {"boost_after", vehicle.boostCharge},
                 {"boost_max", profile->boost.max}});
            break;
        }
    }
}

uint64_t mapVehicleHash(const MapObjectiveConfig& map)
{
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&](const void* data, size_t bytes) {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < bytes; ++i)
        {
            hash ^= p[i];
            hash *= 1099511628211ull;
        }
    };
    for (const MapVehicleSpawn& vehicle : map.vehicles)
    {
        mix(vehicle.id.data(), vehicle.id.size());
        mix(vehicle.templateId.data(), vehicle.templateId.size());
        mix(&vehicle.position.x, sizeof(float));
        mix(&vehicle.position.y, sizeof(float));
        mix(&vehicle.position.z, sizeof(float));
        mix(&vehicle.yaw, sizeof(float));
        const uint8_t enabled = vehicle.enabled ? 1u : 0u;
        mix(&enabled, sizeof(enabled));
    }
    return hash;
}

void copyTemplateId(char* out, size_t capacity, const std::string& value)
{
    if (capacity == 0) return;
    std::memset(out, 0, capacity);
    std::strncpy(out, value.c_str(), capacity - 1);
}

void fillVehicleSpawn(const VehicleState& state, const VehicleSpec& spec,
                      VehicleSpawnEventPacket& pkt, uint32_t tick,
                      uint32_t eventId, uint32_t session)
{
    pkt.header.type = PACKET_VEHICLE_SPAWN_EVENT;
    pkt.header.tick = tick;
    pkt.eventId = eventId;
    pkt.eventSessionId = session;
    pkt.vehicleId = state.id;
    pkt.chassisNetworkId = state.chassisNetworkId;
    copyTemplateId(pkt.templateId, sizeof(pkt.templateId), state.templateId);
    const PhysicalEntity* entity =
        PhysicalEntitySystem::instance().find(state.entityId);
    const glm::vec3 position = entity ? glm::vec3(entity->transform[3])
                                      : state.origin;
    const glm::quat orientation = entity ? entity->orientation : state.orientation;
    pkt.position[0] = position.x;
    pkt.position[1] = position.y;
    pkt.position[2] = position.z;
    pkt.orientation[0] = orientation.w;
    pkt.orientation[1] = orientation.x;
    pkt.orientation[2] = orientation.y;
    pkt.orientation[3] = orientation.z;
    const int seats = std::min<int>(
        MAX_VEHICLE_SEATS,
        std::min<int>(static_cast<int>(state.seatOccupants.size()),
                      static_cast<int>(spec.seats.size())));
    pkt.seatCount = static_cast<int8_t>(seats);
    for (int i = 0; i < seats; ++i)
        pkt.seatOccupants[i] = static_cast<uint32_t>(
            std::max(0, state.seatOccupants[i]));
}

void fillVehicleState(const VehicleState& state,
                      const VehicleSpec& spec,
                      VehicleNetStateEntry& out)
{
    out.vehicleId = state.id;
    out.chassisNetworkId = state.chassisNetworkId;
    out.engineOn = state.engineOn ? 1 : 0;
    out.mode = static_cast<uint8_t>(state.mode);
    out.gear = static_cast<int8_t>(state.gear);
    const int seats = std::min<int>(
        MAX_VEHICLE_SEATS,
        std::min<int>(static_cast<int>(state.seatOccupants.size()),
                      static_cast<int>(spec.seats.size())));
    out.seatCount = static_cast<uint8_t>(seats);
    for (int i = 0; i < seats; ++i)
        out.seatOccupants[i] = static_cast<uint32_t>(
            std::max(0, state.seatOccupants[i]));
    out.engineRpm = state.engineRpm;
    out.speed = state.speed;
    out.boostCharge = state.boostCharge;
    out.throttle = state.throttle;
    out.brake = state.brake;
    out.steer = state.steer;
    out.clutch = state.clutch;
    out.handbrake = state.handbrake;
    const int wheels = std::min<int>(
        MAX_VEHICLE_WHEELS,
        std::min<int>(static_cast<int>(state.wheels.size()),
                      static_cast<int>(spec.wheels.size())));
    out.wheelCount = static_cast<uint8_t>(wheels);
    for (int i = 0; i < wheels; ++i)
    {
        const VehicleWheelState& wheel = state.wheels[i];
        out.wheels[i].spinAngle = wheel.spinAngle;
        out.wheels[i].spinVelocity = wheel.spinVelocity;
        out.wheels[i].steerAngle = wheel.steerAngle;
        out.wheels[i].compression = wheel.compression;
        out.wheels[i].grounded = wheel.grounded ? 1 : 0;
    }
}

void rejectSeatRequest(SOCKET sock, ServerPlayer& player,
                       uint32_t vehicleId, int8_t seat, uint32_t tick,
                       uint64_t& totalPacketsOut)
{
    VehicleSeatEventPacket pkt{};
    pkt.header.type = PACKET_VEHICLE_SEAT_EVENT;
    pkt.header.tick = tick;
    pkt.eventId = nextReliableGameplayEventId();
    pkt.eventSessionId = reliableGameplayEventSessionForPlayer(player);
    pkt.vehicleId = vehicleId;
    pkt.actorId = player.id;
    pkt.seat = seat;
    pkt.action = 0;
    queueReliableGameplayEventToPlayer(
        sock, player, &pkt, sizeof(pkt), pkt.eventId, pkt.eventSessionId,
        totalPacketsOut);
}

void broadcastSeatEvent(SOCKET sock,
                        std::unordered_map<uint32_t, ServerPlayer>& players,
                        uint32_t vehicleId, uint32_t actorId, int8_t seat,
                        uint8_t action, uint32_t tick,
                        uint64_t& totalPacketsOut)
{
    VehicleSeatEventPacket pkt{};
    pkt.header.type = PACKET_VEHICLE_SEAT_EVENT;
    pkt.header.tick = tick;
    pkt.eventId = nextReliableGameplayEventId();
    pkt.eventSessionId = serverReliableEventSessionId();
    pkt.vehicleId = vehicleId;
    pkt.actorId = actorId;
    pkt.seat = seat;
    pkt.action = action;
    queueReliableGameplayEventToAll(sock, players, &pkt, sizeof(pkt),
                                    pkt.eventId, pkt.eventSessionId,
                                    totalPacketsOut);
}

} // namespace

void serverTickVehicles(ServerVehicleState& vehicleState,
                        std::unordered_map<uint32_t, ServerPlayer>& players,
                        World* npcWorld, uint32_t& nextEntityId, uint32_t tick)
{
    if (!VehicleSystem::instance().isServerProcess())
        return;

    // A dedicated server never runs the engine config-polling path, so this is
    // its only profile reload. It used to race the engine thread's first load
    // and crash in the registry hash lookup; the registry now publishes
    // immutable snapshots under a shared_mutex, so a reload from either thread
    // is safe.
    if ((tick % 60u) == 0u)
        VehicleSystem::instance().pollReload();

    {
        const Gamemode& gm =
            GamemodeRegistry::instance().get(serverGamemodeState().matchMode);
        VehicleSystem::instance().setActiveProfile(gm.vehicleProfile);
    }

    const MapObjectiveConfig& map = MapConfigRegistry::instance().current();
    const std::string& mapId = map.mapId;
    const uint64_t revision = MapConfigRegistry::instance().revision();
    const uint64_t vehicleHash = mapVehicleHash(map);
    if (!vehicleState.mapInitialized || vehicleState.mapId != mapId ||
        vehicleState.vehicleHash != vehicleHash)
    {
        // Map change or authored-vehicle edit. Drop only server-authoritative
        // vehicles; client-owned/offline vehicles never exist in this process.
        std::vector<uint32_t> doomed;
        for (const VehicleState& vehicle : VehicleSystem::instance().vehicles())
            if (vehicle.serverAuthoritative)
                doomed.push_back(vehicle.id);
        for (uint32_t id : doomed)
            VehicleSystem::instance().despawn(id);
        vehicleState.knownVehicles.clear();
        vehicleState.npcDriverIds.clear();
        vehicleState.npcVehicleId = 0;
        vehicleState.mapId = mapId;
        vehicleState.mapRevision = revision;
        vehicleState.vehicleHash = vehicleHash;
        vehicleState.mapInitialized = true;
        vehicleState.arenaGoalInside.clear();
        vehicleState.boostPadReadyTick.assign(map.boostPads.size(), tick);

        if (!mapId.empty())
        {
            for (const MapVehicleSpawn& spawn : map.vehicles)
            {
                if (!spawn.enabled)
                    continue;
                const VehicleSpec* spec =
                    VehicleConfigRegistry::instance().find(spawn.templateId);
                if (!spec)
                {
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Network,
                        StructuredLevel::Errors,
                        "vehicle.server.spawn-rejected", spawn.id,
                        "authored vehicle template is missing", tick,
                        {{"vehicle_id", spawn.id},
                         {"template", spawn.templateId},
                         {"map", mapId}});
                    continue;
                }
                glm::mat4 transform =
                    glm::translate(glm::mat4(1.0f), spawn.position) *
                    glm::mat4_cast(glm::angleAxis(
                        spawn.yaw, glm::vec3(0.0f, 0.0f, 1.0f)) *
                        glm::angleAxis(spawn.pitch,
                                       glm::vec3(0.0f, 1.0f, 0.0f)) *
                        glm::angleAxis(spawn.roll,
                                       glm::vec3(1.0f, 0.0f, 0.0f)));
                if (npcWorld)
                    transform = settleVehicleSpawnTransform(
                        *npcWorld, *spec, transform);
                const uint32_t chassisNetworkId = nextEntityId++;
                const uint32_t vehicleId =
                    VehicleSystem::instance().spawnAuthoritative(
                        spawn.templateId, transform, chassisNetworkId);
                if (vehicleId == 0)
                    continue;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Network, StructuredLevel::Important,
                    "vehicle.server.spawned", spawn.id,
                    "authored vehicle spawned by the authoritative server",
                    tick,
                    {{"vehicle_id", vehicleId},
                     {"authored_id", spawn.id},
                     {"template", spawn.templateId},
                     {"chassis_network_id", chassisNetworkId},
                     {"map", mapId},
                     {"yaw", spawn.yaw},
                     {"pitch", spawn.pitch},
                     {"roll", spawn.roll},
                     {"chassis_forward_yaw_deg", [&]() {
                          const PhysicalEntity* chassis =
                              PhysicalEntitySystem::instance().findByNetworkId(
                                  chassisNetworkId);
                          if (!chassis)
                              return 0.0f;
                          const glm::vec3 forward =
                              chassis->orientation * glm::vec3(1.0f, 0.0f, 0.0f);
                          return glm::degrees(std::atan2(forward.y, forward.x));
                      }()},
                     {"position", {spawn.position.x, spawn.position.y,
                                    spawn.position.z}}});
            }
        }
    }

    if (isVehicleArenaMode(serverGamemodeState().matchMode))
    {
        // Arena match clock, one sample per 30 s and one at the limit. The
        // 5-minute limit is a config claim until the journal shows it running.
        {
            ServerGamemodeState& clock = serverGamemodeState();
            if (tick % 1800u == 0u ||
                (clock.matchTimeLimitTick > 0 &&
                 tick == clock.matchTimeLimitTick))
            {
                const bool started = clock.matchStartTick > 0 &&
                                     tick >= clock.matchStartTick;
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::Physics, StructuredLevel::Important,
                    "arena.match.clock", "arena", "arena match clock sample",
                    tick,
                    {{"match_seconds", started ? static_cast<float>(
                                       tick - clock.matchStartTick) / 60.0f
                                     : 0.0f},
                     {"remaining_seconds",
                      clock.matchTimeLimitTick > tick
                          ? static_cast<float>(clock.matchTimeLimitTick - tick) /
                                60.0f
                          : 0.0f},
                     {"time_limit_seconds", clock.timeLimitSeconds},
                     {"match_start_tick", clock.matchStartTick},
                     {"time_limit_tick", clock.matchTimeLimitTick},
                     {"server_tick", tick},
                     {"blue_score", clock.communityTeamScore[0]},
                     {"orange_score", clock.communityTeamScore[1]}});
            }
        }
        tickArenaTimeLimit(vehicleState, tick);
        tickArenaSave(vehicleState, map, tick);
        tickArenaGoals(vehicleState, players, map, tick);
        tickArenaBoostPads(vehicleState, map, tick);
    }

    // Vehicle Arena NPC car: one NPC-driven car uses the same VehicleInput
    // surface as a human driver. The mode's team data is the roster owner, so a
    // 0-NPC practice run does not spawn an invisible synthetic driver.
    if (isVehicleArenaMode(serverGamemodeState().matchMode) &&
        gamemodeAllowsScoreNpc(serverGamemodeState().matchMode))
    {
        // Seat human players before NPCs. Otherwise an NPC can claim the car
        // nearest the human spawn and starve the joining player.
        // A human waiting for a car, and at least one human already seated. The
        // NPC must never take the car a joining human would have reached: at
        // server start no human exists yet, so an "only when nobody is waiting"
        // gate let the NPC claim the blue car and left the human unseated.
        bool humanWaitingForCar = false;
        bool humanSeated = false;
        for (const auto& [playerId, player] : players)
        {
            if (player.dead)
                continue;
            if (player.seatedVehicleId != 0)
                humanSeated = true;
            else if (player.spawnState == ServerPlayer::Active)
                humanWaitingForCar = true;
        }
        const bool botVsBot = std::getenv("MIMITA_ARENA_BOT_VS_BOT") != nullptr;
        // A bot-vs-bot probe has no human to protect: the connected client is
        // deliberately never seated, so "a human is waiting for a car" was true
        // the whole match and shut the bots off after the first goal reset
        // (run 20261126_174545: bot driving stops at t = 5.3 s, exactly the
        // goal, and the ball then rolled into a corner and stayed there for
        // the rest of the match).
        if (botVsBot || (!humanWaitingForCar && humanSeated))
        {
        bool npcVehicleAlive = false;
        for (const VehicleState& vehicle : VehicleSystem::instance().vehicles())
            if (vehicle.id == vehicleState.npcVehicleId)
                npcVehicleAlive = true;
        if (!npcVehicleAlive)
        {
            vehicleState.npcVehicleId = 0;
            vehicleState.npcDriverIds.clear();
        }
        if (vehicleState.npcVehicleId == 0)
        {
            glm::vec3 target(0.0f);
            bool hasTarget = false;
            for (const ArenaKickoffConfig& kickoff : map.arena.kickoffs)
                if (kickoff.enabled && kickoff.team == "orange")
                {
                    target = kickoff.position;
                    hasTarget = true;
                    break;
                }
            VehicleState* best = nullptr;
            float bestDistance = std::numeric_limits<float>::max();
            const ArenaKickoffConfig* blueKickoff =
                arenaKickoffForTeam(map, arenaTeamIndex("blue"), 0);
            const ArenaKickoffConfig* orangeKickoff =
                arenaKickoffForTeam(map, arenaTeamIndex("orange"), 0);
            for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
            {
                if (!vehicle.serverAuthoritative || vehicle.serverMirror ||
                    vehicle.seatOccupants.empty() || vehicle.seatOccupants[0] != 0)
                    continue;
                // Team-aware: an NPC on the orange team must not take the car
                // that sits on the blue kickoff.
                const glm::vec3 carPosition = arenaVehiclePosition(vehicle);
                if (blueKickoff && orangeKickoff)
                {
                    const float toBlue =
                        glm::length(carPosition - blueKickoff->position);
                    const float toOrange =
                        glm::length(carPosition - orangeKickoff->position);
                    if (toBlue < toOrange)
                        continue;
                }
                const glm::vec3 anchor = hasTarget ? target : carPosition;
                const float distance = glm::length(carPosition - anchor);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = &vehicle;
                }
            }
            if (best)
            {
                const uint32_t npcDriverId = nextEntityId++;
                if (VehicleSystem::instance().enterSeat(best->id, 0, npcDriverId))
                {
                    best->mode = VehicleControlMode::Automatic;
                    best->engineOn = true;
                    vehicleState.npcDriverIds.insert(npcDriverId);
                    vehicleState.npcVehicleId = best->id;
                    // The bots are team members: the score and the goal event
                    // name their team, exactly as for a human driver. The team
                    // comes from the car the bot actually took, not from a
                    // hardcoded index: a bot labelled orange while driving the
                    // blue car attacked its own goal.
                    const glm::vec3 seatedAt = arenaVehiclePosition(*best);
                    int botTeam = 1;
                    if (blueKickoff && orangeKickoff)
                    {
                        const float toBlue =
                            glm::length(seatedAt - blueKickoff->position);
                        const float toOrange =
                            glm::length(seatedAt - orangeKickoff->position);
                        botTeam = toBlue < toOrange ? 0 : 1;
                    }
                    serverGamemodeState().communityTeams[npcDriverId] =
                        botTeam;
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Network, StructuredLevel::Important,
                        "vehicle.auto_seat", std::to_string(npcDriverId),
                        "arena NPC car driver seated",
                        tick,
                        {{"driver_id", npcDriverId},
                         {"vehicle_id", best->id},
                         {"seat", 0},
                         {"npc", true},
                         {"team", botTeam},
                         {"mode", serverGamemodeState().matchMode}});
                }
            }
        }
        if (botVsBot && vehicleState.npcVehicleId2 == 0)
        {
            // Second bot: the other team's car, aiming at the other goal.
            VehicleState* best = nullptr;
            float bestDistance = std::numeric_limits<float>::max();
            const ArenaKickoffConfig* blueKickoff =
                arenaKickoffForTeam(map, arenaTeamIndex("blue"), 0);
            const glm::vec3 anchor = blueKickoff ? blueKickoff->position
                                                 : glm::vec3(0.0f);
            for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
            {
                if (!vehicle.serverAuthoritative || vehicle.serverMirror ||
                    vehicle.seatOccupants.empty() || vehicle.seatOccupants[0] != 0)
                    continue;
                const float distance =
                    glm::length(arenaVehiclePosition(vehicle) - anchor);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = &vehicle;
                }
            }
            if (best)
            {
                const uint32_t npcDriverId = nextEntityId++;
                if (VehicleSystem::instance().enterSeat(best->id, 0, npcDriverId))
                {
                    best->mode = VehicleControlMode::Automatic;
                    best->engineOn = true;
                    vehicleState.npcDriverIds.insert(npcDriverId);
                    vehicleState.npcVehicleId2 = best->id;
                    {
                        const glm::vec3 seatedAt = arenaVehiclePosition(*best);
                        int botTeam = 0;
                        if (blueKickoff)
                        {
                            const ArenaKickoffConfig* orangeKickoff2 =
                                arenaKickoffForTeam(map,
                                                    arenaTeamIndex("orange"), 0);
                            if (orangeKickoff2)
                                botTeam =
                                    glm::length(seatedAt -
                                                blueKickoff->position) <
                                            glm::length(seatedAt -
                                                orangeKickoff2->position)
                                        ? 0
                                        : 1;
                        }
                        serverGamemodeState().communityTeams[npcDriverId] =
                            botTeam;
                    }
                    StructuredLogger::instance().writeEvent(
                        StructuredCategory::Network, StructuredLevel::Important,
                        "vehicle.auto_seat", std::to_string(npcDriverId),
                        "arena NPC car driver seated (second bot)",
                        tick,
                        {{"driver_id", npcDriverId},
                         {"vehicle_id", best->id},
                         {"seat", 0},
                         {"npc", true},
                         {"bot_vs_bot", true},
                         {"team",
                          serverGamemodeState()
                              .communityTeams[npcDriverId]},
                         {"mode", serverGamemodeState().matchMode}});
                }
            }
        }
        if (vehicleState.npcVehicleId != 0)
        {
            const PhysicalEntity* ball = findArenaBall();
            if (ball)
                for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
                    if (vehicle.id == vehicleState.npcVehicleId)
                    {
                        // Attacking goal and own goal come from the car's
                        // team, so no bot ever drives at its own net.
                        const int team =
                            static_cast<int>(arenaTeamOfVehicle(vehicle));
                        glm::vec3 targetGoal(0.0f);
                        glm::vec3 ownGoal(0.0f);
                        for (const ArenaGoalConfig& goal : map.arena.goals)
                        {
                            if (!goal.enabled)
                                continue;
                            const int goalTeam = arenaTeamIndex(goal.team);
                            if (goalTeam == team)
                                ownGoal = goal.center;
                            else if (goalTeam >= 0)
                                targetGoal = goal.center;
                        }
                        driveArenaNpcCar(vehicle, *ball, targetGoal, ownGoal);
                        break;
                    }
        }
        if (vehicleState.npcVehicleId2 != 0)
        {
            const PhysicalEntity* ball = findArenaBall();
            if (ball)
                for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
                    if (vehicle.id == vehicleState.npcVehicleId2)
                    {
                        const int team =
                            static_cast<int>(arenaTeamOfVehicle(vehicle));
                        glm::vec3 targetGoal(0.0f);
                        glm::vec3 ownGoal(0.0f);
                        for (const ArenaGoalConfig& goal : map.arena.goals)
                        {
                            if (!goal.enabled)
                                continue;
                            const int goalTeam = arenaTeamIndex(goal.team);
                            if (goalTeam == team)
                                ownGoal = goal.center;
                            else if (goalTeam >= 0)
                                targetGoal = goal.center;
                        }
                        driveArenaNpcCar(vehicle, *ball, targetGoal, ownGoal);
                        break;
                    }
        }
        }
    }

    // Clear seats whose player died, disconnected, or no longer exists.
    for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
    {
        if (!vehicle.serverAuthoritative)
            continue;
        for (size_t seat = 0; seat < vehicle.seatOccupants.size(); ++seat)
        {
            const int occupant = vehicle.seatOccupants[seat];
            if (occupant <= 0)
                continue;
            const auto it = players.find(static_cast<uint32_t>(occupant));
            const bool npcDriver =
                vehicleState.npcDriverIds.count(static_cast<uint32_t>(occupant)) != 0;
            const bool valid = npcDriver ||
                (it != players.end() && !it->second.dead &&
                 !it->second.connectionStale);
            if (valid)
                continue;
            vehicle.seatOccupants[seat] = 0;
            if (seat == 0)
            {
                vehicle.driverActorId = 0;
                vehicle.driverSeat = -1;
            }
        }
    }

    // Vehicle Arena players spawn as their car. Seat an active, unseated player
    // into the nearest free authoritative car without adding a second vehicle
    // or a mode-specific physics path.
    if (isVehicleArenaMode(serverGamemodeState().matchMode))
    {
        for (auto& [playerId, player] : players)
        {
            if (player.dead || player.seatedVehicleId != 0 ||
                player.spawnState != ServerPlayer::Active)
                continue;
            VehicleState* best = nullptr;
            float bestDistance = std::numeric_limits<float>::max();
            for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
            {
                if (!vehicle.serverAuthoritative || vehicle.serverMirror ||
                    vehicle.seatOccupants.empty() || vehicle.seatOccupants[0] != 0)
                    continue;
                const float distance = glm::length(vehicle.origin - player.pos);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = &vehicle;
                }
            }
            if (!best || bestDistance > 40.0f)
                continue;
            if (!VehicleSystem::instance().enterSeat(best->id, 0, player.id))
                continue;
            best->mode = VehicleControlMode::Automatic;
            best->engineOn = true;
            player.seatedVehicleId = best->id;
            player.seatedSeat = 0;
            player.vel = glm::vec3(0.0f);
            glm::vec3 seatPosition(0.0f);
            glm::vec3 seatForward(0.0f, 1.0f, 0.0f);
            if (VehicleSystem::instance().seatWorld(best->id, 0, seatPosition,
                                                    seatForward))
                player.pos = seatPosition;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "vehicle.auto_seat", std::to_string(player.id),
                "arena player spawned into the nearest free vehicle",
                tick,
                {{"player_id", player.id},
                 {"vehicle_id", best->id},
                 {"seat", 0},
                 {"distance", bestDistance},
                 {"mode", serverGamemodeState().matchMode}});
        }
    }

    // The shared physical owner advances only serverSimulated bodies. The
    // vehicle fixed-tick callback applies suspension/tire/drivetrain forces.
    if (npcWorld)
        PhysicalEntitySystem::instance().advanceKinematics(
            SERVER_DT, *npcWorld, true);

    // Void authority: a vehicle below the kill plane is destroyed and every
    // seated player is killed through the normal dead/respawn path.
    const float killZ = getVoidDeathConfig().killZ;
    std::vector<uint32_t> doomed;
    for (VehicleState& vehicle : VehicleSystem::instance().vehiclesMutable())
    {
        if (!vehicle.serverAuthoritative)
            continue;
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(vehicle.entityId);
        if (!entity || entity->transform[3].z >= killZ)
            continue;
        for (int occupant : vehicle.seatOccupants)
        {
            if (occupant <= 0)
                continue;
            const auto playerIt = players.find(static_cast<uint32_t>(occupant));
            if (playerIt == players.end() || playerIt->second.dead)
                continue;
            ServerPlayer& seated = playerIt->second;
            seated.dead = true;
            seated.health = 0;
            seated.seatedVehicleId = 0;
            seated.seatedSeat = -1;
            seated.respawnSeconds = 0.05f;
        }
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.server.void", std::to_string(vehicle.id),
            "authoritative vehicle fell below the void kill plane", tick,
            {{"vehicle_id", vehicle.id},
             {"chassis_network_id", vehicle.chassisNetworkId},
             {"z", entity->transform[3].z},
             {"kill_z", killZ}});
        doomed.push_back(vehicle.id);
    }
    for (uint32_t id : doomed)
        VehicleSystem::instance().despawn(id);
}

void serverReplicateVehicles(SOCKET sock, ServerVehicleState& vehicleState,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             uint32_t tick, uint64_t& totalPacketsOut)
{
    if (!VehicleSystem::instance().isServerProcess())
        return;

    const uint32_t session = serverReliableEventSessionId();
    std::unordered_set<uint32_t> live;
    for (const VehicleState& vehicle : VehicleSystem::instance().vehicles())
        if (vehicle.serverAuthoritative)
            live.insert(vehicle.id);

    for (auto& kv : players)
    {
        ServerPlayer& player = kv.second;
        if (!player.spawned)
            continue;
        auto& known = vehicleState.knownVehicles[player.id];
        for (const VehicleState& vehicle : VehicleSystem::instance().vehicles())
        {
            if (!vehicle.serverAuthoritative)
                continue;
            if (known.count(vehicle.id) != 0)
                continue;
            const VehicleSpec* spec =
                VehicleConfigRegistry::instance().find(vehicle.templateId);
            if (!spec)
                continue;
            VehicleSpawnEventPacket spawn{};
            fillVehicleSpawn(vehicle, *spec, spawn, tick,
                             nextReliableGameplayEventId(), session);
            queueReliableGameplayEventToPlayer(
                sock, player, &spawn, sizeof(spawn),
                spawn.eventId, spawn.eventSessionId, totalPacketsOut);
            known.insert(vehicle.id);
        }
        for (auto it = known.begin(); it != known.end(); )
        {
            if (live.count(*it) != 0)
            {
                ++it;
                continue;
            }
            VehicleDespawnEventPacket despawn{};
            despawn.header.type = PACKET_VEHICLE_DESPAWN_EVENT;
            despawn.header.tick = tick;
            despawn.eventId = nextReliableGameplayEventId();
            despawn.eventSessionId = session;
            despawn.vehicleId = *it;
            const VehicleState* stateVehicle =
                VehicleSystem::instance().find(*it);
            despawn.chassisNetworkId =
                stateVehicle ? stateVehicle->chassisNetworkId : 0;
            queueReliableGameplayEventToPlayer(
                sock, player, &despawn, sizeof(despawn),
                despawn.eventId, despawn.eventSessionId,
                totalPacketsOut);
            it = known.erase(it);
        }

        if (player.seatedVehicleId == 0)
        {
            player.lastReplicatedSeatVehicleId = 0;
            player.lastReplicatedSeat = -1;
        }
        if (player.seatedVehicleId != 0 &&
            (player.lastReplicatedSeatVehicleId != player.seatedVehicleId ||
             player.lastReplicatedSeat != player.seatedSeat))
        {
            VehicleSeatEventPacket seat{};
            seat.header.type = PACKET_VEHICLE_SEAT_EVENT;
            seat.header.tick = tick;
            seat.eventId = nextReliableGameplayEventId();
            seat.eventSessionId = serverReliableEventSessionId();
            seat.vehicleId = player.seatedVehicleId;
            seat.actorId = player.id;
            seat.seat = player.seatedSeat;
            seat.action = VEHICLE_SEAT_ENTER;
            queueReliableGameplayEventToPlayer(
                sock, player, &seat, sizeof(seat), seat.eventId,
                seat.eventSessionId, totalPacketsOut);
            player.lastReplicatedSeatVehicleId = player.seatedVehicleId;
            player.lastReplicatedSeat = player.seatedSeat;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "vehicle.seat.replicated_auto", std::to_string(player.id),
                "auto-seat state replicated to the owning player", tick,
                {{"player_id", player.id},
                 {"vehicle_id", player.seatedVehicleId},
                 {"seat", player.seatedSeat}});
        }

        if ((tick & 1u) != 0u)
            continue;
        VehicleStatePacket packet{};
        packet.header.type = PACKET_VEHICLE_STATE;
        packet.header.tick = tick;
        uint8_t count = 0;
        for (const VehicleState& vehicle : VehicleSystem::instance().vehicles())
        {
            if (!vehicle.serverAuthoritative)
                continue;
            if (count >= MAX_VEHICLE_STATE_ENTRIES)
                break;
            const VehicleSpec* spec =
                VehicleConfigRegistry::instance().find(vehicle.templateId);
            if (!spec)
                continue;
            fillVehicleState(vehicle, *spec, packet.entries[count++]);
        }
        if (count == 0)
            continue;
        packet.count = count;
        if (serverSendToPlayer(sock, player, &packet, sizeof(packet)))
            ++totalPacketsOut;
    }

    for (auto it = vehicleState.knownVehicles.begin();
         it != vehicleState.knownVehicles.end(); )
    {
        const auto playerIt = players.find(it->first);
        if (playerIt == players.end() || !playerIt->second.spawned)
            it = vehicleState.knownVehicles.erase(it);
        else
            ++it;
    }
}

void handleVehicleInputPacket(const char* buffer, int bytes,
                              std::unordered_map<uint32_t, ServerPlayer>& players,
                              const sockaddr_in* from,
                              const TransportConnectionId* connectionId,
                              uint32_t serverTick)
{
    if (!VehicleSystem::instance().isServerProcess() ||
        bytes < (int)sizeof(VehicleInputPacket))
        return;
    const VehicleInputPacket* pkt =
        reinterpret_cast<const VehicleInputPacket*>(buffer);
    const auto it = players.find(pkt->header.playerId);
    if (it == players.end())
        return;
    ServerPlayer& player = it->second;
    if (from && !playerOwnsConnectionSource(player, from, connectionId))
        return;
    // Bounded rejection diagnostic: a dropped input packet used to be silent,
    // which made "the car does not respond" impossible to attribute.
    const auto reject = [&](const char* reason) {
        static uint32_t sLastRejectTick = 0;
        if (serverTick - sLastRejectTick < 30u)
            return;
        sLastRejectTick = serverTick;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.input.rejected", std::to_string(pkt->vehicleId),
            "server rejected a driver input packet", serverTick,
            {{"reason", reason},
             {"vehicle_id", pkt->vehicleId},
             {"player_id", pkt->header.playerId},
             {"seated_vehicle", player.seatedVehicleId},
             {"dead", player.dead}});
    };
    if (player.dead || player.seatedVehicleId == 0 ||
        player.seatedVehicleId != pkt->vehicleId)
    {
        reject(player.dead ? "dead" : "seat_mismatch");
        return;
    }
    VehicleState* vehicle =
        VehicleSystem::instance().find(pkt->vehicleId);
    if (!vehicle || vehicle->serverMirror)
    {
        reject(vehicle ? "server_mirror" : "no_vehicle");
        return;
    }
    if (!VehicleSystem::instance().isDriver(pkt->vehicleId, player.id))
    {
        reject("not_driver");
        return;
    }

    VehicleInput input;
    input.throttle = std::clamp(pkt->throttle, 0.0f, 1.0f);
    input.brake = std::clamp(pkt->brake, 0.0f, 1.0f);
    input.steer = std::clamp(pkt->steer, -1.0f, 1.0f);
    input.clutch = std::clamp(pkt->clutch, 0.0f, 1.0f);
    input.handbrake = std::clamp(pkt->handbrake, 0.0f, 1.0f);
    input.boost = std::clamp(pkt->boost, 0.0f, 1.0f);
    input.pitch = std::clamp(pkt->pitch, -1.0f, 1.0f);
    input.yaw = std::clamp(pkt->yaw, -1.0f, 1.0f);
    input.roll = std::clamp(pkt->roll, -1.0f, 1.0f);
    input.jumpHeld = (pkt->flags & VEHICLE_INPUT_JUMP_HELD) != 0;
    input.powerslide = (pkt->flags & VEHICLE_INPUT_POWER_SLIDE) != 0;
    input.engineToggle = (pkt->flags & VEHICLE_INPUT_ENGINE_TOGGLE) != 0;
    input.modeToggle = (pkt->flags & VEHICLE_INPUT_MODE_TOGGLE) != 0;
    input.shiftUp = (pkt->flags & VEHICLE_INPUT_SHIFT_UP) != 0;
    input.shiftDown = (pkt->flags & VEHICLE_INPUT_SHIFT_DOWN) != 0;
    VehicleSystem::instance().setInput(pkt->vehicleId, input);

    static uint32_t sLastInputLogTick = 0;
    if ((input.throttle > 0.01f || input.brake > 0.01f ||
         std::abs(input.steer) > 0.01f || input.handbrake > 0.01f ||
         std::abs(input.pitch) > 0.01f || std::abs(input.yaw) > 0.01f ||
         std::abs(input.roll) > 0.01f) &&
        serverTick - sLastInputLogTick >= 15u)
    {
        sLastInputLogTick = serverTick;
        const PhysicalEntity* entity =
            PhysicalEntitySystem::instance().find(vehicle->entityId);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.input.server", std::to_string(pkt->vehicleId),
            "server applied driver input to the authoritative vehicle",
            serverTick,
            {{"vehicle_id", pkt->vehicleId},
             {"actor_id", player.id},
             {"throttle", input.throttle},
             {"brake", input.brake},
             {"steer", input.steer},
             {"pitch", input.pitch},
             {"yaw", input.yaw},
             {"roll", input.roll},
             {"handbrake", input.handbrake},
             {"boost", input.boost},
             {"jump_held", input.jumpHeld},
             {"powerslide", input.powerslide},
             {"speed", vehicle->speed},
             {"gear", vehicle->gear},
             {"position", entity ? std::vector<float>{entity->transform[3].x,
                                                      entity->transform[3].y,
                                                      entity->transform[3].z}
                                 : std::vector<float>{}}});
    }
}

void handleVehicleSeatRequest(SOCKET sock, const char* buffer, int bytes,
                              std::unordered_map<uint32_t, ServerPlayer>& players,
                              const sockaddr_in* from,
                              const TransportConnectionId* connectionId,
                              uint32_t serverTick, uint64_t& totalPacketsOut)
{
    if (!VehicleSystem::instance().isServerProcess() ||
        bytes < (int)sizeof(VehicleSeatRequestPacket))
        return;
    const VehicleSeatRequestPacket* pkt =
        reinterpret_cast<const VehicleSeatRequestPacket*>(buffer);
    const auto it = players.find(pkt->header.playerId);
    if (it == players.end())
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "vehicle.seat.request.unknown-player", "vehicle-seat",
            "seat request references an unknown server player", serverTick,
            {{"player_id", pkt->header.playerId},
             {"vehicle_id", pkt->vehicleId},
             {"seat", pkt->seat},
             {"action", pkt->action},
             {"serial", pkt->requestSerial}});
        return;
    }
    ServerPlayer& player = it->second;
    if (from && !playerOwnsConnectionSource(player, from, connectionId))
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Errors,
            "vehicle.seat.request.bad-source", "vehicle-seat",
            "seat request source does not match the player connection",
            serverTick,
            {{"player_id", player.id},
             {"vehicle_id", pkt->vehicleId},
             {"seat", pkt->seat},
             {"serial", pkt->requestSerial}});
        return;
    }
    if (pkt->requestSerial == 0 ||
        pkt->requestSerial == player.lastVehicleSeatRequestSerial)
        return;
    if (player.lastLoggedVehicleSeatRequestSerial != pkt->requestSerial)
    {
        player.lastLoggedVehicleSeatRequestSerial = pkt->requestSerial;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.seat.request.received", "vehicle-seat",
            "server received a vehicle seat request", serverTick,
            {{"player_id", player.id},
             {"vehicle_id", pkt->vehicleId},
             {"seat", pkt->seat},
             {"action", pkt->action},
             {"serial", pkt->requestSerial},
             {"dead", player.dead},
             {"stale", player.connectionStale},
             {"player_pos", {player.pos.x, player.pos.y, player.pos.z}}});
    }
    auto reject = [&](const char* reason) {
        player.lastVehicleSeatRequestSerial = pkt->requestSerial;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.seat.rejected", std::to_string(pkt->vehicleId),
            reason, serverTick,
            {{"vehicle_id", pkt->vehicleId},
             {"actor_id", player.id},
             {"seat", pkt->seat},
             {"action", pkt->action},
             {"serial", pkt->requestSerial},
             {"player_pos", {player.pos.x, player.pos.y, player.pos.z}}});
        rejectSeatRequest(sock, player, pkt->vehicleId, pkt->seat,
                          serverTick, totalPacketsOut);
    };
    if (player.dead || player.connectionStale)
    {
        reject("player is dead or stale");
        return;
    }
    VehicleState* vehicle =
        VehicleSystem::instance().find(pkt->vehicleId);
    if (!vehicle || !vehicle->serverAuthoritative)
    {
        reject("vehicle does not exist or is not authoritative");
        return;
    }
    const VehicleSpec* spec =
        VehicleConfigRegistry::instance().find(vehicle->templateId);
    if (!spec)
    {
        reject("vehicle template is missing");
        return;
    }

    if (pkt->action == VEHICLE_SEAT_EXIT)
    {
        if (player.seatedVehicleId != pkt->vehicleId ||
            player.seatedSeat < 0)
        {
            reject("player is not seated in that vehicle");
            return;
        }
        const int8_t exitedSeat = player.seatedSeat;
        glm::vec3 seatPos(0.0f);
        glm::vec3 forward(0.0f, 1.0f, 0.0f);
        VehicleSystem::instance().seatWorld(pkt->vehicleId, player.seatedSeat,
                                            seatPos, forward);
        const glm::vec3 right = glm::normalize(
            glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f)));
        VehicleSystem::instance().exitSeat(pkt->vehicleId,
                                           player.seatedSeat, player.id);
        player.seatedVehicleId = 0;
        player.seatedSeat = -1;
        player.pos = seatPos + right * 1.4f;
        player.vel = glm::vec3(0.0f);
        player.onGround = false;
        player.attackQueued = false;
        player.lastVehicleSeatRequestSerial = pkt->requestSerial;
        broadcastSeatEvent(sock, players, pkt->vehicleId, player.id,
                           exitedSeat, VEHICLE_SEAT_EXIT,
                           serverTick, totalPacketsOut);
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.seat.server_exit", std::to_string(pkt->vehicleId),
            "server accepted a vehicle seat exit", serverTick,
            {{"vehicle_id", pkt->vehicleId},
             {"actor_id", player.id},
             {"position", {player.pos.x, player.pos.y, player.pos.z}}});
        return;
    }

    if (pkt->action != VEHICLE_SEAT_ENTER ||
        pkt->seat < 0 ||
        pkt->seat >= static_cast<int>(spec->seats.size()))
    {
        reject("seat index is invalid for this vehicle");
        return;
    }

    // If already in this seat, treat as idempotent success.
    if (player.seatedVehicleId == pkt->vehicleId &&
        player.seatedSeat == pkt->seat)
    {
        player.lastVehicleSeatRequestSerial = pkt->requestSerial;
        broadcastSeatEvent(sock, players, pkt->vehicleId, player.id,
                           static_cast<int8_t>(pkt->seat), VEHICLE_SEAT_ENTER,
                           serverTick, totalPacketsOut);
        return;
    }

    const PhysicalEntity* vehicleEntity =
        PhysicalEntitySystem::instance().find(vehicle->entityId);
    if (!vehicleEntity)
    {
        reject("vehicle physical entity is missing");
        return;
    }
    const glm::vec3 vehiclePos = glm::vec3(vehicleEntity->transform[3]);
    const float range = std::max(1.0f, spec->interactionRadius) + 2.0f;
    if (glm::length(player.pos - vehiclePos) > range)
    {
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.seat.rejected", std::to_string(pkt->vehicleId),
            "seat request out of range", serverTick,
            {{"vehicle_id", pkt->vehicleId},
             {"actor_id", player.id},
             {"seat", pkt->seat},
             {"serial", pkt->requestSerial},
             {"distance", glm::length(player.pos - vehiclePos)},
             {"range", range},
             {"player_pos", {player.pos.x, player.pos.y, player.pos.z}},
             {"vehicle_pos", {vehiclePos.x, vehiclePos.y, vehiclePos.z}}});
        player.lastVehicleSeatRequestSerial = pkt->requestSerial;
        rejectSeatRequest(sock, player, pkt->vehicleId, pkt->seat,
                          serverTick, totalPacketsOut);
        return;
    }
    uint32_t occupant = 0;
    if (VehicleSystem::instance().seatOccupied(pkt->vehicleId, pkt->seat,
                                               &occupant) &&
        occupant != player.id)
    {
        reject("seat is occupied");
        return;
    }
    if (player.seatedVehicleId != 0)
    {
        VehicleSystem::instance().exitSeatByActor(player.id);
        player.seatedVehicleId = 0;
        player.seatedSeat = -1;
    }
    if (!VehicleSystem::instance().enterSeat(pkt->vehicleId, pkt->seat,
                                             player.id))
    {
        reject("VehicleSystem::enterSeat failed");
        return;
    }
    glm::vec3 seatPos(0.0f);
    glm::vec3 forward(0.0f, 1.0f, 0.0f);
    if (!VehicleSystem::instance().seatWorld(pkt->vehicleId, pkt->seat,
                                             seatPos, forward))
    {
        VehicleSystem::instance().exitSeat(pkt->vehicleId, pkt->seat,
                                           player.id);
        reject("vehicle seat world transform is unavailable");
        return;
    }
    player.seatedVehicleId = pkt->vehicleId;
    player.seatedSeat = static_cast<int8_t>(pkt->seat);
    player.pos = seatPos;
    player.vel = glm::vec3(0.0f);
    player.onGround = false;
    player.input = {};
    player.attackQueued = false;
    player.lastVehicleSeatRequestSerial = pkt->requestSerial;
    broadcastSeatEvent(sock, players, pkt->vehicleId, player.id,
                       static_cast<int8_t>(pkt->seat), VEHICLE_SEAT_ENTER,
                       serverTick, totalPacketsOut);
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "vehicle.seat.server_enter", std::to_string(pkt->vehicleId),
        "server accepted a vehicle seat enter", serverTick,
        {{"vehicle_id", pkt->vehicleId},
         {"actor_id", player.id},
         {"seat", pkt->seat},
         {"position", {player.pos.x, player.pos.y, player.pos.z}}});
}

// ── Client processing ─────────────────────────────────────────────────

void mpProcessVehicleSpawnEventPacket(MultiplayerContext&,
                                      const VehicleSpawnEventPacket* event)
{
    const std::string templateId(event->templateId);
    const glm::quat orientation(event->orientation[0], event->orientation[1],
                                event->orientation[2], event->orientation[3]);
    const glm::mat4 transform =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(event->position[0], event->position[1],
                                 event->position[2])) *
        glm::mat4_cast(glm::normalize(orientation));
    std::vector<int> occupants(MAX_VEHICLE_SEATS, 0);
    const int seats = std::min<int>(MAX_VEHICLE_SEATS, event->seatCount);
    for (int i = 0; i < seats; ++i)
        occupants[i] = static_cast<int>(event->seatOccupants[i]);
    VehicleSystem::instance().spawnMirror(
        event->vehicleId, templateId, event->chassisNetworkId, transform,
        occupants);
}

void mpProcessVehicleDespawnEventPacket(MultiplayerContext&,
                                        const VehicleDespawnEventPacket* event)
{
    VehicleSystem::instance().despawn(event->vehicleId);
}

void mpProcessVehicleStatePacket(MultiplayerContext&,
                                 const VehicleStatePacket* event)
{
    const int count = std::min<int>(MAX_VEHICLE_STATE_ENTRIES, event->count);
    for (int i = 0; i < count; ++i)
    {
        const VehicleNetStateEntry& entry = event->entries[i];
        VehicleState* vehicle =
            VehicleSystem::instance().find(entry.vehicleId);
        if (!vehicle)
            continue;
        vehicle->engineOn = entry.engineOn != 0;
        vehicle->mode = static_cast<VehicleControlMode>(entry.mode);
        vehicle->gear = entry.gear;
        vehicle->engineRpm = entry.engineRpm;
        vehicle->speed = entry.speed;
        vehicle->boostCharge = entry.boostCharge;
        vehicle->throttle = entry.throttle;
        vehicle->brake = entry.brake;
        vehicle->steer = entry.steer;
        vehicle->clutch = entry.clutch;
        vehicle->handbrake = entry.handbrake;
        const int seats = std::min<int>(MAX_VEHICLE_SEATS, entry.seatCount);
        vehicle->seatOccupants.assign(seats, 0);
        for (int s = 0; s < seats; ++s)
            vehicle->seatOccupants[s] =
                static_cast<int>(entry.seatOccupants[s]);
        const int wheels = std::min<int>(
            MAX_VEHICLE_WHEELS,
            std::min<int>(entry.wheelCount,
                          static_cast<int>(vehicle->wheels.size())));
        for (int w = 0; w < wheels; ++w)
        {
            VehicleWheelState& wheel = vehicle->wheels[w];
            wheel.spinAngle = entry.wheels[w].spinAngle;
            wheel.spinVelocity = entry.wheels[w].spinVelocity;
            wheel.steerAngle = entry.wheels[w].steerAngle;
            wheel.compression = entry.wheels[w].compression;
            wheel.grounded = entry.wheels[w].grounded != 0;
            wheel.rayHit = wheel.grounded;
            wheel.suspensionLength =
                std::max(0.0f, wheel.compression);
        }
    }
}

void mpProcessVehicleSeatEventPacket(MultiplayerContext& ctx,
                                     const VehicleSeatEventPacket* event)
{
    VehicleState* vehicle =
        VehicleSystem::instance().find(event->vehicleId);
    if (!vehicle)
        return;
    if (event->action != VEHICLE_SEAT_ENTER &&
        event->action != VEHICLE_SEAT_EXIT)
    {
        if (event->actorId == ctx.localPlayerId)
            VehicleSystem::instance().clearSeatRequest();
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.seat.rejected.replicated", std::to_string(event->vehicleId),
            "server rejected a seat request", ctx.tick,
            {{"vehicle_id", event->vehicleId},
             {"actor_id", event->actorId},
             {"seat", event->seat}});
        return;
    }
    if (event->action == VEHICLE_SEAT_EXIT)
    {
        for (int& occupant : vehicle->seatOccupants)
            if (occupant == static_cast<int>(event->actorId))
                occupant = 0;
    }
    else if (event->seat >= 0 &&
             event->seat < static_cast<int>(vehicle->seatOccupants.size()))
    {
        vehicle->seatOccupants[event->seat] =
            static_cast<int>(event->actorId);
    }
    if (event->actorId != 0 &&
        event->actorId == ctx.localPlayerId)
    {
        if (event->action == VEHICLE_SEAT_ENTER)
        {
            VehicleSystem::instance().enterLocalDriver(
                event->vehicleId, event->seat, event->actorId);
        }
        else
        {
            VehicleSystem::instance().exitLocalDriver();
        }
        VehicleSystem::instance().clearSeatRequest();
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "vehicle.seat.replicated", std::to_string(event->vehicleId),
        event->action == VEHICLE_SEAT_ENTER ? "seat enter replicated"
                                            : "seat exit replicated",
        ctx.tick,
        {{"vehicle_id", event->vehicleId},
         {"actor_id", event->actorId},
         {"seat", event->seat},
         {"local", event->actorId == ctx.localPlayerId}});
}

void mpSendVehicleInput(MultiplayerContext& ctx, uint32_t currentMs)
{
    if (!ctx.connected || ctx.localPlayerId == 0)
        return;
    static uint32_t sLastVehicleInputMs = 0;
    if (currentMs - sLastVehicleInputMs < 15u)
        return;
    sLastVehicleInputMs = currentMs;

    VehicleSystem& vehicles = VehicleSystem::instance();
    if (vehicles.hasPendingSeatRequest())
    {
        static uint32_t sLastSeatRequestLogMs = 0;
        if (currentMs - sLastSeatRequestLogMs >= 250u)
        {
            sLastSeatRequestLogMs = currentMs;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Network, StructuredLevel::Important,
                "vehicle.seat.request.sent", "vehicle-seat",
                "client sending an authoritative seat request", ctx.tick,
                {{"vehicle_id", vehicles.pendingSeatVehicleId()},
                 {"seat", vehicles.pendingSeatIndex()},
                 {"exit", vehicles.pendingSeatIsExit()},
                 {"serial", vehicles.pendingSeatSerial()},
                 {"connected", ctx.connected},
                 {"local_player_id", ctx.localPlayerId}});
        }
        VehicleSeatRequestPacket seat{};
        seat.header.type = PACKET_VEHICLE_SEAT_REQUEST;
        seat.header.tick = ctx.tick;
        seat.header.playerId = ctx.localPlayerId;
        seat.requestSerial = vehicles.pendingSeatSerial();
        seat.vehicleId = vehicles.pendingSeatVehicleId();
        seat.seat = static_cast<int8_t>(vehicles.pendingSeatIndex());
        seat.action = vehicles.pendingSeatIsExit() ? VEHICLE_SEAT_EXIT
                                                   : VEHICLE_SEAT_ENTER;
        mpSendPacket(ctx, &seat, sizeof(seat));
    }

    if (!vehicles.hasLocalDriver())
        return;
    const uint32_t vehicleId = vehicles.localVehicleId();
    VehicleState* state = vehicles.find(vehicleId);
    if (!state || (!state->serverMirror && !state->clientPredicted))
        return;
    const VehicleInput input = vehicles.sampleLocalInput();
    static uint32_t sLastLocalInputLogMs = 0;
    static uint32_t sLastIdleSampleMs = 0;
    const bool hasInput =
        input.throttle > 0.01f || input.brake > 0.01f ||
        std::abs(input.steer) > 0.01f || input.clutch > 0.01f ||
        input.handbrake > 0.01f || std::abs(input.pitch) > 0.01f ||
        std::abs(input.yaw) > 0.01f || std::abs(input.roll) > 0.01f ||
        input.engineToggle || input.modeToggle ||
        input.shiftUp || input.shiftDown;
    // One unconditional 1 Hz sample. Without it a driver that produces no
    // input at all is invisible in the journal, which is exactly the state that
    // is hardest to diagnose ("I press and nothing happens").
    const bool idleSampleDue = currentMs - sLastIdleSampleMs >= 1000u;
    if (hasInput && currentMs - sLastLocalInputLogMs >= 250u)
    {
        sLastLocalInputLogMs = currentMs;
    }
    else if (!hasInput && !idleSampleDue)
    {
        return;
    }
    if (!hasInput)
        sLastIdleSampleMs = currentMs;
    {
        sLastLocalInputLogMs = currentMs;
        StructuredLogger::instance().writeEvent(
            StructuredCategory::Network, StructuredLevel::Important,
            "vehicle.input.local", std::to_string(vehicleId),
            hasInput ? "local seated driver input sampled"
                     : "local seated driver idle sample",
            ctx.tick,
            {{"vehicle_id", vehicleId},
             {"throttle", input.throttle},
             {"brake", input.brake},
             {"steer", input.steer},
             {"gamepad_connected", GamepadInput::instance().connected()},
             {"raw_throttle_axis", GamepadInput::instance().rawAxis(5)},
             {"raw_brake_axis", GamepadInput::instance().rawAxis(4)},
             {"focused", InputRouter::gameWindowFocused()},
             {"idle", !hasInput},
             {"pitch", input.pitch},
             {"yaw", input.yaw},
             {"roll", input.roll},
             {"clutch", input.clutch},
             {"handbrake", input.handbrake},
             {"boost", input.boost},
             {"jump_held", input.jumpHeld},
             {"powerslide", input.powerslide},
             {"engineToggle", input.engineToggle},
             {"modeToggle", input.modeToggle},
             {"shiftUp", input.shiftUp},
             {"shiftDown", input.shiftDown},
             {"gear", state->gear},
             {"mode", static_cast<int>(state->mode)},
             {"engineOn", state->engineOn}});
    }
    VehicleInputPacket pkt{};
    pkt.header.type = PACKET_VEHICLE_INPUT;
    pkt.header.tick = ctx.tick;
    pkt.header.playerId = ctx.localPlayerId;
    pkt.sequence = currentMs;
    pkt.vehicleId = vehicleId;
    pkt.throttle = input.throttle;
    pkt.brake = input.brake;
    pkt.steer = input.steer;
    pkt.pitch = input.pitch;
    pkt.yaw = input.yaw;
    pkt.roll = input.roll;
    pkt.clutch = input.clutch;
    pkt.handbrake = input.handbrake;
    pkt.boost = input.boost;
    if (input.jumpHeld) pkt.flags |= VEHICLE_INPUT_JUMP_HELD;
    if (input.powerslide) pkt.flags |= VEHICLE_INPUT_POWER_SLIDE;
    if (input.engineToggle) pkt.flags |= VEHICLE_INPUT_ENGINE_TOGGLE;
    if (input.modeToggle) pkt.flags |= VEHICLE_INPUT_MODE_TOGGLE;
    if (input.shiftUp) pkt.flags |= VEHICLE_INPUT_SHIFT_UP;
    if (input.shiftDown) pkt.flags |= VEHICLE_INPUT_SHIFT_DOWN;
    mpSendPacket(ctx, &pkt, sizeof(pkt));
}

} // namespace MimitaNet