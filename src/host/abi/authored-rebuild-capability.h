// 2026-11-13
// Resident authored-world rebuild worker capability.
//
// The worker and thread stay resident. The game passes a request by value on
// submit and receives/moves result shared pointers; the operation holds a host
// job lease so the barrier cannot unload a generation with rebuild work alive.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kAuthoredRebuildCapabilityVersion = 1u;

struct AuthoredRebuildCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    void* (*worker)(void* hostUserData);
    void (*submit)(void* hostUserData, void* worker, const void* request);
    void (*try_take_result)(void* hostUserData, void* worker, void* outSharedPtr);
    void (*retire)(void* hostUserData, void* worker, void* inoutSharedPtr);
    void (*cancel_and_wait)(void* hostUserData, void* worker);
};

}  // namespace mimita::host
