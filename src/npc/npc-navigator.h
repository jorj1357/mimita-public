// 09 10 2026
/* purpose
* Per-NPC navigation layer: turns an abstract NpcGoal into a cached local route
* and returns a steering direction plus traversal requirements.
* Uses a bounded rolling-horizon ground grid around the actor (the maps are far
* too large for a global graph in this slice) and repaths on a timer, on
* significant goal movement, or when stuck.
* Does NOT decide combat, pick goals, or apply physics/input. Movement execution
* stays in npc.cpp through the shared kernel.
* Does NOT own world collision or movement configuration.
*/
#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "npc/npc-goal.h"

struct World;
class Npc;
struct MovementConfig;

struct NpcNavResult
{
    glm::vec3 dir{0.0f};        // planar steering direction, zero if none
    glm::vec3 waypoint{0.0f};   // current navigation target
    glm::vec3 destination{0.0f};// goal-resolved destination point
    bool wantJump = false;      // next traversal needs a jump
    bool wantDownDash = false;  // next traversal is a drop
    bool hasPath = false;       // following a cached multi-node route
    bool detour = false;        // route deviates from the direct line to dest
    int pathNodes = 0;          // waypoints remaining on the cached route
};

struct NpcNavigator
{
    NpcGoal goal;
    std::vector<glm::vec3> path;
    int pathIndex = 0;
    float repathTimer = 0.0f;
    glm::vec3 lastGoal{0.0f};
    bool hasLastGoal = false;

    // Diagnostics.
    uint32_t planCount = 0;
    uint32_t repathCount = 0;

    // Called from the NPC update after sensing and goal selection.
    NpcNavResult update(Npc& npc, const NpcGoal& goal, const World& world,
                        const MovementConfig* movement, float dt);

    void reset();
    // Force a replan on the next update (e.g. target teleported).
    void requestRepath() { repathTimer = 0.0f; hasLastGoal = false; }
};
