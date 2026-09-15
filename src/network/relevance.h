// 09 15 2026
/* purpose
* Generic snapshot/relevance policy facts. The kernel gathers candidate entities
* (from generic Transform state) and dispatches net.relevance per viewer; a hot
* policy decides which candidates replicate, their priority tier, and the
* low-tier cadence. Transport/framing stays cold. No player/NPC/entity-type
* specific replication policy.
* POD only: no STL, no pointers.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

static constexpr std::uint32_t GAME_MAX_RELEVANCE_CANDIDATES = 64;

struct GameRelevanceCandidateV1 {
    std::uint64_t entity;
    float position[3];
    std::uint32_t flags;      // bit0 = always-relevant (generic ReplicationPolicy)
    std::uint32_t reserved;
};

struct GameRelevanceQueryV1 {
    std::uint64_t viewerEntity;
    float viewerPosition[3];
    std::uint32_t tick;
    std::uint32_t candidateCount;
    GameRelevanceCandidateV1 candidates[GAME_MAX_RELEVANCE_CANDIDATES];
    // out: policy decision per candidate (parallel arrays, same order as input).
    std::uint32_t outInclude[GAME_MAX_RELEVANCE_CANDIDATES];  // 0 skip, 1 include
    std::uint32_t outTier[GAME_MAX_RELEVANCE_CANDIDATES];     // 0 every tick, 1 low
    std::uint32_t outLowTierEveryNTicks;                       // cadence for tier 1
    std::uint32_t handled;                                     // set 1 by the policy
};

// Generic runtime event id for the relevance query.
static constexpr std::uint64_t GAME_EVENT_NET_RELEVANCE = gameHash("net.relevance");

// Generic per-entity replication metadata (package/hot-writable): bit0 =
// always-relevant regardless of distance (match/objective state).
static constexpr std::uint64_t GAME_COMPONENT_REPLICATION_POLICY =
    gameHash("ReplicationPolicy");

} // namespace MimitaNet
