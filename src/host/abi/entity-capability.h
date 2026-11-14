// 2026-11-12
// Versioned entity/state capability for a game.dll generation.
//
// The resident host owns the records. The game generation reads and writes a
// bounded snapshot by stable handle. No C++ container, pointer into game-DLL
// memory, or reference to a live gameplay object crosses this boundary.

#pragma once

#include <cstdint>

#include "host/abi/game-host-abi.h"

namespace mimita::host {

inline constexpr std::uint32_t kEntityCapabilityVersion = 11u;
inline constexpr std::uint64_t kEntityKindShift = 56u;
inline constexpr std::uint64_t kEntityIdMask = (1ull << kEntityKindShift) - 1u;

enum class EntityKind : std::uint32_t {
    Unknown = 0,
    Player = 1,
    Npc = 2,
};

enum class PlayerFlag : std::uint32_t {
    Host = 1u << 0,
    AdminEnabled = 1u << 1,
    AdminGranted = 1u << 2,
    Stale = 1u << 3,
    Dead = 1u << 4,
    Downed = 1u << 5,
};

inline EntityHandle makeEntityHandle(EntityKind kind, std::uint32_t id) {
    return (static_cast<std::uint64_t>(kind) << kEntityKindShift) |
           (static_cast<std::uint64_t>(id) & kEntityIdMask);
}

inline EntityKind entityKind(EntityHandle handle) {
    return static_cast<EntityKind>(handle >> kEntityKindShift);
}

inline std::uint32_t entityId(EntityHandle handle) {
    return static_cast<std::uint32_t>(handle & kEntityIdMask);
}

struct EntityAdapterV1 {
    std::uint32_t version;
    std::uint32_t structSize;
    void* owner;

    HostResult (*read)(void* owner, EntityHandle handle, EntitySnapshotV1* out);
    HostResult (*write)(void* owner, EntityHandle handle,
                        const EntitySnapshotV1* in);
    // Returns the total count for `kind`; writes at most `capacity` handles.
    std::uint32_t (*list)(void* owner, std::int32_t kind, EntityHandle* out,
                          std::uint32_t capacity);
    // Creates a default record of `kind` and returns its stable handle.
    HostResult (*create)(void* owner, std::int32_t kind, EntityHandle* out);
    // Applies an authoritative transform + movement lifecycle reset.
    HostResult (*reset_transform)(void* owner, EntityHandle handle,
                                  const float position[3],
                                  const float velocity[3], float yaw,
                                  std::uint32_t onGround,
                                  std::uint32_t invulnerabilityTicks,
                                  std::uint32_t maxAirJumps,
                                  std::uint32_t awaitAck);
    HostResult (*remove)(void* owner, EntityHandle handle);
    HostResult (*find_connection)(void* owner, std::uint64_t peer,
                                  EntityHandle* out);
    HostResult (*find_reconnect_token)(void* owner, const char* token,
                                       std::uint32_t includePrevious,
                                       EntityHandle* out);
    HostResult (*find_join_token)(void* owner, const char* token,
                                  EntityHandle* out);
    // Sends every other spawned player's stored avatar manifest chunks to the
    // receiver through the resident transport.
    HostResult (*send_avatar_manifests)(void* owner, EntityHandle receiver);
};

struct EntityCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    HostResult (*read)(void* hostUserData, EntityHandle handle,
                       EntitySnapshotV1* out);
    HostResult (*write)(void* hostUserData, EntityHandle handle,
                        const EntitySnapshotV1* in);
    std::uint32_t (*list)(void* hostUserData, std::int32_t kind,
                          EntityHandle* out, std::uint32_t capacity);
    HostResult (*create)(void* hostUserData, std::int32_t kind,
                         EntityHandle* out);
    HostResult (*reset_transform)(void* hostUserData, EntityHandle handle,
                                  const float position[3],
                                  const float velocity[3], float yaw,
                                  std::uint32_t onGround,
                                  std::uint32_t invulnerabilityTicks,
                                  std::uint32_t maxAirJumps,
                                  std::uint32_t awaitAck);
    HostResult (*remove)(void* hostUserData, EntityHandle handle);
    HostResult (*find_connection)(void* hostUserData, std::uint64_t peer,
                                  EntityHandle* out);
    HostResult (*find_reconnect_token)(void* hostUserData, const char* token,
                                       std::uint32_t includePrevious,
                                       EntityHandle* out);
    HostResult (*find_join_token)(void* hostUserData, const char* token,
                                  EntityHandle* out);
    HostResult (*send_avatar_manifests)(void* hostUserData,
                                        EntityHandle receiver);
};

}  // namespace mimita::host
