// 09 29 2026
/* purpose
* Own the pure decision for whether the local player may run gameplay
* simulation, plus the composite local-lifecycle identity, without any engine,
* transport, or rendering dependency so both live gameplay and a standalone
* test use the exact same rule.
* Does NOT read MultiplayerContext or Player (the network adapter fills the
* plain view below), send packets, run physics, or own spawn authority.
*/
#pragma once

#include <cstdint>

namespace MimitaNet {

// Plain snapshot of the lifecycle facts the readiness decision depends on.
// The multiplayer adapter fills it from MultiplayerContext/Player.
struct LocalGameplayReadiness
{
    // False for single-player/replay; those paths are never gated.
    bool networked = true;
    bool connected = false;
    bool mapReadyForPlayer = false;
    bool waitingForMapLoad = false;
    bool hasServerPosition = false;
    // True while an authoritative spawn packet is queued but not applied.
    bool spawnTransformPending = true;
    bool hasSpawnGeneration = false;
    bool hasServerEpoch = false;
    // Outgoing epoch and applied epoch both match the server's.
    bool outgoingEpochMatches = false;
    bool appliedEpochMatches = false;
    // Player model/body parts exist so the aim body can bind.
    bool modelReady = false;
};

// True when the local player may run gameplay simulation (movement, gravity,
// collision, aimbody/ragdoll). Requires the authoritative spawn transform to be
// installed but deliberately does NOT wait for the SpawnActivated round trip,
// so instant respawn stays responsive.
inline bool localGameplaySimulationReady(const LocalGameplayReadiness& r)
{
    if (!r.networked)
        return true;
    return r.connected
        && !r.waitingForMapLoad
        && r.mapReadyForPlayer
        && r.hasServerPosition
        && !r.spawnTransformPending
        && r.hasSpawnGeneration
        && r.hasServerEpoch
        && r.outgoingEpochMatches
        && r.appliedEpochMatches
        && r.modelReady;
}

// Composite identity of the local authoritative life. Advances on initial
// spawn, respawn, instant respawn, teleport, duel spawn, map change, reconnect,
// and any true transform discontinuity. Consumers (simulation, aimbody rebind)
// watch this instead of duplicating lifecycle detection.
inline uint64_t localLifecycleId(uint32_t serverEpoch, uint32_t spawnGeneration)
{
    return (static_cast<uint64_t>(serverEpoch) << 32)
         | static_cast<uint64_t>(spawnGeneration);
}

} // namespace MimitaNet
