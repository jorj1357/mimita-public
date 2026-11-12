// 2026-11-12
// Purpose: bounded, plain-data NPC target-selection probe. This is the temporary
// validation slice that proves the whole-game loader/barrier can execute a real
// NPC decision in a live server process.
//
// In: policy + my team + up to 64 candidate actors (relative position).
// Out: one chosen target id. No STL, pointers, or game objects cross the ABI.
//
// The probe is parity-only: the host logs the inline result and the DLL result
// and keeps the inline result. It is reverted before the all-files cutover.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kNpcTargetProbeVersion = 1u;
inline constexpr std::uint32_t kNpcTargetMaxCandidates = 64u;
inline constexpr std::uint32_t kEventNpcTargetSelect = 720u;

struct NpcTargetCandidateV1 {
    std::uint32_t id;
    std::int32_t team;
    std::uint8_t isNpc;
    std::uint8_t dead;
    std::uint8_t reserved[2];
    float relativePosition[3];
};

struct NpcTargetDecisionV1 {
    std::uint32_t structSize;
    std::uint32_t version;
    std::uint32_t actorId;
    std::int32_t myTeam;
    std::int32_t mode;  // NpcTargetingMode
    std::uint8_t includePlayers;
    std::uint8_t includeNpcs;
    std::uint8_t reserved[2];
    std::uint32_t tick;
    std::uint32_t currentTargetId;
    std::uint32_t candidateCount;

    // Output written by the game generation.
    std::uint32_t outTargetId;
    std::int32_t outHandled;
    std::uint32_t reserved2;

    NpcTargetCandidateV1 candidates[kNpcTargetMaxCandidates];
};

}  // namespace mimita::host
