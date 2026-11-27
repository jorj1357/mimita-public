// 2026-11-27
/* purpose
* Decide whether an authored geometry signature change is block-only and can be
* applied to the live authoritative world without a full rebuild, and coalesce
* rapid editor drags into a bounded number of authoritative applies.
* This file DOES NOT own map data, world state, or the rebuild itself.
*/
#include "map/authored-block-rebuild.h"

namespace {

// Coalesce drag/edit bursts. The authoritative world may trail the editor by at
// most this interval; it is three to six fixed ticks, not tens of seconds.
constexpr uint64_t kMinBlockApplyIntervalMs = 100;

struct BlockAuthorityState
{
    uint64_t appliedNonBlockSignature = 0;
    uint64_t lastApplyMs = 0;
};

BlockAuthorityState& state()
{
    static BlockAuthorityState value;
    return value;
}

}  // namespace

void authoredBlockAuthorityReset()
{
    state() = BlockAuthorityState{};
}

void authoredBlockAuthorityMarkApplied(uint64_t nonBlockSignature,
                                       uint64_t nowMs)
{
    BlockAuthorityState& s = state();
    s.appliedNonBlockSignature = nonBlockSignature;
    s.lastApplyMs = nowMs;
}

bool authoredBlockAuthorityCanApply(uint64_t nonBlockSignature)
{
    const BlockAuthorityState& s = state();
    return s.appliedNonBlockSignature != 0 &&
           s.appliedNonBlockSignature == nonBlockSignature;
}

bool authoredBlockAuthorityThrottleAllows(uint64_t nowMs)
{
    const BlockAuthorityState& s = state();
    return s.lastApplyMs == 0 ||
           nowMs >= s.lastApplyMs + kMinBlockApplyIntervalMs;
}
// map-authored-parity: saved for batch build 2026-11-27T01:45Z
