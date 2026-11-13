// 2026-11-13
// Resident UDP socket ownership and bounded send/receive operations.
//
// The game holds only an opaque handle. It never sees or uses a SOCKET,
// sockaddr lifetime, or socket option ownership.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kSocketCapabilityVersion = 2u;

struct SocketCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    std::uint64_t (*open_udp)(void* hostUserData);
    void (*close)(void* hostUserData, std::uint64_t handle);
    std::int32_t (*bind_address)(void* hostUserData, std::uint64_t handle,
                                 const void* address, std::uint32_t addressSize);
    std::int32_t (*get_local_address)(void* hostUserData, std::uint64_t handle,
                                      void* address, std::uint32_t addressSize);
    std::int32_t (*set_nonblocking)(void* hostUserData, std::uint64_t handle);
    std::int32_t (*set_reuse_address)(void* hostUserData, std::uint64_t handle,
                                      std::int32_t enabled);
    std::int32_t (*set_exclusive_address)(void* hostUserData, std::uint64_t handle,
                                          std::int32_t enabled);
    std::int32_t (*set_recv_timeout)(void* hostUserData, std::uint64_t handle,
                                     std::uint32_t milliseconds);
    std::int32_t (*send_to)(void* hostUserData, std::uint64_t handle,
                            const void* data, std::uint32_t size,
                            const void* address, std::uint32_t addressSize);
    std::int32_t (*recv_from)(void* hostUserData, std::uint64_t handle,
                              void* data, std::uint32_t capacity,
                              void* address, std::uint32_t addressSize,
                              std::uint32_t* received, std::int32_t* wouldBlock);
};

}  // namespace mimita::host
