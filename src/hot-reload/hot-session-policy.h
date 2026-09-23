// 09 23 2026
/* purpose
* Define generic, hot-replaceable server session-handshake policies: reconnect
* token grace/rotation and the map-ready spawn/rearm decision. The EXE owns the
* player table, token generation, transforms, and packets; a hot module owns the
* grace window and the spawn/rearm choice.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own token storage, transport, or transforms.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// reconnect.grace: whether an old (previous) reconnect token is still honored.
struct GameReconnectGraceV1 {
    std::uint32_t structSize;
    std::uint32_t resendExistingAccept;   // matched the previous token, not current
    float previousValidUntilMs;           // 0 = none
    float nowMs;
    std::uint32_t graceMs;                // rotation grace window
    // out
    std::uint32_t honorPrevious;          // 1 = accept the previous token
    float outPreviousValidUntilMs;        // expiry to store after a fresh accept
    std::uint32_t result;
};

using GameReconnectGraceFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GameReconnectGraceV1* request);

// map.ready: initial spawn vs re-arm the transform epoch.
struct GameMapReadyV1 {
    std::uint32_t structSize;
    std::uint32_t alreadySpawned;
    std::uint32_t mapMatches;
    // out
    std::uint32_t spawn;      // 1 = initial authoritative spawn
    std::uint32_t rearm;      // 1 = re-arm the transform-epoch gate
    std::uint32_t result;
};

using GameMapReadyFn = void (MIMITA_GAME_CALL *)(void* host, GameMapReadyV1* request);

struct GameSessionPolicyV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GameReconnectGraceFn reconnectGrace;
    GameMapReadyFn mapReady;
    const char* name;
};

using GameSessionLookupFn = const GameSessionPolicyV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_CAP_SESSION_POLICY = gameHash("net.session-policy");
static constexpr std::uint64_t GAME_SIG_SESSION_POLICY =
    gameHash("sig.net.session-policy.v1");

namespace HotSessionPolicyImpl {

inline void reconnectGrace(GameReconnectGraceV1& r)
{
    r.outPreviousValidUntilMs = r.nowMs + r.graceMs;
    r.honorPrevious = (r.resendExistingAccept && r.previousValidUntilMs > 0.0f &&
                       r.nowMs <= r.previousValidUntilMs)
        ? 1u : 0u;
    r.result = 1u;
}

inline void mapReady(GameMapReadyV1& r)
{
    r.spawn = (!r.alreadySpawned && r.mapMatches) ? 1u : 0u;
    r.rearm = (r.alreadySpawned && r.mapMatches) ? 1u : 0u;
    r.result = 1u;
}

} // namespace HotSessionPolicyImpl

} // namespace MimitaNet
