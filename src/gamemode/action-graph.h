// Generic JSON-defined gamemode runtime - fixed-tick action graph.
//
// Purpose: turn a mode manifest's declared capabilities into an ordered set of
// scheduled actions the server can evaluate at a fixed 60 Hz. The graph is
// pure data + pure queries; it never names a mode and never executes gameplay.
//
// The phase values intentionally mirror DuelStatePhase (packets.h) so the
// server passes its phase directly. They are re-declared here so this module
// stays free of the networking header for standalone tests.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "gamemode/mode-pack.h"

namespace MimitaGamemode {

enum class ActionPhase : uint8_t
{
    Waiting = 0,
    Countdown = 1,
    Active = 2,
    MatchEnd = 3,
    Intermission = 4,
    Results = 6,
    Go = 7,
    Any = 255
};

enum class ActionTrigger : uint8_t
{
    MatchStart = 0,      // fires once when the match enters Active
    PhaseEnter = 1,      // fires once when the target phase is entered
    Interval = 2,        // fires every intervalTicks while in the target phase
    ElapsedSeconds = 3,  // fires once at elapsedSeconds into the target phase
    Completion = 4,      // fires when a capability reports success
    Failure = 5          // fires when a capability reports failure
};

struct ScheduledAction
{
    std::string capabilityId;
    ActionTrigger trigger = ActionTrigger::MatchStart;
    uint8_t phase = (uint8_t)ActionPhase::Any;
    float elapsedSeconds = 0.0f;
    uint32_t intervalTicks = 0;
};

// One resolved plan for a match. Built from a manifest before the match starts;
// the server executes the matching capability handlers without checking the
// mode name.
class ActionGraph
{
public:
    static ActionGraph build(const ModePack& pack);

    bool empty() const { return mActions.empty(); }
    const std::vector<ScheduledAction>& actions() const { return mActions; }

    bool hasCapability(const std::string& capabilityId) const;

    // Capability ids whose action should run when `phase` begins. Only the
    // graph's phase-entry / match-start actions are considered; interval and
    // elapsed actions are evaluated per tick by the owner with `dueAt`.
    std::vector<std::string> onPhaseEnter(uint8_t phase) const;

    // Capability ids due this fixed tick given the phase and the tick the
    // current phase began. Interval actions fire on `phaseStartTick` and every
    // `intervalTicks` after; elapsed actions fire once the elapsed tick count
    // is reached.
    std::vector<std::string> dueAt(uint8_t phase, uint32_t tick,
                                   uint32_t phaseStartTick) const;

private:
    std::vector<ScheduledAction> mActions;
};

} // namespace MimitaGamemode
