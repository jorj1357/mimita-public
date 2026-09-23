// 09 23 2026
/* purpose
* Define the generic, hot-replaceable server reload-request decision policy and
// the ONE implementation shared by the cold EXE fallback and the hot provider.
* The EXE owns the weapon state, ammo adoption, timer arming, and the result
* packet; a hot module owns the accept/reject reason ordering.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own weapon state or transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Reload reasons (mirror the wire reason codes in ReloadResultPacket).
enum GameReloadReasonV1 : std::uint32_t {
    GAME_RELOAD_ACCEPT = 0,
    GAME_RELOAD_REASON_DEAD = 2,
    GAME_RELOAD_REASON_MAG_FULL = 3,
    GAME_RELOAD_REASON_NO_RESERVE = 4,
    GAME_RELOAD_REASON_ALREADY = 5,
};

struct GameReloadDecisionV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t dead;
    std::int32_t currentAmmo;
    std::int32_t magazineSize;
    std::int32_t reserveAmmo;
    std::uint32_t alreadyReloading;
    // out
    std::uint32_t accept;         // 1 = accept the reload
    std::uint32_t beginReload;    // 1 = start a fresh reload (not already)
    std::uint32_t reason;         // GameReloadReasonV1
    std::uint32_t result;
};

using GameReloadDecisionFn = void (MIMITA_GAME_CALL *)(void* host,
                                                       GameReloadDecisionV1* request);

static constexpr std::uint64_t GAME_CAP_RELOAD_DECISION =
    gameHash("net.reload-decision");
static constexpr std::uint64_t GAME_SIG_RELOAD_DECISION =
    gameHash("sig.net.reload-decision.v1");

namespace HotReloadDecisionImpl {

inline void evaluate(GameReloadDecisionV1& r)
{
    r.accept = 0u;
    r.beginReload = 0u;
    r.reason = GAME_RELOAD_REASON_MAG_FULL;
    r.result = 1u;

    if (r.dead) {
        r.reason = GAME_RELOAD_REASON_DEAD;
        return;
    }
    if (r.currentAmmo >= r.magazineSize) {
        r.reason = GAME_RELOAD_REASON_MAG_FULL;
        return;
    }
    if (r.reserveAmmo <= 0) {
        r.reason = GAME_RELOAD_REASON_NO_RESERVE;
        return;
    }
    r.accept = 1u;
    if (r.alreadyReloading) {
        // Already reloading: accept but report current state.
        r.reason = GAME_RELOAD_REASON_ALREADY;
        return;
    }
    r.beginReload = 1u;
    r.reason = GAME_RELOAD_ACCEPT;
}

} // namespace HotReloadDecisionImpl

} // namespace MimitaNet
