#pragma once

#include <string>
#include <glm/glm.hpp>

class Npc;
struct NpcStateMachine;

enum class NpcState
{
    Idle,
    RandomWalk,
    Chase,
    Circle,
    Strafe,
    Retreat,
    Attack,
    Recover,
    Advance,
    HoldPosition,
    Peek,
    Aim,
    ZigZag,
    Patrol  // no target: keep advancing forward, re-routing on walls/new ground
};

std::string npcStateName(NpcState s);
NpcState pickNextState(Npc& npc);
void computeStateMovement(Npc& npc, glm::vec3& outMoveDir, bool& outJump, bool& outDash, bool& outAttack, float dt);

float stateMinTime(NpcState s, float d01);
float stateMaxTime(NpcState s, float d01);

struct NpcStateMachine
{
    NpcState currentState = NpcState::Idle;
    NpcState previousState = NpcState::Idle;
    float stateTimer = 0.0f;
    float nextDecisionTime = 0.0f;

    float orbitAngle = 0.0f;
    float orbitDirection = 1.0f;
    float orbitDistance = 6.0f;
    float orbitSwapTimer = 0.0f;

    float strafeDirection = 1.0f;
    float strafeSwapTimer = 0.0f;

    float retreatTimer = 0.0f;

    float recoverTimer = 0.0f;

    glm::vec3 wanderTarget{0.0f};
    float wanderTimer = 0.0f;

    glm::vec3 lastKnownTarget{0.0f};
    float lastKnownAge = 0.0f;

    glm::vec3 stuckUnstickDir{1.0f, 0.0f, 0.0f};
    float stuckTimer = 0.0f;

    // Advance state
    float advanceTimer = 0.0f;

    // Peek state
    float peekDir = 1.0f;
    float peekTimer = 0.0f;
    glm::vec3 peekStartPos{0.0f};

    // ZigZag state
    float zigPhase = 0.0f;
    float zigTimer = 0.0f;

    // Hold position
    float holdTimer = 0.0f;

    // ── Search / patrol memory ─────────────────────────────────────
    // A persistent forward heading plus rings of recently visited and recently
    // blocked world points, so search never doubles back over ground it just
    // covered and never re-tests the same wall. No map knowledge is used: a new
    // heading is only chosen when the current one is blocked, when it has been
    // committed for long enough, or when the actor stops making progress.
    struct SearchPoint
    {
        glm::vec3 pos{0.0f};
        float time = 0.0f;
    };

    glm::vec3 patrolDir{0.0f};
    float patrolRepathTimer = 0.0f;   // <=0 => choose a new forward heading now
    static constexpr int PATROL_RECENT_MAX = 32;
    SearchPoint patrolVisited[PATROL_RECENT_MAX];
    int patrolVisitedCount = 0;
    int patrolVisitedHead = 0;
    SearchPoint patrolBlocked[PATROL_RECENT_MAX];
    int patrolBlockedCount = 0;
    int patrolBlockedHead = 0;
    float patrolSnapshotTimer = 0.0f; // one visited snapshot per configured interval

    // Generic no-progress watchdog, shared by search and chase. When the actor
    // cannot move for too long it blacklists the spot and either re-picks a
    // search heading or takes a short lateral detour before resuming.
    float patrolNoProgressTimer = 0.0f;
    glm::vec3 patrolLastProgressPos{0.0f};
    int patrolForcedDetourTicks = 0;
    glm::vec3 patrolForcedDetourDir{0.0f};

    // Continue-pursuit search begins only after reaching the last-known point.
    // The profile supplies the duration in fixed 60 Hz ticks.
    bool pursuitSearchActive = false;
    float pursuitSearchTimer = 0.0f;
    glm::vec3 pursuitSearchDir{0.0f};
};
