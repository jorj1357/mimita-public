#include "gamemode/arena-ball.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "debug/structured-log.h"
#include "gamemode/map-config.h"
#include "physics/physical-entity.h"

namespace ArenaBall
{
namespace
{
constexpr const char* kBallPersistenceId = "arena-ball";

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
    entity->angularDamping = 0.4f;
    entity->maxAngularSpeed = 40.0f;
    entity->gravityScale = 1.0f;
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
