// 10 02 2026
/* purpose
* Generalize navigation input/output without replacing the local A* navigator.
* A NavigationRequest bundles start/destination with the actor's movement
* capabilities; a helper converts it to the existing NpcGoal the navigator
* already consumes, so behavior code never implements wall/slope/collision
* logic.
* Does NOT path, steer, or apply physics. The rolling local ground planner
* remains the one implementation for Counter-Strike.
*/
#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "npc/npc-goal.h"

// What an actor can physically do. Defaults match the current Counter-Strike
// roster: walk, jump, drop; no run/fly/climb.
struct MovementCapabilities {
    bool canWalk = true;
    bool canRun = false;
    bool canJump = true;
    bool canDrop = true;
    bool canFly = false;
    bool canClimb = false;
};

// A behavior-code navigation request. `goal` classifies intent (reach a point,
// follow, maintain distance, flee); capabilities describe feasibility.
struct NavigationRequest {
    glm::vec3 start{0.0f};
    glm::vec3 destination{0.0f};
    NpcGoalKind goal = NpcGoalKind::ReachPosition;
    uint32_t targetActorId = 0;
    float desiredDistance = 0.0f;
    float tolerance = 1.2f;
    MovementCapabilities capabilities;
};

// Convert a request into the existing NpcGoal the navigator consumes. This is
// the single adapter; callers do not build goals by hand.
NpcGoal navigationRequestToGoal(const NavigationRequest& request);

// Segment classification helpers used by behavior code and tests.
enum class NavSegmentType : uint8_t {
    Walk = 0,
    Jump,
    Drop,
    Gap,
    Unreachable
};

// Classify the vertical relationship between two points under the given
// capabilities and maximum jump height. Pure; no world query.
NavSegmentType classifySegment(float heightDelta,
                               bool hasGap,
                               const MovementCapabilities& caps,
                               float maxJumpHeight);

// World-independent selftest for the request adapter and segment classifier.
bool npcNavRequestSelfTest(std::string& report);
