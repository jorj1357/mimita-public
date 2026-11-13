// 2026-11-13
// Resident file-handle capability for long-lived game streams.
//
// Per-call reads/writes use ordinary RAII helpers. A handle that can outlive a
// single call (for example a replay raw-frame stream) must be host-owned.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kFilesCapabilityVersion = 1u;

struct FilesCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::uint64_t (*open_write)(void* hostUserData, const char* path);
    std::int32_t (*write)(void* hostUserData, std::uint64_t handle,
                          const void* data, std::uint32_t size);
    void (*close)(void* hostUserData, std::uint64_t handle);
};

}  // namespace mimita::host
