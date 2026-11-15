// 2026-11-14
// Per-domain resident state capability. Records are generated from
// config/hot-reload/state-schema.json; the host owns storage and the game
// reads/writes one domain at a time so unrelated fields cannot be clobbered.
#pragma once

#include "host/abi/entity-capability.h"
#include "host/generated/resident-state-v1.h"

namespace mimita::host {

inline constexpr std::uint32_t kEntityDomainCapabilityVersion = 2u;

enum class DomainId : std::uint32_t {
    PlayerCore = 1,
    PlayerSession = 2,
    PlayerChat = 3,
    PlayerVip = 4,
    PlayerCombat = 5,
    PlayerMovement = 6,
    NpcCore = 7,
    NpcCombat = 8,
};

struct EntityDomainCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    HostResult (*read_player_core)(void* hostUserData, EntityHandle handle,
                                   state::player_core::Record* out);
    HostResult (*write_player_core)(void* hostUserData, EntityHandle handle,
                                    const state::player_core::Record* in);
    HostResult (*read_player_session)(void* hostUserData, EntityHandle handle,
                                      state::player_session::Record* out);
    HostResult (*write_player_session)(void* hostUserData, EntityHandle handle,
                                       const state::player_session::Record* in);
    HostResult (*read_domain)(void* hostUserData, DomainId domain,
                              EntityHandle handle, void* record,
                              std::uint32_t recordSize);
    HostResult (*write_domain)(void* hostUserData, DomainId domain,
                               EntityHandle handle, const void* record,
                               std::uint32_t recordSize);
};

}  // namespace mimita::host
