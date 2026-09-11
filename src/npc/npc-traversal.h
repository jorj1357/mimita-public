// 09 10 2026
/* purpose
* Capability-aware traversal layer between the navigator and input generation.
* Given the navigator's next waypoint/path information, the actor's effective
* MovementConfig, and current body state, it chooses a traversal method
* (walk/jump/drop/dash/dash-jump), keeps it stable across ticks, and emits the
* matching input intent for the existing movement physics.
* Does NOT choose combat goals or compute paths.
* Does NOT simulate movement or write physics state.
*/
#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "npc/npc-navigator.h"

struct MovementConfig;
class Npc;

enum class TraversalType : uint8_t
{
    Walk = 0,
    Jump,
    Drop,
    Dash,
    DashJump,
};

const char* traversalTypeName(TraversalType type);

// Smallest useful traversal representation: what to do, where, and roughly
// which way. Direction is supplied by the navigator and reused by execution.
struct TraversalRequest
{
    TraversalType type = TraversalType::Walk;
    glm::vec3 targetPosition{0.0f};
    glm::vec3 desiredDirection{0.0f};
};

// Input intent produced for one tick of a traversal.
struct NpcTraversalStep
{
    glm::vec3 direction{0.0f};  // planar, normalized
    bool jump = false;
    bool dash = false;
    bool downDash = false;
    bool active = false;        // a traversal is currently running
    TraversalType type = TraversalType::Walk;
};

class NpcTraversalExecutor
{
public:
    NpcTraversalStep update(Npc& npc, const NpcNavResult& nav,
                            const MovementConfig* movement, float dt);
    void reset();

private:
    TraversalType select(const Npc& npc, const NpcNavResult& nav,
                         const MovementConfig& cfg, bool& feasible) const;
    void begin(const Npc& npc, TraversalType type, const glm::vec3& target);
    void finish(const Npc& npc, bool completed);
    void fail(Npc& npc, const char* reason);

    bool mActive = false;
    TraversalType mType = TraversalType::Walk;
    TraversalType mLastType = TraversalType::Walk;
    glm::vec3 mTarget{0.0f};
    int mTicks = 0;
    bool mJumpIssued = false;
    bool mDashIssued = false;
    float mLastDist = 1e18f;
    int mNoProgressTicks = 0;
    float mFailCooldown = 0.0f;
    float mDashBanTimer = 0.0f;
};
