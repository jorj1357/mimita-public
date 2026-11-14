// 2026-11-12
// Resident entity adapter for the dedicated server.
//
// Maps stable entity handles to live ServerPlayer/ServerNpc records. The game
// generation only receives a bounded EntitySnapshotV1.

#pragma once

#include <cstdint>
#include <unordered_map>

#include "host/abi/entity-capability.h"
#include "network/server.h"

namespace mimita::host {

class HostServerEntityAdapter {
public:
    static HostServerEntityAdapter& instance();

    void bindPlayers(std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players);
    void bindNpcs(std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>* npcs);
    void bindNextIds(std::uint32_t* nextPlayerId, std::uint32_t* nextNpcId);
    void clear();

    const EntityAdapterV1* adapter() const { return &adapter_; }
    void* owner() { return this; }

private:
    HostServerEntityAdapter();

    static HostResult readThunk(void* owner, EntityHandle handle,
                                EntitySnapshotV1* out);
    static HostResult writeThunk(void* owner, EntityHandle handle,
                                 const EntitySnapshotV1* in);
    static std::uint32_t listThunk(void* owner, std::int32_t kind,
                                   EntityHandle* out, std::uint32_t capacity);
    static HostResult createThunk(void* owner, std::int32_t kind,
                                  EntityHandle* out);
    static HostResult resetTransformThunk(void* owner, EntityHandle handle,
                                          const float position[3],
                                          const float velocity[3], float yaw,
                                          std::uint32_t onGround,
                                          std::uint32_t invulnerabilityTicks,
                                          std::uint32_t maxAirJumps,
                                          std::uint32_t awaitAck);
    static HostResult removeThunk(void* owner, EntityHandle handle);
    static HostResult findConnectionThunk(void* owner, std::uint64_t peer,
                                          EntityHandle* out);
    static HostResult findReconnectTokenThunk(void* owner, const char* token,
                                              std::uint32_t includePrevious,
                                              EntityHandle* out);
    static HostResult findJoinTokenThunk(void* owner, const char* token,
                                         EntityHandle* out);

    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players_ = nullptr;
    std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>* npcs_ = nullptr;
    std::uint32_t* nextPlayerId_ = nullptr;
    std::uint32_t* nextNpcId_ = nullptr;
    EntityAdapterV1 adapter_{};
};

}  // namespace mimita::host
