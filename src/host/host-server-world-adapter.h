// 2026-11-13
// Resident read-only world adapter for the dedicated/listen server.
//
// Maps the authoritative resident HeadlessWorld to bounded plain-data queries.

#pragma once

#include "host/abi/world-capability.h"

namespace MimitaNet {
struct HeadlessWorld;
}

namespace mimita::host {

class HostServerWorldAdapter {
public:
    static HostServerWorldAdapter& instance();

    void bind(MimitaNet::HeadlessWorld* world);
    void setSpawnOverride(bool enabled, float x, float y, float z);
    void clear();

    const WorldAdapterV1* adapter() const { return &adapter_; }
    void* owner() { return this; }

private:
    HostServerWorldAdapter();

    static HostResult infoThunk(void* owner, WorldInfoV1* out);
    static HostResult raycastThunk(void* owner, const float origin[3],
                                   const float direction[3], float maxDistance,
                                   WorldRayHitV1* out);
    static HostResult pickSpawnThunk(void* owner, std::uint32_t playerId,
                                     WorldSpawnV1* out);

    MimitaNet::HeadlessWorld* world_ = nullptr;
    bool spawnOverrideEnabled_ = false;
    float spawnOverridePosition_[3] = {0.0f, 0.0f, 0.0f};
    WorldAdapterV1 adapter_{};
};

}  // namespace mimita::host
