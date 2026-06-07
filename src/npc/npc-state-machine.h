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
    Recover
};

std::string npcStateName(NpcState s);
NpcState pickNextState(Npc& npc);
void computeStateMovement(Npc& npc, glm::vec3& outMoveDir, bool& outJump, bool& outDash, bool& outAttack);

float stateMinTime(NpcState s, float d01);
float stateMaxTime(NpcState s, float d01);

struct NpcStateMachine
{
    NpcState currentState = NpcState::Idle;
    NpcState previousState = NpcState::Idle;
    float stateTimer = 0.0f;
    float nextDecisionTime = 0.0f;

    // Circle orbit
    float orbitAngle = 0.0f;
    float orbitDirection = 1.0f;
    float orbitDistance = 6.0f;
    float orbitSwapTimer = 0.0f;

    // Strafe
    float strafeDirection = 1.0f;
    float strafeSwapTimer = 0.0f;

    // Retreat forced timeout
    float retreatTimer = 0.0f;

    // Recover
    float recoverTimer = 0.0f;

    // Wander
    glm::vec3 wanderTarget{0.0f};
    float wanderTimer = 0.0f;

    // Last known player info (for when target is temporarily lost)
    glm::vec3 lastKnownTarget{0.0f};
    float lastKnownAge = 0.0f;

    // Stuck handling
    glm::vec3 stuckUnstickDir{1.0f, 0.0f, 0.0f};
    float stuckTimer = 0.0f;
};
