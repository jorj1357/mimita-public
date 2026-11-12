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
    void clear();

    const EntityAdapterV1* adapter() const { return &adapter_; }
    void* owner() { return this; }

private:
    HostServerEntityAdapter();

    static HostResult readThunk(void* owner, EntityHandle handle,
                                EntitySnapshotV1* out);
    static HostResult writeThunk(void* owner, EntityHandle handle,
                                 const EntitySnapshotV1* in);

    std::unordered_map<std::uint32_t, MimitaNet::ServerPlayer>* players_ = nullptr;
    std::unordered_map<std::uint32_t, MimitaNet::ServerNpc>* npcs_ = nullptr;
    EntityAdapterV1 adapter_{};
};

}  // namespace mimita::host
