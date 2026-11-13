// 2026-11-13
// Versioned persistence queue capability.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kPersistenceCapabilityVersion = 1u;

enum class PersistenceOp : std::uint32_t {
    ObservePlayer = 1,
    Tick = 2,
    EnqueueKill = 3,
    EnqueueMatchResult = 4,
    TakeNotices = 5,
    SessionId = 6,
    QueueDepth = 7,
    TotalSent = 8,
    TotalRetries = 9,
    TotalDuplicates = 10,
};

struct PersistenceObserveArgs {
    std::uint32_t playerId;
    std::uint32_t reserved;
    std::uint64_t userId;
    std::int32_t active;
    char name[128];
    char ticket[256];
};

struct PersistenceCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::int32_t (*invoke)(void* hostUserData, PersistenceOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize);
};

}  // namespace mimita::host
