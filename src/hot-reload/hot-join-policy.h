// 09 23 2026
/* purpose
* Define the generic, hot-replaceable server join-acceptance policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the packet, the coordinator call, the player table, and the reject packet;
* a hot module owns the local/coordinator/password acceptance rules.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own token validation, transport, or player state.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Reject reasons (numeric values are the wire contract; mirror JoinRejectPacket).
enum GameJoinRejectV1 : std::uint32_t {
    GAME_JOIN_REJECT_NONE = 0,
    GAME_JOIN_REJECT_FULL = 1,
    GAME_JOIN_REJECT_BAD_TOKEN = 2,
    GAME_JOIN_REJECT_WRONG_PASSWORD = 5,
};

struct GameJoinPolicyV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t playersFull;
    std::uint32_t tokenEmpty;
    std::uint32_t coordinatorIsLocal;
    std::uint32_t hasCoordinatorCode;
    std::uint32_t coordinatorValidated;   // coordinatorIceValidateJoin result
    std::uint32_t passwordProtected;
    std::uint32_t passwordMatches;
    // out
    std::uint32_t accept;
    std::uint32_t rejectReason;           // GameJoinRejectV1
    std::uint32_t result;
};

using GameJoinPolicyFn = void (MIMITA_GAME_CALL *)(void* host,
                                                   GameJoinPolicyV1* request);

static constexpr std::uint64_t GAME_CAP_JOIN_POLICY = gameHash("net.join-policy");
static constexpr std::uint64_t GAME_SIG_JOIN_POLICY =
    gameHash("sig.net.join-policy.v1");

namespace HotJoinPolicyImpl {

inline void evaluate(GameJoinPolicyV1& r)
{
    r.accept = 0u;
    r.rejectReason = GAME_JOIN_REJECT_NONE;
    r.result = 1u;

    if (r.playersFull) {
        r.rejectReason = GAME_JOIN_REJECT_FULL;
        return;
    }
    if (r.tokenEmpty) {
        r.rejectReason = GAME_JOIN_REJECT_BAD_TOKEN;
        return;
    }
    // A local server skips coordinator validation; otherwise the coordinator
    // must have validated the token and a coordinator code must be configured.
    if (!r.coordinatorIsLocal) {
        if (!r.hasCoordinatorCode || !r.coordinatorValidated) {
            r.rejectReason = GAME_JOIN_REJECT_BAD_TOKEN;
            return;
        }
    }
    if (r.passwordProtected && !r.passwordMatches) {
        r.rejectReason = GAME_JOIN_REJECT_WRONG_PASSWORD;
        return;
    }
    r.accept = 1u;
}

} // namespace HotJoinPolicyImpl

} // namespace MimitaNet
