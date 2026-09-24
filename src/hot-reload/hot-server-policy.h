// 09 23 2026
/* purpose
* Define the generic, hot-replaceable dedicated-server startup policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the launch options, the world, the player/NPC stores, and the socket loop;
* a hot module owns the mode selection (duel vs community), the startup NPC
* count, and the startup NPC fallback placement.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own the loop, transport, or match state.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Mode selection: 0 = duel, 1 = community.
struct GameServerModeV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t duelRequested;
    std::uint32_t gameModeIsSandbox;
    // out
    std::uint32_t useDuel;         // 1 = duel branch, 0 = community branch
    std::uint32_t startMatch;      // community: start the match immediately
    std::uint32_t result;
};

// Startup NPC spawn plan (count + fallback placement when no spawn points).
struct GameServerStartupNpcV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t npcsEnabled;
    std::uint32_t requestedCount;
    std::uint32_t spawnPointCount;
    std::uint32_t maxSpawn;
    // out
    std::uint32_t count;           // clamped NPC count to spawn
    std::uint32_t useSpawnPoints;  // 1 = place on world spawn points, 0 = fallback
    std::uint32_t result;
};

using GameServerModeFn = void (MIMITA_GAME_CALL *)(void* host, GameServerModeV1* request);
using GameServerStartupNpcFn = void (MIMITA_GAME_CALL *)(void* host,
                                                         GameServerStartupNpcV1* request);

struct GameServerPolicyV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GameServerModeFn mode;
    GameServerStartupNpcFn startupNpc;
    const char* name;
};

using GameServerPolicyLookupFn =
    const GameServerPolicyV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_CAP_SERVER_POLICY = gameHash("net.server-policy");
static constexpr std::uint64_t GAME_SIG_SERVER_POLICY =
    gameHash("sig.net.server-policy.v1");

namespace HotServerPolicyImpl {

inline void mode(GameServerModeV1& r)
{
    r.useDuel = r.duelRequested ? 1u : 0u;
    // Community: start the match unless the mode is sandbox.
    r.startMatch = (!r.duelRequested && !r.gameModeIsSandbox) ? 1u : 0u;
    r.result = 1u;
}

inline void startupNpc(GameServerStartupNpcV1& r)
{
    const std::uint32_t wanted = r.npcsEnabled ? r.requestedCount : 0u;
    r.count = wanted < r.maxSpawn ? wanted : r.maxSpawn;
    r.useSpawnPoints = r.spawnPointCount > 0 ? 1u : 0u;
    r.result = 1u;
}

} // namespace HotServerPolicyImpl

} // namespace MimitaNet
