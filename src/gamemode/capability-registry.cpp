// Generic JSON-defined gamemode runtime - capability allowlist.
//
// One owner for the set of capability ids the compiled server can execute.
// The mode-pack registry validates every manifest entry against this list, so
// a typo or an unsupported capability is rejected at load, not at match time.

#include "gamemode/capability-registry.h"

#include <algorithm>

namespace MimitaGamemode {

CapabilityRegistry& CapabilityRegistry::instance()
{
    static CapabilityRegistry registry;
    return registry;
}

CapabilityRegistry::CapabilityRegistry()
{
    mKnown = {
        Cap::kLifecycleIntermissionCountdown,
        Cap::kParticipantsAllActors,
        Cap::kInventoryRandomPerActor,
        Cap::kWinLastActorAlive,
        Cap::kHazardSpawn,
        Cap::kDamageArea,
        Cap::kObjectiveFinishVolume,
        Cap::kPresentationDisasterBanner,
    };
    std::sort(mKnown.begin(), mKnown.end());
}

bool CapabilityRegistry::isKnown(const std::string& id) const
{
    return std::binary_search(mKnown.begin(), mKnown.end(), id);
}

std::string CapabilityRegistry::familyOf(const std::string& id) const
{
    const size_t dot = id.find('.');
    return dot == std::string::npos ? id : id.substr(0, dot);
}

} // namespace MimitaGamemode
