// 2026-11-13
// Versioned resident session-identity capability.
//
// The host owns the live map id, coordinator code/token, and private-server
// password state. The game generation reads/writes bounded plain data; the
// password itself never crosses the ABI (only a match result does).

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kSessionIdentityCapabilityVersion = 1u;

enum class SessionTextField : std::uint32_t {
    MapId = 0,
    CoordinatorCode = 1,
    CoordinatorJoinToken = 2,
    MatchMode = 3,
    CommunityMode = 4,
};

enum class SessionFlagField : std::uint32_t {
    PasswordProtected = 0,
};

struct SessionIdentityCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Returns the full text length (excluding NUL) and NUL-terminates `out`
    // when capacity > 0.
    std::uint32_t (*get_text)(void* hostUserData, std::uint32_t field,
                              char* out, std::uint32_t capacity);
    void (*set_text)(void* hostUserData, std::uint32_t field,
                     const char* value);
    std::uint32_t (*get_flag)(void* hostUserData, std::uint32_t field);
    void (*set_flag)(void* hostUserData, std::uint32_t field,
                     std::uint32_t value);
    void (*set_password)(void* hostUserData, std::uint32_t protectedFlag,
                         const char* password);
    std::uint32_t (*password_matches)(void* hostUserData,
                                      const char* supplied);
};

}  // namespace mimita::host
