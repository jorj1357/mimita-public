// 09 23 2026
/* purpose
* Define the generic, hot-replaceable client connection-health policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the transport, session state, and side effects (teardown, reconnect, UI);
* a hot module owns the next-state decision and the reconnect retry cadence.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own transport, teardown, or notifications.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// ConnectionState numeric values are the cross-boundary contract. They mirror
// network/connection-state.h; the bridge maps them both ways.
struct GameConnectionHealthV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t current;             // ConnectionState numeric
    std::uint64_t now;
    std::uint64_t lastHeardAge;
    std::uint32_t heardSinceDisconnect;
    std::uint64_t staleThresholdMs;
    std::uint64_t hardTimeoutMs;
    std::uint64_t graceDeadline;
    std::uint64_t graceMs;
    // out
    std::uint32_t next;                // ConnectionState numeric
    std::uint32_t result;
};

// Reconnect retry cadence: whether attempt N may be sent now.
struct GameReconnectCadenceV1 {
    std::uint32_t structSize;
    // inputs
    std::int32_t attempts;
    std::int32_t maxAttempts;
    std::uint64_t backoffMs;
    std::uint64_t intervalMs;
    std::uint64_t now;
    std::uint64_t lastAttemptMs;
    // out
    std::uint32_t maySend;             // 1 = send another attempt this tick
    std::uint64_t outBackoffMs;        // backoff to store when zero
    std::uint32_t cappedOut;           // 1 = attempt cap reached
    std::uint32_t result;
};

using GameConnectionHealthFn = void (MIMITA_GAME_CALL *)(
    void* host, GameConnectionHealthV1* request);
using GameReconnectCadenceFn = void (MIMITA_GAME_CALL *)(
    void* host, GameReconnectCadenceV1* request);

struct GameConnectionPolicyV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    GameConnectionHealthFn nextState;
    GameReconnectCadenceFn cadence;
    const char* name;
};

using GameConnectionLookupFn =
    const GameConnectionPolicyV1* (MIMITA_GAME_CALL *)(void* host);

static constexpr std::uint64_t GAME_CAP_CONNECTION_POLICY =
    gameHash("net.connection-policy");
static constexpr std::uint64_t GAME_SIG_CONNECTION_POLICY =
    gameHash("sig.net.connection-policy.v1");

namespace HotConnectionHealthImpl {

// ConnectionState numeric values, mirroring network/connection-state.h. Kept as
// plain integers here so the policy has no engine dependency; the bridge passes
// the real enum values and they must match these.
enum : std::uint32_t {
    kConnected = 6,
    kReconnecting = 7,
    kWeakConnection = 10,
    kReconnectFailed = 11,
};

inline void nextState(GameConnectionHealthV1& r)
{
    r.result = 1u;
    switch (r.current) {
    case kConnected:
    case kWeakConnection:
        if (r.lastHeardAge > r.hardTimeoutMs)
            r.next = kReconnecting;
        else if (r.lastHeardAge > r.staleThresholdMs)
            r.next = kWeakConnection;
        else
            r.next = kConnected;
        return;
    case kReconnecting:
        if (r.heardSinceDisconnect)
            r.next = kConnected;
        else if (r.graceDeadline != 0 && r.now >= r.graceDeadline)
            r.next = kReconnectFailed;
        else
            r.next = kReconnecting;
        return;
    default:
        r.next = r.current;
        return;
    }
}

inline void cadence(GameReconnectCadenceV1& r)
{
    r.result = 1u;
    r.cappedOut = (r.attempts >= r.maxAttempts) ? 1u : 0u;
    r.outBackoffMs = r.backoffMs == 0 ? r.intervalMs : r.backoffMs;
    r.maySend = (!r.cappedOut &&
                 (r.now - r.lastAttemptMs) >= r.outBackoffMs) ? 1u : 0u;
}

} // namespace HotConnectionHealthImpl

} // namespace MimitaNet
