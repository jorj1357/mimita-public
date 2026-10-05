// Generic JSON-defined gamemode runtime - fixed-tick action graph.
//
// Builds a bounded, ordered action list from a manifest and answers pure
// scheduling queries. Capability-specific scheduling lives here so the server
// only asks "what is due now?", never "what mode is this?".

#include "gamemode/action-graph.h"

#include <algorithm>

#include "gamemode/capability-registry.h"

namespace MimitaGamemode {

namespace {

// Default phase/trigger policy for each known capability. Keeping it here means
// a manifest only names capabilities; the reusable runtime decides when each
// fires. Unknown capabilities are rejected by the loader before this point.
bool defaultScheduleFor(const std::string& capabilityId, ScheduledAction& out)
{
    using namespace Cap;
    if (capabilityId == kLifecycleIntermissionCountdown) {
        out.trigger = ActionTrigger::PhaseEnter;
        out.phase = (uint8_t)ActionPhase::Intermission;
        return true;
    }
    if (capabilityId == kParticipantsAllActors) {
        out.trigger = ActionTrigger::MatchStart;
        out.phase = (uint8_t)ActionPhase::Active;
        return true;
    }
    if (capabilityId == kInventoryRandomPerActor) {
        out.trigger = ActionTrigger::MatchStart;
        out.phase = (uint8_t)ActionPhase::Active;
        return true;
    }
    if (capabilityId == kWinLastActorAlive) {
        out.trigger = ActionTrigger::Interval;
        out.phase = (uint8_t)ActionPhase::Active;
        out.intervalTicks = 1;
        return true;
    }
    if (capabilityId == kPresentationDisasterBanner) {
        out.trigger = ActionTrigger::PhaseEnter;
        out.phase = (uint8_t)ActionPhase::Countdown;
        return true;
    }
    if (capabilityId == kHazardSpawn || capabilityId == kDamageArea ||
        capabilityId == kObjectiveFinishVolume) {
        out.trigger = ActionTrigger::Interval;
        out.phase = (uint8_t)ActionPhase::Active;
        out.intervalTicks = 60;
        return true;
    }
    return false;
}

} // namespace

ActionGraph ActionGraph::build(const ModePack& pack)
{
    ActionGraph graph;

    // Preserve manifest declaration order; that order is the execution order
    // for same-tick actions.
    for (const std::string& capabilityId : pack.capabilities) {
        ScheduledAction action;
        action.capabilityId = capabilityId;
        if (!defaultScheduleFor(capabilityId, action)) {
            // The loader rejects unknown capabilities, so this is defensive.
            continue;
        }
        graph.mActions.push_back(std::move(action));
    }

    // A declared disaster always schedules a deterministic weapon assignment
    // and a last-actor-alive win check when the pack did not list them.
    if (!pack.disasters.empty()) {
        if (!graph.hasCapability(Cap::kInventoryRandomPerActor)) {
            ScheduledAction shield;
            shield.capabilityId = Cap::kInventoryRandomPerActor;
            shield.trigger = ActionTrigger::MatchStart;
            shield.phase = (uint8_t)ActionPhase::Active;
            graph.mActions.push_back(std::move(shield));
        }
        if (!graph.hasCapability(Cap::kWinLastActorAlive)) {
            ScheduledAction win;
            win.capabilityId = Cap::kWinLastActorAlive;
            win.trigger = ActionTrigger::Interval;
            win.phase = (uint8_t)ActionPhase::Active;
            win.intervalTicks = 1;
            graph.mActions.push_back(std::move(win));
        }
    }
    return graph;
}

bool ActionGraph::hasCapability(const std::string& capabilityId) const
{
    return std::any_of(mActions.begin(), mActions.end(),
        [&capabilityId](const ScheduledAction& a) {
            return a.capabilityId == capabilityId;
        });
}

std::vector<std::string> ActionGraph::onPhaseEnter(uint8_t phase) const
{
    std::vector<std::string> out;
    for (const ScheduledAction& action : mActions) {
        if (action.phase != phase)
            continue;
        if (action.trigger == ActionTrigger::MatchStart ||
            action.trigger == ActionTrigger::PhaseEnter)
            out.push_back(action.capabilityId);
    }
    return out;
}

std::vector<std::string> ActionGraph::dueAt(uint8_t phase, uint32_t tick,
                                            uint32_t phaseStartTick) const
{
    std::vector<std::string> out;
    const uint32_t elapsed = tick >= phaseStartTick ? tick - phaseStartTick : 0;
    for (const ScheduledAction& action : mActions) {
        if (action.phase != phase)
            continue;
        if (action.trigger == ActionTrigger::Interval) {
            if (action.intervalTicks > 0 && (elapsed % action.intervalTicks) == 0)
                out.push_back(action.capabilityId);
        } else if (action.trigger == ActionTrigger::ElapsedSeconds) {
            const uint32_t target = (uint32_t)(action.elapsedSeconds * 60.0f);
            if (elapsed == target)
                out.push_back(action.capabilityId);
        }
    }
    return out;
}

} // namespace MimitaGamemode
