// 09 23 2026
/* purpose
* Define the generic, hot-replaceable client snapshot-apply policy and the ONE
* implementation shared by the cold EXE fallback and the hot provider. The EXE
* owns the replica storage, interpolation buffers, and world; a hot module owns
* whether a local/remote snapshot sample may mutate lifecycle.
* POD only: no STL or engine objects cross the boundary.
* Does NOT own replicas, interpolation, or transport.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

enum GameSnapshotDropV1 : std::uint32_t {
    GAME_SNAPSHOT_APPLY = 0,
    GAME_SNAPSHOT_DROP_STALE_LOCAL = 1,
    GAME_SNAPSHOT_DROP_STALE_MEMBERSHIP = 2,
};

struct GameSnapshotApplyV1 {
    std::uint32_t structSize;
    // inputs
    std::uint32_t isLocal;
    std::uint32_t existsBefore;          // remote replica already present
    std::uint32_t membershipAllowed;     // snapshot may mutate world membership
    std::uint32_t incomingEpoch;
    std::uint32_t localServerEpoch;
    std::uint32_t serverTick;
    std::uint32_t latestLocalSnapshotTick;
    // out
    std::uint32_t acceptLifecycle;       // local sample may update authoritative state
    std::uint32_t mayCreate;             // may create a new remote entity
    std::uint32_t dropReason;            // GameSnapshotDropV1
    std::uint32_t result;
};

using GameSnapshotApplyFn = void (MIMITA_GAME_CALL *)(
    void* host, GameSnapshotApplyV1* request);

static constexpr std::uint64_t GAME_CAP_CLIENT_SNAPSHOT =
    gameHash("net.client-snapshot");
static constexpr std::uint64_t GAME_SIG_CLIENT_SNAPSHOT =
    gameHash("sig.net.client-snapshot.v1");

namespace HotClientSnapshotImpl {

inline void evaluate(GameSnapshotApplyV1& r)
{
    r.acceptLifecycle = 1u;
    r.mayCreate = r.existsBefore || r.membershipAllowed ? 1u : 0u;
    r.dropReason = GAME_SNAPSHOT_APPLY;
    r.result = 1u;

    if (!r.existsBefore && !r.membershipAllowed) {
        r.mayCreate = 0u;
        r.dropReason = GAME_SNAPSHOT_DROP_STALE_MEMBERSHIP;
    }

    if (!r.isLocal)
        return;

    const bool olderEpoch = r.incomingEpoch != 0 && r.localServerEpoch != 0 &&
        r.incomingEpoch < r.localServerEpoch;
    const bool sameEpochOlderTick = r.incomingEpoch == r.localServerEpoch &&
        r.serverTick <= r.latestLocalSnapshotTick;
    if (olderEpoch || sameEpochOlderTick) {
        r.acceptLifecycle = 0u;
        r.dropReason = GAME_SNAPSHOT_DROP_STALE_LOCAL;
    }
}

} // namespace HotClientSnapshotImpl

} // namespace MimitaNet
