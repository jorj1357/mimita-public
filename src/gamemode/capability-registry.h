// Generic JSON-defined gamemode runtime - capability identifiers.
//
// Purpose: one stable vocabulary that a mode manifest and the compiled server
// runtime both use. A manifest names capabilities; the server resolves them to
// compiled handlers. Capability ids describe reusable behavior and must never
// identify an individual mode name ("ffa", "tdm", ...).
//
// This file is data only. It does NOT execute behavior and does NOT own match
// state; the server runtime and the disaster runtime own execution.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MimitaGamemode {

// Stable capability ids. Add new reusable capabilities here, then register the
// compiled handler in the owning runtime. Never add a mode name.
namespace Cap {
inline constexpr const char* kLifecycleIntermissionCountdown = "lifecycle.intermission_countdown";
inline constexpr const char* kParticipantsAllActors = "participants.all_actors";
inline constexpr const char* kInventoryRandomPerActor = "inventory.random_per_actor";
inline constexpr const char* kWinLastActorAlive = "win.last_actor_alive";
inline constexpr const char* kHazardSpawn = "hazard.spawn";
inline constexpr const char* kDamageArea = "damage.area";
inline constexpr const char* kObjectiveFinishVolume = "objective.finish_volume";
inline constexpr const char* kPresentationDisasterBanner = "presentation.disaster_banner";
} // namespace Cap

// Allowlist owner. The mode-pack registry asks this registry whether a
// manifest's declared capability is known; unknown capabilities fail the load.
class CapabilityRegistry
{
public:
    static CapabilityRegistry& instance();

    bool isKnown(const std::string& id) const;
    const std::vector<std::string>& knownIds() const { return mKnown; }

    // True when the first token of a capability id is registered. Used for
    // human-readable diagnostics ("unknown capability family").
    std::string familyOf(const std::string& id) const;

private:
    CapabilityRegistry();
    std::vector<std::string> mKnown;
};

} // namespace MimitaGamemode
