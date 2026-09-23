// 09 23 2026
/* purpose
* Generic historical-state query boundary for lag compensation. The EXE owns the
* bounded history store (player + NPC samples, timestamps, generation tags); a
* hot capability answers a query for (entity, tick) with the rewind pose and,
* critically, owns the *selection policy* (interpolate vs nearest vs
* cross-generation clamp). This lets lag-compensation ordering be edited live
* without rebuilding the EXE.
* Does NOT own history storage, sample insertion, collision, or the socket.
*/
#pragma once

#include <cstdint>

#include "hot-reload/game-api.h"

namespace MimitaNet {

// Which history the query targets. The kernel resolves the entity id in that
// store; hot code never sees the ServerPlayer/ServerNpc layout.
enum class HistoryDomainV1 : std::uint32_t {
    Player = 0,
    Npc = 1,
};

// How the query selected the result. Reported so the hot policy decision and the
// cold execution are both visible in the journal/decision record.
enum class HistorySelectionV1 : std::uint32_t {
    None = 0,           // no history / entity missing
    ExactTick = 1,      // a sample existed at exactly this tick
    Interpolated = 2,   // blended between two samples in one generation
    NearestNewer = 3,   // clamped to the newer sample (target older than front)
    NearestOlder = 4,   // clamped to the older sample (target newer than back)
    GenerationClamp = 5,// two samples straddled an F/G boundary; clamped to newer
};

struct GameHistoryQueryV1 {
    // in
    std::uint64_t entityId;
    std::uint32_t domain;             // HistoryDomainV1
    std::uint32_t tick;               // target server tick to reconstruct
    std::uint32_t currentTick;        // server's current tick (for bounds)
    // out
    std::uint32_t handled;            // 1 when a history store answered
    std::uint32_t selection;          // HistorySelectionV1
    std::uint32_t sampleTickA;        // bracketing older sample tick (0 = none)
    std::uint32_t sampleTickB;        // bracketing newer sample tick (0 = none)
    float fraction;                   // interpolation fraction used (0..1)
    float position[3];
    float velocity[3];
    float yaw;
    std::uint64_t generationA;        // logical generation of sample A
    std::uint64_t generationB;        // logical generation of sample B
    std::uint32_t found;              // 1 when a pose was produced
    std::uint32_t reserved;
};

// The generic lookup callable for one player. Returns the raw sample pair plus
// validity so the hot handler can apply its own selection policy. `out` receives
// up to two samples (older, newer); the kernel owns the source data.
static constexpr std::uint32_t HISTORY_MAX_SAMPLES = 2;

struct GameHistorySampleV1 {
    std::uint32_t tick;
    std::uint32_t logicalGenerationId;
    float position[3];
    float velocity[3];
    float yaw;
};

// Capability: answer a history query. The kernel fills raw samples; the hot
// provider (or kernel default) decides selection. Registered by the EXE as
// `history.query`; hot code resolves it through the generic doorway.
using GameHistoryQueryFn = std::uint32_t (MIMITA_GAME_CALL *)(
    void* host, GameHistoryQueryV1* query);

static constexpr std::uint64_t GAME_CAP_HISTORY_QUERY = gameHash("history.query");
// Policy capability: hot code owns selection given the bracketing samples.
// The kernel mechanism answers with raw samples; the hot policy decides
// interpolate/nearest/clamp. Event id for the generic seam.
static constexpr std::uint64_t GAME_EVENT_HISTORY_SELECT = gameHash("history.select");

struct GameHistorySelectV1 {
    // in: raw bracketing samples from the EXE-owned store
    std::uint32_t targetTick;
    std::uint32_t currentTick;
    std::uint32_t exactTick;          // 1 when a sample exists at targetTick
    std::uint32_t haveA;              // older sample present
    std::uint32_t haveB;              // newer sample present
    GameHistorySampleV1 a;
    GameHistorySampleV1 b;
    // out: the selection decision
    std::uint32_t handled;
    std::uint32_t selection;          // HistorySelectionV1
    float fraction;
    float position[3];
    float velocity[3];
    float yaw;
};

} // namespace MimitaNet
