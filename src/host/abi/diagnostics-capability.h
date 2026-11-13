// 2026-11-13
// Resident process diagnostics capability.
//
// Crash/stack inspection belongs to the resident process. A game generation
// asks for a bounded stack string; it never installs a second crash handler.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kDiagnosticsCapabilityVersion = 1u;

struct DiagnosticsCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    std::int32_t (*capture_thread_stack)(void* hostUserData, std::uint32_t threadId,
                                         char* out, std::uint32_t outSize);
};

}  // namespace mimita::host
