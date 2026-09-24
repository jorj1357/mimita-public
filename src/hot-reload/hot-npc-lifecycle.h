// 09 24 2026
/* purpose
* Define the generic, hot-replaceable NPC lifecycle policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns entity storage, sockets, the fixed-tick loop, and applying the result; a
* hot module owns whether/how many startup NPCs exist, spawn placement, initial
* health, starting weapon/loadout, difficulty, respawn, and reconciliation of
* stale automatic NPCs.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own the loop, transport, entity storage, or damage.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// The single fallback implementation used when no hot provider is registered.
// It preserves the exact pre-migration behavior: enable/disable startup NPCs
// from the cold config, clamp the count, place on spawn points when available.
namespace HotNpcLifecycleImpl {

inline void evaluate(NpcLifecyclePolicyV1& r)
{
    switch (r.reason) {
    case GAME_NPC_LIFECYCLE_RESPAWN:
        // Revive immediately when respawns are enabled; the shared respawn
        // rule (net.respawn) owns the delay elsewhere.
        r.outSpawnId = r.respawnPlayerId;
        r.outRespawnNow = r.respawnEnabled ? 1u : 0u;
        break;
    case GAME_NPC_LIFECYCLE_RECONCILE:
    case GAME_NPC_LIFECYCLE_GENERATION: {
        // Keep the automatic set aligned with the configured count.
        const std::uint32_t desired = r.configStartupEnabled
            ? (r.requestedCount < r.maxSpawn ? r.requestedCount : r.maxSpawn)
            : 0u;
        r.desiredAutomatic = desired;
        r.spawnCount = desired > r.existingAutomaticCount
                           ? desired - r.existingAutomaticCount : 0u;
        r.destroyAutomatic =
            r.existingAutomaticCount > desired ? 1u : 0u;
        r.useSpawnPoints = r.spawnPointCount > 0 ? 1u : 0u;
        break;
    }
    default:
        r.spawnCount = r.configStartupEnabled
                           ? (r.requestedCount < r.maxSpawn ? r.requestedCount
                                                            : r.maxSpawn)
                           : 0u;
        r.desiredAutomatic = r.spawnCount;
        r.useSpawnPoints = r.spawnPointCount > 0 ? 1u : 0u;
        break;
    }
    r.allowManualSpawn = 1u;
    r.handled = 1u;
    r.result = 1u;
}

} // namespace HotNpcLifecycleImpl

} // namespace MimitaNet
