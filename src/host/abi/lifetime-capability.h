// 2026-11-12
// Versioned generation-lifetime capabilities.
//
// These are leases, not raw threads or callbacks. The host owns the actual
// executor. A generation must acquire/release leases so the barrier can refuse
// unload while any work can still re-enter it.

#pragma once

#include <cstddef>
#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kJobsCapabilityVersion = 1u;
inline constexpr std::uint32_t kCallbacksCapabilityVersion = 1u;
inline constexpr std::uint32_t kResourcesCapabilityVersion = 1u;

struct JobsCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::uint64_t (*acquire)(void* hostUserData, const char* tag);
    HostResult (*release)(void* hostUserData, std::uint64_t lease);
    std::uint32_t (*busy)(void* hostUserData);
    void* (*json_watch_create)(void* hostUserData, const char* const* roots,
                               std::uint32_t rootCount, std::uint32_t intervalMs);
    void (*json_watch_destroy)(void* hostUserData, void* watch);
    std::int32_t (*json_watch_consume_dirty)(void* hostUserData, void* watch);
    void (*json_watch_mark_dirty)(void* hostUserData, void* watch);
};

struct CallbacksCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::uint64_t (*register_callback)(void* hostUserData, std::uint32_t kind,
                                       const char* tag);
    HostResult (*unregister_callback)(void* hostUserData, std::uint64_t token);
    std::uint32_t (*busy)(void* hostUserData);
};

struct ResourcesCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::uint64_t (*allocate)(void* hostUserData, std::uint32_t kind,
                              std::size_t bytes, std::size_t alignment,
                              const char* tag);
    void (*release)(void* hostUserData, std::uint64_t resource);
    std::uint32_t (*busy)(void* hostUserData);
};

}  // namespace mimita::host
