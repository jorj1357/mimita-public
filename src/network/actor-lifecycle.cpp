#include "network/actor-lifecycle.h"

#include "debug/debug-log.h"
#include "config/spawn-velocity-config.h"

#include <cmath>

namespace MimitaNet {

void emitActorSpawnEvent(const ActorSpawnEvent& event, const char* actorName)
{
    const char* kind = event.actorKind == ActorKind::Player ? "player" : "npc";
    Debug::log(Debug::Category::Networking,
        "[ACTOR SPAWN] entity=%u kind=%s name=%s reason=%u generation=%u "
        "epoch=%u tick=%u pos=(%.3f,%.3f,%.3f) look=(%.3f,%.3f,%.3f) "
        "velocity=(%.3f,%.3f,%.3f)\n",
        event.entityId, kind, actorName ? actorName : "-",
        static_cast<unsigned>(event.reason), event.spawnGeneration,
        static_cast<unsigned>(event.transformEpoch), event.serverTick,
        event.position.x, event.position.y, event.position.z,
        event.lookDirection.x, event.lookDirection.y, event.lookDirection.z,
        event.velocity.x, event.velocity.y, event.velocity.z);
}

ActorSpawnEvent finalizeActorSpawn(ActorSpawnEvent event, const char* actorName)
{
    const float yaw = std::atan2(event.lookDirection.y, event.lookDirection.x);
    event.lookDirection = glm::vec3(std::cos(yaw), std::sin(yaw), 0.0f);
    event.velocity = SpawnVelocityConfig::instance().enabled()
        ? SpawnVelocityConfig::instance().computeSpawnImpulse(glm::degrees(yaw))
        : glm::vec3(0.0f);
    emitActorSpawnEvent(event, actorName);
    return event;
}

} // namespace MimitaNet
