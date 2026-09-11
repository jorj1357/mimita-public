// 09 10 2026
/* purpose
* Define the abstract movement goal an NPC brain can request of navigation.
* Goals describe intent only (reach/follow/maintain/flee/LOS); they do not
* contain pathfinding, steering, or physical input.
* Does NOT implement navigation, pathfinding, or movement execution.
*/
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

enum class NpcGoalKind : uint8_t
{
    None = 0,
    ReachPosition,       // reach targetPos
    FollowActor,         // reach the current target actor
    MaintainDistance,    // hold desiredDistance from the current target actor
    FleeActor,           // move away from the current target actor
    ReachLineOfSight,    // move until the current target actor is visible
};

struct NpcGoal
{
    NpcGoalKind kind = NpcGoalKind::None;
    uint32_t targetActorId = 0;
    glm::vec3 targetPos{0.0f};        // used by ReachPosition (and debug)
    float desiredDistance = 0.0f;     // used by MaintainDistance / FleeActor
    float tolerance = 1.2f;           // arrival tolerance

    bool valid() const { return kind != NpcGoalKind::None; }
};
