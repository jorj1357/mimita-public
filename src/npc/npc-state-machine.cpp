#include "npc-state-machine.h"
#include "npc.h"
#include "npc-navigation.h"

#include <algorithm>
#include <glm/gtc/constants.hpp>

#include "config.h"
#include "debug/debug-log.h"
#include "npc/npc-internal.h"
#include "npc/npc-mind.h"
#include "combat/weapon-registry.h"
#include "ecs/actor-entities.h"
#include "hot-reload/game-api.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-npc-state-select.h"

float stateMinTime(NpcState s, float d01)
{
    (void)d01;
    switch (s)
    {
        case NpcState::Idle:        return 0.3f;
        case NpcState::RandomWalk:  return 0.5f;
        case NpcState::Chase:       return 0.3f;
        case NpcState::Circle:      return 0.3f;
        case NpcState::Strafe:      return 0.3f;
        case NpcState::Retreat:     return 0.2f;
        case NpcState::Attack:      return 0.08f;
        case NpcState::Recover:     return 0.1f;
        case NpcState::Advance:     return 0.3f;
        case NpcState::HoldPosition: return 0.3f;
        case NpcState::Peek:        return 0.3f;
        case NpcState::Aim:         return 0.15f;
        case NpcState::ZigZag:      return 0.4f;
    }
    return 0.2f;
}

float stateMaxTime(NpcState s, float d01)
{
    float base = 1.0f - d01 * 0.5f;
    switch (s)
    {
        case NpcState::Idle:        return 3.0f * base;
        case NpcState::RandomWalk:  return 4.0f * base;
        case NpcState::Chase:       return 2.0f * base;
        case NpcState::Circle:      return 3.0f * base;
        case NpcState::Strafe:      return 2.5f * base;
        case NpcState::Retreat:     return 1.5f * base;
        case NpcState::Attack:      return 0.3f;
        case NpcState::Recover:     return 0.5f * base;
        case NpcState::Advance:     return 2.0f * base;
        case NpcState::HoldPosition: return 2.5f * base;
        case NpcState::Peek:        return 2.0f * base;
        case NpcState::Aim:         return 1.5f * base;
        case NpcState::ZigZag:      return 2.5f * base;
    }
    return 1.5f * base;
}

std::string npcStateName(NpcState s)
{
    switch (s)
    {
        case NpcState::Idle:        return "IDLE";
        case NpcState::RandomWalk:  return "RANDOMWALK";
        case NpcState::Chase:       return "CHASE";
        case NpcState::Circle:      return "CIRCLE";
        case NpcState::Strafe:      return "STRAFE";
        case NpcState::Retreat:     return "RETREAT";
        case NpcState::Attack:      return "ATTACK";
        case NpcState::Recover:     return "RECOVER";
        case NpcState::Advance:     return "ADVANCE";
        case NpcState::HoldPosition: return "HOLD";
        case NpcState::Peek:        return "PEEK";
        case NpcState::Aim:         return "AIM";
        case NpcState::ZigZag:      return "ZIGZAG";
    }
    return "UNKNOWN";
}

NpcState pickNextState(Npc& npc)
{
    const float d01 = difficulty01(npc.difficulty);
    const auto& sensors = npc.sensors;

    // State selection is a hot policy (npc.state-select). Cold owns the
    // world/navigation query, the mind scalars, and weapon range; the hot policy
    // owns the scoring, randomness, and guards. Persistent stuck timer + RNG ride
    // in/out so the policy is stateless and reload-safe.
    NpcStateSelectPolicyV1 request{};
    request.structSize = sizeof(NpcStateSelectPolicyV1);
    request.entity = Ecs::raw(
        Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, npc.id));
    request.currentState = (std::uint32_t)npc.stateMachine.currentState;
    request.hasTarget = sensors.hasTarget ? 1u : 0u;
    request.isStuck = NpcNavigation::isStuck(npc) ? 1u : 0u;
    request.distance = sensors.targetDistance;
    request.difficulty01 = d01;
    request.effectiveAggression =
        npcMindEffectiveAggression(npc, npc.tuning.aggression);
    request.weaponRange = weaponEffectiveRange(npc);
    request.preferredRange =
        (npc.behavior.active && npc.behavior.preferredRange > 0.0f)
            ? npc.behavior.preferredRange : 0.0f;
    request.retreatBonus = npcMindRetreatBonus(npc);
    request.attackCooldown = npc.attackCooldown;
    request.hitReactionTimer = npc.hitReactionTimer;
    request.lastKnownAge = npc.stateMachine.lastKnownAge;
    request.distToLastKnown =
        glm::length(npc.stateMachine.lastKnownTarget - npc.body.pos);
    request.retreatTimer = npc.stateMachine.retreatTimer;
    request.aggressionTuning = npc.tuning.aggression;
    request.randomnessScale = 1.0f;
    request.stuckTimer = npc.stateMachine.stuckTimer;
    request.rngState = npc.rngState;

    auto* stateFn = reinterpret_cast<GameNpcStateSelectFn>(
        MimitaRuntime::GenericRuntime::instance().capability(
            GAME_CAP_NPC_STATE_SELECT));
    if (stateFn)
        stateFn(nullptr, &request);
    else
        MimitaNet::HotNpcStateSelectImpl::evaluate(request);

    npc.stateMachine.stuckTimer = request.stuckTimer;
    npc.rngState = request.rngState;
    return static_cast<NpcState>(request.chosenState);
}
