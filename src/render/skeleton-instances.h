// 09 15 2026
/* purpose
* Cold generic per-entity skeleton instance. Keyed by EntityId (never by a
* Player or Npc pointer), it holds the local/world bone transforms that the generic
* `skeleton.apply` capability writes from hot PoseState. The GPU skinning/draw
* mechanism stays here (cold); hot code owns which pose. No DLL pointers are
* stored. A missing or dead entity is ignored.
* Does NOT own animation policy, resources, or draw submission.
*/
#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "ecs/entity-types.h"
#include "hot-reload/game-api.h"

namespace SkeletonInstances {

struct BonePose {
    std::uint64_t part = 0;       // gameHash("leftArm") etc.
    glm::mat4 local{1.0f};
    glm::mat4 world{1.0f};
};

struct Instance {
    EntityId entity = kInvalidEntityId;
    std::uint32_t boneCount = 0;
    std::uint32_t version = 0;
    BonePose bones[GAME_MAX_POSE_PARTS];
};

// Create or return the skeleton instance for an entity. Identity pose initially.
Instance* ensure(EntityId entity);
// Return the instance only if the entity is still alive.
Instance* get(EntityId entity);
// Map a generic pose onto the entity's skeleton instance and bump its version.
// Returns false when the entity is invalid.
bool applyPose(EntityId entity, const GameSkeletonPoseV1& pose);
// Drop instances whose entity has been destroyed.
void purgeDead();
void clear();
std::size_t count();

} // namespace SkeletonInstances
