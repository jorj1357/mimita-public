// 2026-11-12
// Purpose: resident host runtime for the whole-game game.dll.
//
// Owns the loader, the generation barrier, and the host service table. It is
// disabled unless MIMITA_GAME_PROBE=1, so the default EXE path is unchanged.
// With MIMITA_GAME_DLL set to a candidate DLL path, it loads one generation and
// exposes a bounded NPC target-selection probe for parity validation.
//
// This header is resident/bootstrap code. It must not move into game.dll.

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "host/abi/npc-target-probe-abi.h"
#include "host/abi/entity-capability.h"
#include "host/abi/lifetime-capability.h"
#include "host/abi/render-capability.h"
#include "host/abi/transport-capability.h"
#include "host/host-barrier.h"
#include "host/host-loader.h"

namespace mimita::host {
struct TransportAdapterV1;
}

namespace mimita::host {

class HostRuntime {
public:
    static HostRuntime& instance();

    // Reads MIMITA_GAME_PROBE and MIMITA_GAME_DLL, loads and validates one
    // generation, and commits it. Returns false and leaves the no-probe path
    // untouched on any failure.
    bool startFromEnvironment();

    // Quiesce the active generation, release it, and stop the barrier worker.
    void shutdown();

    bool probeEnabled() const { return probeEnabled_.load(std::memory_order_acquire); }

    // Parity probe: runs the real target-selection decision inside the active
    // game generation. Returns true only when the generation handled the event
    // and returned a candidate id that the host considers valid.
    bool probeNpcTargetSelect(NpcTargetDecisionV1* decision);

    // First inbound networking foundation: deliver one bounded transport
    // message to the active generation through on_event. The payload pointer is
    // valid only for the call. The resident path may still process the packet
    // for parity.
    bool deliverInbound(const TransportMessageV1& message);

    // Dispatch one inbound message and let the game fill a bounded response
    // envelope. The resident server sends the response to the exact source
    // endpoint. Returns true only when the generation handled the message.
    bool dispatchInbound(TransportDispatchV1* dispatch);

    // Poll build/game/ready.json (or MIMITA_GAME_READY) and swap a validated
    // candidate generation at a safe boundary. Called from the owning loop.
    void poll();

    // Resident state phone. The server owner registers an adapter that maps
    // stable handles to live player/NPC records. The game never sees the map.
    void setEntityAdapter(const EntityAdapterV1* adapter, void* owner);
    void clearEntityAdapter();

    std::uint64_t activeGeneration() const { return loader_.activeGeneration(); }

    // Registered by the active client/server session owner. Cleared on session
    // teardown. The adapter must stay valid while a generation can call it.
    void setTransportAdapter(const TransportAdapterV1* adapter, void* owner);
    void clearTransportAdapter();

private:
    HostRuntime();
    ~HostRuntime() = default;

    HostRuntime(const HostRuntime&) = delete;
    HostRuntime& operator=(const HostRuntime&) = delete;

    static void logThunk(void* userData, std::int32_t level, const char* message);
    static HostResult entityReadThunk(void* userData, EntityHandle handle,
                                      EntitySnapshotV1* out);
    static HostResult entityWriteThunk(void* userData, EntityHandle handle,
                                       const EntitySnapshotV1* in);
    static std::uint64_t tickThunk(void* userData);
    static double timeThunk(void* userData);
    static void* reserveThunk(void* userData, std::size_t bytes,
                              std::size_t alignment, const char* tag);
    static void releaseThunk(void* userData, void* pointer, const char* tag);
    static void* getCapabilityThunk(void* userData, std::uint32_t capability,
                                    std::uint32_t version);
    static TransportResult transportSendThunk(void* userData, PeerHandle peer,
                                              TransportChannel channel,
                                              std::uint32_t typeId,
                                              std::uint32_t flags,
                                              const std::uint8_t* payload,
                                              std::uint32_t payloadSize);
    static TransportResult transportBroadcastThunk(void* userData,
                                                   TransportChannel channel,
                                                   std::uint32_t typeId,
                                                   std::uint32_t flags,
                                                   const std::uint8_t* payload,
                                                   std::uint32_t payloadSize);
    static TransportResult transportSendToPlayerThunk(void* userData,
                                                      std::uint32_t playerId,
                                                      std::uint32_t typeId,
                                                      std::uint32_t flags,
                                                      const std::uint8_t* payload,
                                                      std::uint32_t payloadSize);
    static TransportResult transportSessionStateThunk(void* userData,
                                                      SessionStateV1* out);
    static TransportResult transportPeerStateThunk(void* userData, PeerHandle peer,
                                                   PeerStateV1* out);
    static std::uint32_t transportPeerCountThunk(void* userData);

    static HostResult entityCapabilityReadThunk(void* userData, EntityHandle handle,
                                               EntitySnapshotV1* out);
    static HostResult entityCapabilityWriteThunk(void* userData, EntityHandle handle,
                                                 const EntitySnapshotV1* in);

    static HostResult renderSubmitThunk(void* userData,
                                        const RenderCommandV1* commands,
                                        std::uint32_t count);
    static HostResult renderFlushThunk(void* userData);
    static std::uint32_t renderPendingThunk(void* userData);

    static std::uint64_t jobsAcquireThunk(void* userData, const char* tag);
    static HostResult jobsReleaseThunk(void* userData, std::uint64_t lease);
    static std::uint32_t jobsBusyThunk(void* userData);

    static std::uint64_t callbackRegisterThunk(void* userData, std::uint32_t kind,
                                               const char* tag);
    static HostResult callbackUnregisterThunk(void* userData, std::uint64_t token);
    static std::uint32_t callbackBusyThunk(void* userData);

    static std::uint64_t resourceAllocateThunk(void* userData, std::uint32_t kind,
                                               std::size_t bytes,
                                               std::size_t alignment,
                                               const char* tag);
    static void resourceReleaseThunk(void* userData, std::uint64_t resource);
    static std::uint32_t resourceBusyThunk(void* userData);

    HostLoader loader_;
    GenerationBarrier& barrier_;
    HostServicesV1 services_{};
    std::atomic<bool> probeEnabled_{false};
    std::uint64_t tick_ = 0;
    bool started_ = false;
    std::mutex probeMutex_;
    const TransportAdapterV1* transportAdapter_ = nullptr;
    void* transportOwner_ = nullptr;
    const EntityAdapterV1* entityAdapter_ = nullptr;
    void* entityOwner_ = nullptr;

    std::atomic<std::uint32_t> jobsBusy_{0};
    std::atomic<std::uint32_t> callbacksBusy_{0};
    std::atomic<std::uint32_t> renderPending_{0};
    std::atomic<std::uint64_t> nextLease_{1};
    std::unordered_map<std::uint64_t, void*> resources_;
    std::mutex resourceMutex_;
    std::string readyPath_;
    std::uint64_t lastReadyGeneration_ = 0;
    std::uint64_t lastFailedGeneration_ = 0;
    std::string lastFailedDll_;
    std::chrono::steady_clock::time_point lastFailedAt_{};
    std::chrono::steady_clock::time_point lastPoll_{};
    std::mutex swapMutex_;
};

}  // namespace mimita::host
