#include "gamemode/arena-ball.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "impact/destructible-world-config.h"
#include "devtools/terminal.h"
#include "physics/physical-entity.h"

namespace ArenaBall
{
namespace
{
constexpr const char* kBallPersistenceId = "arena-ball";

// Ball test surface state. Declared here so spawnFromMap can arm the one-shot
// probe kick before the helpers below exist.
bool sHasPendingSet = false;
glm::vec3 sPendingPosition(0.0f);
glm::vec3 sPendingVelocity(0.0f);
// One-shot probe kick from MIMITA_ARENA_BALL_KICK="vx,vy,vz". The dev-loop ball
// probe launch mode sets it; without it the field is inert.
bool sProbeKickPending = false;
bool sProbeKickHasPosition = false;
glm::vec3 sProbeKickVelocity(0.0f);
glm::vec3 sProbeKickPosition(0.0f);

// Convert the authored ball gravity into the per-entity scale the shared
// object integrator uses. Keeps global object gravity untouched for crates.
float ballGravityScale(float gravityMps2)
{
    const float worldGravity = std::fabs(
        MimitaImpact::DestructibleWorldConfig::instance().objectGravity());
    if (worldGravity <= 0.01f)
        return 1.0f;
    return gravityMps2 / worldGravity;
}

void removeExistingBall()
{
    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    std::vector<uint32_t> doomed;
    for (const PhysicalEntity& entity : system.entities())
        if (entity.persistenceId == kBallPersistenceId)
            doomed.push_back(entity.id);
    for (uint32_t id : doomed)
        system.remove(id);
}
} // namespace

ArenaBallSpawn spawnFromMap(const MapObjectiveConfig& config, std::string* error)
{
    if (!config.arena.present || !config.arena.ball.present)
    {
        if (error) *error = "map has no arena ball";
        return 0;
    }
    const ArenaBallConfig& ball = config.arena.ball;
    const float radius = std::max(ball.radius, 0.001f);
    if (radius <= 0.0f || !std::isfinite(radius))
    {
        if (error) *error = "arena ball radius is invalid";
        return 0;
    }

    removeExistingBall();

    std::vector<CollisionTriangle> triangles;
    buildSphereCollisionTriangles(triangles, glm::vec3(0.0f), radius);

    glm::mat4 transform(1.0f);
    transform[3] = glm::vec4(ball.spawn, 1.0f);
    PhysicalEntitySystem& system = PhysicalEntitySystem::instance();
    const uint32_t id = system.add(triangles, transform,
                                   PhysicalEntityMotion::Dynamic, 0);
    PhysicalEntity* entity = system.find(id);
    if (!entity)
    {
        if (error) *error = "failed to create arena ball entity";
        return 0;
    }

    entity->persistenceId = kBallPersistenceId;
    entity->shape = PhysicalEntityShape::Sphere;
    entity->halfExtents = glm::vec3(radius);
    entity->mass = std::max(ball.massKg, 0.001f);
    const float volume = (4.0f / 3.0f) * glm::pi<float>() * radius * radius * radius;
    entity->density = std::max(entity->mass / volume, 0.001f);
    entity->restitution = ball.restitution;
    entity->friction = ball.friction;
    entity->linearDamping = ball.linearDamping;
    // Rocket League's ball has one decay: drag 0.03/s. Replace the global
    // object damping instead of adding to it.
    entity->linearDampingOverride = ball.linearDamping;
    entity->angularDamping = 0.4f;
    // A rolling sphere at the 60 m/s linear cap needs v/r = 65.7 rad/s, so a
    // 6 rad/s spin cap makes rolling impossible: the ball slides for the whole
    // roll and contact friction eats the speed (measured 4.0 m/s^2 instead of
    // the 0.03/s drag reference). The cap is set at the rolling requirement so
    // the ball can actually roll.
    entity->maxAngularSpeed = 66.0f;
    entity->gravityScale = ballGravityScale(ball.gravityMps2);
    entity->collidesWithActors = true;
    entity->motion = PhysicalEntityMotion::Dynamic;
    entity->serverSimulated = true;
    entity->replicate = true;
    entity->renderModel = false;
    entity->collisionBox = false;
    entity->velocity = glm::vec3(0.0f);
    entity->angularVelocity = glm::vec3(0.0f);
    entity->sleeping = false;
    entity->sleepTicks = 0;
    entity->sleepAnchorValid = false;
    refreshEntityMassProperties(*entity);

    if (const char* probe = std::getenv("MIMITA_ARENA_BALL_KICK"))
    {
        float vx = 0.0f;
        float vy = 0.0f;
        float vz = 0.0f;
        float px = 0.0f;
        float py = 0.0f;
        float pz = 0.0f;
        // Three values = velocity at the current position; six values =
        // position and velocity, so a probe can start away from map geometry.
        const int parsed = std::sscanf(probe, "%f,%f,%f,%f,%f,%f",
                                       &vx, &vy, &vz, &px, &py, &pz);
        if (parsed >= 3)
        {
            sProbeKickPending = true;
            sProbeKickVelocity = glm::vec3(vx, vy, vz);
            sProbeKickHasPosition = parsed == 6;
            sProbeKickPosition = glm::vec3(px, py, pz);
        }
    }

    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "arena.ball.spawned", std::to_string(entity->networkId),
        "server spawned the authored arena ball",
        0,
        nlohmann::json{
            {"network_id", entity->networkId},
            {"entity_id", entity->id},
            {"map", config.mapId},
            {"radius", radius},
            {"visual_radius", ball.visualRadius},
            {"mass", entity->mass},
            {"position", {ball.spawn.x, ball.spawn.y, ball.spawn.z}}},
        __FILE__, __LINE__, __FUNCTION__);
    return entity->networkId;
}

bool reset(PhysicalEntity& ball, const MapObjectiveConfig& config)
{
    if (!config.arena.present || !config.arena.ball.present)
        return false;
    const ArenaBallConfig& data = config.arena.ball;
    const float radius = std::max(data.radius, 0.001f);
    ball.shape = PhysicalEntityShape::Sphere;
    ball.halfExtents = glm::vec3(radius);
    ball.mass = std::max(data.massKg, 0.001f);
    ball.restitution = data.restitution;
    ball.friction = data.friction;
    ball.linearDamping = data.linearDamping;
    ball.linearDampingOverride = data.linearDamping;
    ball.gravityScale = ballGravityScale(data.gravityMps2);
    ball.transform = glm::mat4(1.0f);
    ball.transform[3] = glm::vec4(data.spawn, 1.0f);
    ball.previousTransform = ball.transform;
    ball.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    ball.velocity = glm::vec3(0.0f);
    ball.angularVelocity = glm::vec3(0.0f);
    ball.sleeping = false;
    ball.sleepTicks = 0;
    ball.sleepAnchorValid = false;
    ball.supportGraceTicks = 0;
    ball.lastWallContactTick = -1.0f;
    ball.lastInteractionTick = 0;
    refreshEntityMassProperties(ball);
    return true;
}
} // namespace ArenaBall

namespace ArenaBall
{
PhysicalEntity* find()
{
    for (PhysicalEntity& entity : PhysicalEntitySystem::instance().entities())
        if (entity.persistenceId == kBallPersistenceId)
            return &entity;
    return nullptr;
}

void setState(PhysicalEntity& ball, const glm::vec3& position,
              const glm::vec3& velocity)
{
    // Same discipline as reset(): the ball must never keep a stale contact,
    // sleep anchor, or previous transform from the pose it was moved out of.
    ball.transform = glm::mat4(1.0f);
    ball.transform[3] = glm::vec4(position, 1.0f);
    ball.previousTransform = ball.transform;
    ball.velocity = velocity;
    ball.angularVelocity = glm::vec3(0.0f);
    ball.sleeping = false;
    ball.sleepTicks = 0;
    ball.sleepAnchorValid = false;
    ball.supportGraceTicks = 0;
    ball.lastWallContactTick = -1.0f;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Physics, StructuredLevel::Important,
        "arena.ball.test.set", std::to_string(ball.networkId),
        "arena ball placed by the test surface",
        0,
        nlohmann::json{{"network_id", ball.networkId},
                       {"position", {position.x, position.y, position.z}},
                       {"velocity", {velocity.x, velocity.y, velocity.z}},
                       {"speed", glm::length(velocity)}},
        __FILE__, __LINE__, __FUNCTION__);
}

void requestState(const glm::vec3& position, const glm::vec3& velocity)
{
    if (PhysicalEntity* ball = find())
    {
        setState(*ball, position, velocity);
        return;
    }
    sHasPendingSet = true;
    sPendingPosition = position;
    sPendingVelocity = velocity;
}

void tick()
{
    if (sProbeKickPending)
    {
        if (PhysicalEntity* ball = find())
        {
            sProbeKickPending = false;
            const glm::vec3 position =
                sProbeKickHasPosition ? sProbeKickPosition
                                      : glm::vec3(ball->transform[3]);
            setState(*ball, position, sProbeKickVelocity);
        }
        return;
    }
    if (!sHasPendingSet)
        return;
    PhysicalEntity* ball = find();
    if (!ball)
        return;
    sHasPendingSet = false;
    setState(*ball, sPendingPosition, sPendingVelocity);
}

namespace
{
bool parseFloatArg(const std::string& text, float& out)
{
    try
    {
        size_t used = 0;
        out = std::stof(text, &used);
        return used == text.size();
    }
    catch (...)
    {
        return false;
    }
}
} // namespace

void registerArenaBallCommands()
{
    Terminal::instance().registerCommand({
        "ball_set",
        "Place the arena ball with an optional velocity (server authority)",
        "ball_set <x> <y> <z> [vx vy vz]",
        [](const std::vector<std::string>& args) {
            glm::vec3 position(0.0f);
            glm::vec3 velocity(0.0f);
            const bool ok =
                (args.size() == 3 || args.size() == 6) &&
                parseFloatArg(args[0], position.x) &&
                parseFloatArg(args[1], position.y) &&
                parseFloatArg(args[2], position.z) &&
                (args.size() < 6 || (parseFloatArg(args[3], velocity.x) &&
                                     parseFloatArg(args[4], velocity.y) &&
                                     parseFloatArg(args[5], velocity.z)));
            if (!ok)
            {
                Terminal::instance().addLog(
                    "[BALL] usage: ball_set <x> <y> <z> [vx vy vz]");
                return;
            }
            requestState(position, velocity);
            Terminal::instance().addLog(
                "[BALL] placed x=" + std::to_string(position.x) +
                " y=" + std::to_string(position.y) +
                " z=" + std::to_string(position.z) +
                " speed=" + std::to_string(glm::length(velocity)));
        }
    });

    Terminal::instance().registerCommand({
        "ball_status",
        "Report the arena ball position, velocity and speed",
        "ball_status",
        [](const std::vector<std::string>&) {
            PhysicalEntity* ball = find();
            if (!ball)
            {
                Terminal::instance().addLog("[BALL] no arena ball in this map");
                return;
            }
            const glm::vec3 position(ball->transform[3]);
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Physics, StructuredLevel::Important,
                "arena.ball.status", std::to_string(ball->networkId),
                "arena ball state on request", 0,
                nlohmann::json{{"network_id", ball->networkId},
                               {"position",
                                {position.x, position.y, position.z}},
                               {"velocity",
                                {ball->velocity.x, ball->velocity.y,
                                 ball->velocity.z}},
                               {"speed", glm::length(ball->velocity)},
                               {"angular_speed",
                                glm::length(ball->angularVelocity)},
                               {"sleeping", ball->sleeping}},
                __FILE__, __LINE__, __FUNCTION__);
            Terminal::instance().addLog(
                "[BALL] speed=" + std::to_string(glm::length(ball->velocity)));
        }
    });
}
} // namespace ArenaBall
