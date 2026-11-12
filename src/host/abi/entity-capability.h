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

inline constexpr std::uint32_t kEntityCapabilityVersion = 1u;
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
};

struct EntityCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    HostResult (*read)(void* hostUserData, EntityHandle handle,
                       EntitySnapshotV1* out);
    HostResult (*write)(void* hostUserData, EntityHandle handle,
                        const EntitySnapshotV1* in);
};

}  // namespace mimita::host
