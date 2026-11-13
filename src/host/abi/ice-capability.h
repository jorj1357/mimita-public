// 2026-11-13
// Resident ICE session capability.
//
// The resident host owns juice agents, sockets, and callback threads. A game
// generation holds only an opaque handle and exchanges bounded SDP/event data.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kIceCapabilityVersion = 1u;
inline constexpr std::uint32_t kIceMaxEventData = 64u * 1024u;

enum class IceEventKindV1 : std::int32_t {
    StateChanged = 0,
    Candidate = 1,
    GatheringDone = 2,
    Recv = 3,
};

struct IceEventV1 {
    std::int32_t kind;
    std::int32_t newState;
    std::uint32_t dataSize;
    const std::uint8_t* data;
};

struct IceCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    void* (*create)(void* hostUserData);
    void (*destroy)(void* hostUserData, void* handle);
    std::int32_t (*initialize)(void* hostUserData, void* handle,
                               const void* configuration);
    void (*shutdown)(void* hostUserData, void* handle);
    std::int32_t (*gather_candidates)(void* hostUserData, void* handle);
    std::int32_t (*local_sdp)(void* hostUserData, void* handle, char* out,
                              std::uint32_t outSize);
    std::int32_t (*set_remote_description)(void* hostUserData, void* handle,
                                           const char* sdp);
    std::int32_t (*send)(void* hostUserData, void* handle, const void* data,
                         std::uint32_t size);
    void (*tick)(void* hostUserData, void* handle);
    std::int32_t (*state)(void* hostUserData, void* handle);
    std::int32_t (*poll_events)(void* hostUserData, void* handle,
                                IceEventV1* out, std::uint32_t capacity,
                                std::uint32_t* count);
    void (*log_selected_path)(void* hostUserData, void* handle);
};

}  // namespace mimita::host
