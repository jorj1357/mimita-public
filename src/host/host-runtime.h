// 2026-11-12
// Purpose: resident host runtime for the whole-game game.dll.
//
// Owns the loader, the generation barrier, and the host service table. Hot by
// default: it loads the newest built generation from MIMITA_GAME_DLL or
// build/game/current.json. Resident gameplay is no longer a fallback.
//
// This header is resident/bootstrap code. It must not move into game.dll.

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "host/abi/game-host-abi.h"
#include "host/abi/game-server-abi.h"
#include "host/abi/npc-target-probe-abi.h"
#include "host/abi/entity-capability.h"
#include "host/abi/lifetime-capability.h"
#include "host/abi/render-capability.h"
#include "host/abi/state-capability.h"
#include "host/abi/session-events-capability.h"
#include "host/abi/transport-capability.h"
#include "host/abi/world-capability.h"
#include "host/host-barrier.h"
#include "host/host-loader.h"

struct GLFWwindow;

namespace mimita::host {
struct TransportAdapterV1;
struct WorldAdapterV1;
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
    bool hotnessStrictMode() const;

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

    // Resident module-host callbacks for editor services. The active game
    // generation owns MapEditor and returns the bounded result.
    bool invokeMapEditorHost(MapEditorHostCallV1* call);

    // Resident effects capability calls. The active game generation owns the
    // effect pool; pointers in the request are valid only for this call.
    bool invokeEffectsHost(EffectsHostCallV1* call);

    // True when the active generation exports the server cutover behavior API.
    bool dispatchAgentCommand(const AgentCommandV1& command);
    bool serverApiAvailable() const;
    bool serverProcessPacket(ServerFrameV1* frame, const void* event,
                             ServerPacketResultV1* out);
    bool serverSimulatePlayers(ServerFrameV1* frame);
    bool serverSimulateWorld(ServerFrameV1* frame);
    bool serverPublish(ServerFrameV1* frame);
    bool serverGamemode(ServerFrameV1* frame);
    bool serverPolicyAvailable() const;
    bool serverPolicyStartup(const ServerStartupV1* startup,
                             ServerStartupResultV1* result);
    bool serverPolicyWorldLoad(const ServerWorldLoadV1* load,
                               ServerWorldLoadResultV1* result);
    bool serverPolicyConfigure(const ServerConfigureV1* configure,
                               ServerConfigureResultV1* result);
    void* serverPolicyCreatePlayer();
    void serverPolicyDestroyPlayer(void* player);
    bool serverPolicyMakeUdpConnectionId(std::uint32_t addressNetwork,
                                         std::uint16_t portNetwork,
                                         ServerConnectionIdV1* out);
    bool serverPolicyAllocateIceConnectionId(ServerConnectionIdV1* out);
    bool serverPolicyLegacyEndpoint(const ServerConnectionIdV1* id,
                                    std::uint32_t* addressNetwork,
                                    std::uint16_t* portNetwork);
    bool serverPolicyClearActorPreset(std::uint32_t playerId);
    bool serverPolicyPollReload(void* npcSystem, std::uint32_t tick);
    bool serverPolicyAuthIdentity(ServerAuthIdentityV1* out);
    bool serverPolicyProjectilePerf(ServerProjectilePerfV1* out);
    bool serverPolicyClearRuntimeObjects(const char* reason);
    bool serverPolicyCoordinatorIceDone(const char* roomCode);
    bool gameExportAvailable() const;
    int gameRunExport(const char* clipPath, const char* outputPath,
                      int width, int height, int visible, int verbose);
    bool moduleHostQueryAvailable() const;
    std::uint64_t moduleHostMapRevision();
    const void* moduleHostMapObjectiveConfig();
    bool moduleHostProfileFootprint(float outFootprint[3]);
    int moduleHostNpcLineOfSightBlocked(const float from[3],
                                        const float to[3]);
    int moduleHostNpcRaycastBlocked(const float from[3],
                                    const float direction[3],
                                    float maxDistance, float* outDistance);
    void moduleHostSetNpcWorld(const void* world);

    // Drive one whole-game fixed tick and render call through the barrier.
    // Safe-point ownership remains with the caller: this does not advance the
    // legacy EXE game loop by itself.
    HostResult driveTick(double deltaSeconds);
    HostResult driveRender(void* renderContext);
    HostResult driveFrame(double deltaSeconds, void* renderContext);

    // True only when the active generation declares a real whole-game frame and
    // the owner explicitly enabled host-owned frame mode.
    bool ownsGameFrame() const;
    void installInputCallbacks(GLFWwindow* window);
    // Queue a launcher auto-join request for delivery to the active generation.
    void queueStartupConnect(const StartupConnectV1& request);
    MenuStateV1* menuState() { return &menuState_; }
    const HostServicesV1* services() const { return &services_; }
    void* ensureStateObject(std::uint32_t id, std::uint32_t size,
                            std::uint32_t alignment, std::int32_t* created);
    void* ensureStateObjectMigrating(std::uint32_t id, std::uint32_t size,
                                     std::uint32_t alignment,
                                     mimita::host::StateMigrateFn migrate,
                                     std::int32_t* created);
    // True on the thread that owns the GL context. Off-thread GL calls are
    // refused (and deletions deferred) so a game worker cannot crash the
    // driver; the game must marshal uploads to the main thread.
    bool onGlThread(const char* op);
    void markGlThread();
    void deferGlDelete(std::uint32_t kind, std::uint32_t id);
    void processDeferredGlDeletes();
    bool bootstrapGame(const GameStartupV1& startup);
    bool gameShouldClose() const;

    // Poll build/game/ready.json (or MIMITA_GAME_READY) and swap a validated
    // candidate generation at a safe boundary. Called from the owning loop.
    void poll();

    // Resident state phone. The server owner registers an adapter that maps
    // stable handles to live player/NPC records. The game never sees the map.
    void setEntityAdapter(const EntityAdapterV1* adapter, void* owner);
    void clearEntityAdapter();
    void setWorldAdapter(const WorldAdapterV1* adapter, void* owner);
    void clearWorldAdapter();

    std::uint64_t activeGeneration() const { return loader_.activeGeneration(); }
    void* activeGameModule() const {
        return loader_.activeGenerationInfo().module;
    }
    const GameServerV1* activeServerApi() const {
        return loader_.activeGenerationInfo().serverApi;
    }
    const GameServerPolicyV1* activeServerPolicy() const {
        return loader_.activeGenerationInfo().serverPolicy;
    }

    // Registered by the active client/server session owner. Cleared on session
    // teardown. The adapter must stay valid while a generation can call it.
    void setTransportAdapter(const TransportAdapterV1* adapter, void* owner);
    void clearTransportAdapter();

private:
    HostRuntime();
    ~HostRuntime();

    struct ReadySnapshot {
        bool invalid = false;
        std::uint64_t generation = 0;
        std::string dll;
        std::string error;
    };

    void startReadyWatcher();
    void stopReadyWatcher();

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
    static TransportResult transportSendToEndpointThunk(
        void* userData, std::uint32_t addressNetworkOrder,
        std::uint16_t portNetworkOrder, TransportChannel channel,
        std::uint32_t typeId, std::uint32_t flags,
        const std::uint8_t* payload, std::uint32_t payloadSize);
    static TransportResult transportBindSessionThunk(void* userData,
                                                     PeerHandle peer,
                                                     std::uint32_t playerId);

    static HostResult entityCapabilityReadThunk(void* userData, EntityHandle handle,
                                               EntitySnapshotV1* out);
    static HostResult entityCapabilityWriteThunk(void* userData, EntityHandle handle,
                                                 const EntitySnapshotV1* in);
    static std::uint32_t entityCapabilityListThunk(void* userData,
                                                   std::int32_t kind,
                                                   EntityHandle* out,
                                                   std::uint32_t capacity);
    static HostResult entityCapabilityCreateThunk(void* userData,
                                                  std::int32_t kind,
                                                  EntityHandle* out);
    static HostResult entityCapabilityResetTransformThunk(
        void* userData, EntityHandle handle, const float position[3],
        const float velocity[3], float yaw, std::uint32_t onGround,
        std::uint32_t invulnerabilityTicks, std::uint32_t maxAirJumps,
        std::uint32_t awaitAck);
    static HostResult entityCapabilityRemoveThunk(void* userData,
                                                  EntityHandle handle);
    static HostResult entityCapabilityFindConnectionThunk(
        void* userData, std::uint64_t peer, EntityHandle* out);
    static HostResult entityCapabilityFindReconnectTokenThunk(
        void* userData, const char* token, std::uint32_t includePrevious,
        EntityHandle* out);
    static HostResult entityCapabilityFindJoinTokenThunk(
        void* userData, const char* token, EntityHandle* out);
    static HostResult entityCapabilitySendAvatarManifestsThunk(
        void* userData, EntityHandle receiver);
    static std::uint32_t sessionGetTextThunk(void* userData,
                                             std::uint32_t field, char* out,
                                             std::uint32_t capacity);
    static void sessionSetTextThunk(void* userData, std::uint32_t field,
                                    const char* value);
    static std::uint32_t sessionGetFlagThunk(void* userData,
                                             std::uint32_t field);
    static void sessionSetFlagThunk(void* userData, std::uint32_t field,
                                    std::uint32_t value);
    static void sessionSetPasswordThunk(void* userData,
                                        std::uint32_t protectedFlag,
                                        const char* password);
    static std::uint32_t sessionPasswordMatchesThunk(void* userData,
                                                     const char* supplied);
    static std::uint32_t sessionEventsNextIdThunk(void* userData);
    static std::uint32_t sessionEventsSessionIdThunk(void* userData,
                                                     std::uint32_t playerId);
    static SessionEventQueueResult sessionEventsQueueToPlayerThunk(
        void* userData, std::uint32_t playerId, const std::uint8_t* bytes,
        std::uint32_t size, std::uint32_t eventId, std::uint32_t maxPending);
    static SessionEventQueueResult sessionEventsQueueToAllThunk(
        void* userData, const std::uint8_t* bytes, std::uint32_t size,
        std::uint32_t eventId, std::uint32_t maxPending);
    static HostResult sessionEventsResetPlayerThunk(void* userData,
                                                    std::uint32_t playerId);

    static HostResult worldInfoThunk(void* userData, WorldInfoV1* out);
    static HostResult worldRaycastThunk(void* userData, const float origin[3],
                                        const float direction[3],
                                        float maxDistance, WorldRayHitV1* out);
    static HostResult worldPickSpawnThunk(void* userData,
                                          std::uint32_t playerId,
                                          WorldSpawnV1* out);

    static HostResult renderSubmitThunk(void* userData,
                                        const RenderCommandV1* commands,
                                        std::uint32_t count);
    static HostResult renderFlushThunk(void* userData);
    static std::uint32_t renderPendingThunk(void* userData);

    static std::uint64_t jobsAcquireThunk(void* userData, const char* tag);
    static HostResult jobsReleaseThunk(void* userData, std::uint64_t lease);
    static std::uint32_t jobsBusyThunk(void* userData);
    static void jobsSubmitThunk(void* userData, void* job,
                                void (*invoke)(void*), void (*destroy)(void*),
                                const char* tag);
    static void jobsDoneThunk(void* userData);
    static std::uint64_t jobsStartWorkerThunk(void* userData, void* job,
                                              void (*invoke)(void*),
                                              void (*destroy)(void*),
                                              const char* tag);
    static void jobsWaitWorkerThunk(void* userData, std::uint64_t handle);
    static void jobsReleaseWorkerThunk(void* userData, std::uint64_t handle);

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

    static void inputCursorCallback(GLFWwindow* window, double x, double y);
    static void inputKeyCallback(GLFWwindow* window, int key, int scancode,
                                 int action, int mods);
    static void inputMouseButtonCallback(GLFWwindow* window, int button,
                                         int action, int mods);
    static void inputCharCallback(GLFWwindow* window, unsigned int codepoint);
    static void inputScrollCallback(GLFWwindow* window, double xOffset,
                                    double yOffset);
    static void inputDropCallback(GLFWwindow* window, int count,
                                  const char** paths);
    void enqueueInput(const GameInputEventV1& event);
    void dispatchQueuedInput();
    void dispatchQueuedStartup();

    HostLoader loader_;
    GenerationBarrier& barrier_;
    HostServicesV1 services_{};
    MenuStateV1 menuState_{sizeof(MenuStateV1), 1, 0, -1, false, false, false};
    struct StateObjectRecord {
        void* storage = nullptr;
        std::uint32_t size = 0;
        std::uint32_t alignment = 0;
    };
    std::mutex stateObjectMutex_;
    std::unordered_map<std::uint32_t, StateObjectRecord> stateObjects_;
    std::thread::id glThreadId_{};
    std::atomic<bool> glThreadKnown_{false};
    std::mutex glDeleteMutex_;
    struct DeferredGlDelete {
        std::uint32_t kind = 0;
        std::uint32_t id = 0;
    };
    std::vector<DeferredGlDelete> deferredGlDeletes_;
    std::mutex inputMutex_;
    std::deque<GameInputEventV1> inputQueue_;
    std::mutex startupMutex_;
    std::deque<StartupConnectV1> startupQueue_;
    std::atomic<bool> probeEnabled_{false};
    std::uint64_t tick_ = 0;
    bool started_ = false;
    std::mutex probeMutex_;
    const TransportAdapterV1* transportAdapter_ = nullptr;
    void* transportOwner_ = nullptr;
    const EntityAdapterV1* entityAdapter_ = nullptr;
    void* entityOwner_ = nullptr;
    const WorldAdapterV1* worldAdapter_ = nullptr;
    void* worldOwner_ = nullptr;

    std::atomic<std::uint32_t> jobsBusy_{0};
    std::atomic<std::uint32_t> callbacksBusy_{0};
    std::atomic<std::uint32_t> renderPending_{0};
    std::atomic<std::uint64_t> nextLease_{1};
    std::unordered_map<std::uint64_t, void*> resources_;
    std::mutex resourceMutex_;
    std::string readyPath_;
    std::uint64_t lastReadyGeneration_ = 0;
    std::uint64_t activePublishedGeneration_ = 0;
    std::uint64_t lastFailedGeneration_ = 0;
    std::string lastFailedDll_;
    std::chrono::steady_clock::time_point lastFailedAt_{};
    std::mutex swapMutex_;
    std::thread readyWatchThread_;
    std::atomic<bool> readyWatchStop_{false};
    std::atomic<bool> readyPending_{false};
    std::mutex readyResultMutex_;
    ReadySnapshot readySnapshot_;
};

}  // namespace mimita::host
