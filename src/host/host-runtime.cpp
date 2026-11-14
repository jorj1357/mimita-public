// 2026-11-12
// Resident host runtime implementation. Disabled unless MIMITA_GAME_PROBE=1.

#include "host/host-runtime.h"
#include "renderer/renderer.h"
#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdlib>
#include <malloc.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>

#include "debug/crash-handler.h"
#include "debug/structured-log.h"
#include "host/abi/log-capability.h"
#include "host/abi/audio-capability.h"
#include "host/abi/authored-rebuild-capability.h"
#include "host/abi/effects-capability.h"
#include "host/abi/gamemode-capability.h"
#include "host/abi/global-capability.h"
#include "host/abi/files-capability.h"
#include "host/abi/gl-resource-capability.h"
#include "host/abi/session-events-capability.h"
#include "host/abi/session-identity-capability.h"
#include "host/host-server-session-events.h"
#include "host/abi/socket-capability.h"
#include "host/host-session-identity.h"
#include "host/abi/ice-capability.h"
#include "host/abi/transport-session-capability.h"
#include "host/abi/diagnostics-capability.h"
#include "host/abi/render-device-capability.h"
#include "host/abi/state-capability.h"
#include "host/abi/persistence-capability.h"
#include "host/abi/module-capability.h"
#include "host/abi/transport-capability.h"
#include "audio/audio.h"
#include "audio/music-manager.h"
#include "duel/duel-queue.h"
#include "effects/effect-part.h"
#include "persistence/persistence-queue.h"
#include "utils/json-file-watch.h"
#include "hot-reload/module-host.h"
#include "host/transport-adapter.h"
#include "network/ice/ice-agent.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "network/ice/ice-config.h"
#include "network/server.h"
#include "combat/weapon-viewmodel.h"
#include "gui/hud/chat-bubble.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "network/server-authored-rebuild.h"

extern void* gpPlayer;
extern void* gpCamera;
extern void* gpWorld;
extern void* gpNpcSystem;
extern void* gpWeapons;
extern void* gpFreecamEnabled;
extern void* gpDeathPosition;
extern void* gpSelectedEditorObject;
extern void* gpEditorMode;
extern std::string* gpActiveGameMode;
extern std::string* gpActiveMapPath;
extern void* gpWorldLoaded;
extern void* gpReplayRecorder;
extern void* gpReplayPlayer;
extern std::unordered_map<std::string, std::unique_ptr<Player>>* gpReplayActorModels;
extern std::unordered_map<std::string, WeaponViewModel>* gpReplayWeaponModels;
extern void* gpReplayFactory;
extern void* gpReplayBrowser;
extern void* gpReplayTimeline;
extern void* gpReplayCameraMgr;
extern std::unordered_map<std::string, ActorChatState>* gpReplayChatStates;
extern std::vector<std::string>* gpReplayClipsCache;
extern std::unordered_map<int, std::string>* gpCommandBinds;
extern void* gpBindPrev;
extern void* gpDuelConfig;
extern void* gpMpContext;
extern void* gpGameState;
extern void* gpChatHistory;
extern bool gReplayExportRenderMode;
extern bool gMainmenuDebug;
extern bool gReplayCinematicMode;
extern bool gNetPresentationDebug;
extern bool gNetDebugEntities;
extern bool gRoomCodeShow;


namespace mimita::host {
namespace {

struct ResidentJob {
    void* job = nullptr;
    void (*invoke)(void*) = nullptr;
    void (*destroy)(void*) = nullptr;
    void (*done)(void*) = nullptr;
    void* doneData = nullptr;
};

class HostJobExecutor {
public:
    HostJobExecutor() : thread_([this] { run(); }) {}
    ~HostJobExecutor() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

    void submit(ResidentJob job) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            jobs_.push_back(job);
        }
        cv_.notify_one();
    }

private:
    void run() {
        for (;;) {
            ResidentJob job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty()) return;
                job = jobs_.front();
                jobs_.pop_front();
            }
            if (job.job && job.invoke) {
                try { job.invoke(job.job); } catch (...) {}
            }
            if (job.job && job.destroy) job.destroy(job.job);
            if (job.done) job.done(job.doneData);
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<ResidentJob> jobs_;
    bool stop_ = false;
    std::thread thread_;
};

HostJobExecutor gJobExecutor;

struct HostWorkerSlot {
    std::thread thread;
    std::atomic<bool> finished{false};
};

std::mutex gWorkerMutex;
std::unordered_map<std::uint64_t, std::shared_ptr<HostWorkerSlot>> gWorkers;
std::uint64_t gNextWorker = 1;


std::wstring widenUtf8(const char* text) {
    if (!text || !*text) {
        return std::wstring();
    }
    const int length = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<std::size_t>(length - 1), L'\0');
    if (length > 1) {
        ::MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), length);
    }
    return result;
}

void hostLogText(void*, std::int32_t category, std::int32_t level, const char* message) {
    StructuredLogger::instance().writeEvent(
        static_cast<StructuredCategory>(category),
        static_cast<StructuredLevel>(level),
        "log.text", "", message ? message : "", 0,
        nlohmann::json{{"message", message ? message : ""}},
        __FILE__, __LINE__, __FUNCTION__);
}

void hostLogEvent(void*, std::int32_t category, std::int32_t level,
                  const char* eventName, const char* correlationId,
                  const char* reason, std::uint32_t tick, const char* fieldsJson,
                  const char* sourceFile, std::int32_t sourceLine,
                  const char* functionName) {
    nlohmann::json fields = nlohmann::json::object();
    if (fieldsJson && *fieldsJson) {
        try {
            fields = nlohmann::json::parse(fieldsJson);
        } catch (...) {
            fields = nlohmann::json{{"raw", fieldsJson}};
        }
    }
    StructuredLogger::instance().writeEvent(
        static_cast<StructuredCategory>(category),
        static_cast<StructuredLevel>(level),
        eventName ? eventName : "", correlationId ? correlationId : "",
        reason ? reason : "", tick, fields,
        sourceFile, sourceLine, functionName);
}

std::int32_t hostShouldLog(void*, std::int32_t category, std::int32_t level) {
    return StructuredLogger::instance().shouldLog(
        static_cast<StructuredCategory>(category),
        static_cast<StructuredLevel>(level)) ? 1 : 0;
}

std::int32_t moduleInvoke(void*, ModuleOp op, const void* input,
                          std::uint32_t inputSize, void* output,
                          std::uint32_t outputSize) {
    auto& host = ModuleHost::instance();
    const auto writeInt = [&](std::int32_t value) {
        if (output && outputSize >= sizeof(std::int32_t)) {
            *static_cast<std::int32_t*>(output) = value;
        }
    };
    const auto moduleName = [&](const char*& name) {
        if (!input || inputSize == 0) {
            return false;
        }
        name = static_cast<const char*>(input);
        return name[0] != '\0';
    };
    switch (op) {
        case ModuleOp::ActiveGeneration: {
            const char* name = nullptr;
            if (!moduleName(name)) return 0;
            if (output && outputSize >= sizeof(std::uint32_t)) {
                *static_cast<std::uint32_t*>(output) = host.activeGeneration(name);
            }
            return 1;
        }
        case ModuleOp::ModuleLoaded: {
            const char* name = nullptr;
            if (!moduleName(name)) return 0;
            writeInt(host.moduleLoaded(name) ? 1 : 0);
            return 1;
        }
        case ModuleOp::MapEditorLoaded:
            writeInt(host.mapEditorLoaded() ? 1 : 0);
            return 1;
        case ModuleOp::NpcPerceptionLoaded:
            writeInt(host.npcPerceptionLoaded() ? 1 : 0);
            return 1;
        case ModuleOp::NpcBehaviorLoaded:
            writeInt(host.npcBehaviorLoaded() ? 1 : 0);
            return 1;
        case ModuleOp::BeginFixedTick:
            host.beginFixedTick();
            return 1;
        case ModuleOp::EndFixedTick:
            host.endFixedTick();
            return 1;
        case ModuleOp::Startup:
            host.startup();
            return 1;
        case ModuleOp::Poll:
            host.poll();
            return 1;
        case ModuleOp::WorldgenResolve:
            return host.worldgenResolve(static_cast<mimita_worldgen::ResolveV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NavigationPolicy:
            return host.navigationPolicy(static_cast<mimita_navigation_policy::NavigationPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::CollisionActorPairPolicy:
            return host.collisionActorPairPolicy(static_cast<mimita_collision_policy::ActorPairPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::MapGeometryNodePolicy:
            return host.mapGeometryNodePolicy(static_cast<mimita_map_geometry::NodePolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkHitClaimPolicy:
            return host.networkHitClaimPolicy(static_cast<mimita_network_policy::HitClaimPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkMovementValidationPolicy:
            return host.networkMovementValidationPolicy(static_cast<mimita_network_policy::MovementValidationPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcBehaviorSelfTest:
            return host.npcBehaviorSelfTest(static_cast<mimita_npc_behavior::SelfTestV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcGrenadeDecision:
            return host.npcGrenadeDecision(static_cast<mimita_npc_behavior::GrenadeDecisionV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcPerceptionStep:
            return host.npcPerceptionStep(static_cast<mimita_npc_perception::PerceptionRequestV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcWeaponChoice:
            return host.npcWeaponChoice(static_cast<mimita_npc_behavior::WeaponChoiceV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::ChunkCollisionBuild:
            return host.chunkCollisionBuild(static_cast<mimita_chunk_collision::BuildChunkV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::CollisionContactPolicy:
            return host.collisionContactPolicy(static_cast<mimita_collision_policy::ContactPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::CollisionFinalPosePolicy:
            return host.collisionFinalPosePolicy(static_cast<mimita_collision_policy::FinalPosePolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::CollisionSolveActorWorld:
            return host.collisionSolveActorWorld(static_cast<mimita_collision_policy::ActorWorldSolveV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::CollisionSolveContacts:
            return host.collisionSolveContacts(static_cast<mimita_collision_policy::ContactSolveV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::CollisionSurfacePolicy:
            return host.collisionSurfacePolicy(static_cast<mimita_collision_policy::SurfacePolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::MapEditorMapCursor:
            return host.mapEditorMapCursor(static_cast<mimita_map_editor::EditorInputV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::MapEditorResolveClick:
            return host.mapEditorResolveClick(static_cast<mimita_map_editor::EditorClickV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::MapGeometryObjectPlan:
            return host.mapGeometryObjectPlan(static_cast<mimita_map_geometry::ObjectPlanV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkCombatValidationPolicy:
            return host.networkCombatValidationPolicy(static_cast<mimita_network_policy::CombatValidationPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkInterpolationPolicy:
            return host.networkInterpolationPolicy(static_cast<mimita_network_policy::InterpolationPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkMapSyncPolicy:
            return host.networkMapSyncPolicy(static_cast<mimita_network_policy::MapSyncPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkMovementCorrection:
            return host.networkMovementCorrection(static_cast<mimita_network_policy::MovementCorrectionPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkReconcilePolicy:
            return host.networkReconcilePolicy(static_cast<mimita_network_policy::ReconcilePolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkReconnectPolicy:
            return host.networkReconnectPolicy(static_cast<mimita_network_policy::ReconnectPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkSessionHealthPolicy:
            return host.networkSessionHealthPolicy(static_cast<mimita_network_policy::SessionHealthPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NetworkSnapshotPolicy:
            return host.networkSnapshotPolicy(static_cast<mimita_network_policy::SnapshotPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcActorState:
            return host.npcActorState(static_cast<mimita_npc_behavior::ActorStateV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcAimPolicy:
            return host.npcAimPolicy(static_cast<mimita_npc_behavior::AimPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcAttackIntent:
            return host.npcAttackIntent(static_cast<mimita_npc_behavior::AttackIntentV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcBehaviorAction:
            return host.npcBehaviorAction(static_cast<mimita_npc_behavior::ActionV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcBehaviorDecision:
            return host.npcBehaviorDecision(static_cast<mimita_npc_behavior::DecisionV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcDashDecision:
            return host.npcDashDecision(static_cast<mimita_npc_behavior::DashDecisionV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcFireGate:
            return host.npcFireGate(static_cast<mimita_npc_behavior::FireGateV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcGoalScore:
            return host.npcGoalScore(static_cast<mimita_npc_behavior::GoalScoreV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcPayloadRole:
            return host.npcPayloadRole(static_cast<mimita_npc_behavior::PayloadRoleV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcStateTiming:
            return host.npcStateTiming(static_cast<mimita_npc_behavior::StateTimingV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcTargetSelect:
            return host.npcTargetSelect(static_cast<mimita_npc_behavior::TargetSelectV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::NpcTargetingPolicy:
            return host.npcTargetingPolicy(static_cast<mimita_npc_behavior::TargetingPolicyV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::WorldRenderingBuildBox:
            return host.worldRenderingBuildBox(static_cast<mimita_world_rendering::BoxMeshV1*>(const_cast<void*>(input))) ? 1 : 0;
        case ModuleOp::InvokeEvent: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.invokeEvent(a->u0, a->ptr0, a->u1, a->u2) ? 1 : 0;
        }
        case ModuleOp::MapEditorComputeDragDelta: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorComputeDragDelta(
                static_cast<const mimita_map_editor::DragMathV1*>(a->ptr0),
                static_cast<float*>(a->ptr1)) ? 1 : 0;
        }
        case ModuleOp::MapEditorPickGizmoAxis: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorPickGizmoAxis(
                static_cast<const mimita_map_editor::GizmoPickV1*>(a->ptr0),
                static_cast<int*>(a->ptr1)) ? 1 : 0;
        }
        case ModuleOp::MapEditorAddChunk: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorAddChunk(a->s0, a->outText, a->outCapacity) ? 1 : 0;
        }
        case ModuleOp::MapEditorAddChunkPart: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorAddChunkPart(a->s0, a->outText, a->outCapacity) ? 1 : 0;
        }
        case ModuleOp::MapEditorAddConnector: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorAddConnector(a->s0) ? 1 : 0;
        }
        case ModuleOp::MapEditorEvaluateConnectorPair: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorEvaluateConnectorPair(
                a->s0, a->s1, a->s2, a->s3, a->outF, a->outText,
                a->outCapacity) ? 1 : 0;
        }
        case ModuleOp::MapEditorExecuteCommand: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorExecuteCommand(a->s0, a->s1) ? 1 : 0;
        }
        case ModuleOp::MapEditorPreviewConnections: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapEditorPreviewConnections(a->outText, a->outCapacity) ? 1 : 0;
        }
        case ModuleOp::MapEditorSetSelectedPosition:
            return host.mapEditorSetSelectedPosition(
                static_cast<const float*>(input)) ? 1 : 0;
        case ModuleOp::MapGeometrySignature: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.mapGeometrySignature(
                static_cast<const char*>(a->ptr0), a->u0,
                static_cast<std::uint64_t*>(a->ptr1)) ? 1 : 0;
        }
        case ModuleOp::NpcBehaviorDashChoice: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.npcBehaviorDashChoice(a->u0, a->u1, a->f0) ? 1 : 0;
        }
        case ModuleOp::NpcBehaviorHoldPursuit: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.npcBehaviorHoldPursuit(a->u0, a->u1) ? 1 : 0;
        }
        case ModuleOp::WorldRenderingAnalyzeFaces: {
            auto* a = const_cast<ModuleArgs*>(static_cast<const ModuleArgs*>(input));
            if (!a) return 0;
            return host.worldRenderingAnalyzeFaces(
                static_cast<const float*>(a->ptr0),
                static_cast<const float*>(a->ptr1),
                static_cast<mimita_world_rendering::FaceAuditV1*>(a->ptr2),
                a->u0) ? 1 : 0;
        }
    }
    return 0;
}

std::int32_t persistenceInvoke(void*, PersistenceOp op, const void* input,
                                std::uint32_t inputSize, void* output,
                                std::uint32_t outputSize) {
    auto& queue = PersistenceQueue::instance();
    const auto writeU64 = [&](std::uint64_t value) {
        if (output && outputSize >= sizeof(std::uint64_t)) {
            *static_cast<std::uint64_t*>(output) = value;
        }
    };
    const auto writeString = [&](const std::string& value) {
        if (!output || outputSize == 0) return;
        const std::size_t count =
            value.size() < outputSize - 1 ? value.size() : outputSize - 1;
        std::memcpy(output, value.data(), count);
        static_cast<char*>(output)[count] = 0;
    };
    switch (op) {
        case PersistenceOp::ObservePlayer: {
            if (!input || inputSize < sizeof(PersistenceObserveArgs)) return 0;
            const auto* a = static_cast<const PersistenceObserveArgs*>(input);
            queue.observePlayer(a->playerId, a->userId, a->name, a->ticket,
                                a->active != 0);
            return 1;
        }
        case PersistenceOp::Tick:
            queue.tick();
            return 1;
        case PersistenceOp::EnqueueKill:
            if (!input || inputSize < sizeof(PersistenceKillEvent)) return 0;
            queue.enqueueKill(*static_cast<const PersistenceKillEvent*>(input));
            return 1;
        case PersistenceOp::EnqueueMatchResult:
            if (!input || inputSize < sizeof(PersistenceMatchEvent)) return 0;
            queue.enqueueMatchResult(*static_cast<const PersistenceMatchEvent*>(input));
            return 1;
        case PersistenceOp::TakeNotices: {
            if (!output || outputSize < sizeof(std::vector<ProgressionNotice>)) return 0;
            auto* notices = static_cast<std::vector<ProgressionNotice>*>(output);
            *notices = queue.takeNotices();
            return 1;
        }
        case PersistenceOp::SessionId:
            writeString(queue.sessionId());
            return 1;
        case PersistenceOp::QueueDepth:
            writeU64(static_cast<std::uint64_t>(queue.queueDepth()));
            return 1;
        case PersistenceOp::TotalSent:
            writeU64(queue.totalSent());
            return 1;
        case PersistenceOp::TotalRetries:
            writeU64(queue.totalRetries());
            return 1;
        case PersistenceOp::TotalDuplicates:
            writeU64(queue.totalDuplicates());
            return 1;
    }
    return 0;
}

std::int32_t duelInvoke(void*, GamemodeOp op, const void* input,
                        std::uint32_t inputSize, void* output,
                        std::uint32_t outputSize) {
    auto& duel = DuelQueue::instance();
    const auto args = [&]() -> const GamemodeTextArgs* {
        if (!input || inputSize < sizeof(GamemodeTextArgs)) return nullptr;
        return static_cast<const GamemodeTextArgs*>(input);
    };
    const auto writeInt = [&](std::int32_t value) {
        if (output && outputSize >= sizeof(std::int32_t)) {
            *static_cast<std::int32_t*>(output) = value;
        }
    };
    const auto writeFloat = [&](float value) {
        if (output && outputSize >= sizeof(float)) {
            *static_cast<float*>(output) = value;
        }
    };
    const auto writeString = [&](const std::string& value) {
        if (!output || outputSize == 0) return;
        const std::size_t count =
            value.size() < outputSize - 1 ? value.size() : outputSize - 1;
        std::memcpy(output, value.data(), count);
        static_cast<char*>(output)[count] = 0;
    };
    switch (op) {
        case GamemodeOp::DuelStopQueue:
            duel.stopQueue();
            return 1;
        case GamemodeOp::DuelCountdownActive:
            writeInt(duel.countdownActive() ? 1 : 0);
            return 1;
        case GamemodeOp::DuelCountdownLeft:
            writeFloat(duel.countdownLeft());
            return 1;
        case GamemodeOp::DuelDefaultProfileId:
            writeString(DuelQueue::defaultProfileId());
            return 1;
        case GamemodeOp::DuelOnEnemySpawn:
            if (!input || inputSize < sizeof(MimitaNet::DuelEnemySpawnPacket)) return 0;
            duel.onDuelEnemySpawn(
                *static_cast<const MimitaNet::DuelEnemySpawnPacket*>(input));
            return 1;
        case GamemodeOp::DuelOnState:
            if (!input || inputSize < sizeof(MimitaNet::DuelStatePacket)) return 0;
            duel.onDuelState(*static_cast<const MimitaNet::DuelStatePacket*>(input));
            return 1;
        case GamemodeOp::DuelOnMapChange: {
            const auto* a = args();
            if (!a) return 0;
            duel.onMapChange(a->s0, a->u0, a->u1);
            return 1;
        }
        case GamemodeOp::DuelRequestRematchNow:
            duel.requestRematchNow();
            return 1;
        case GamemodeOp::DuelRequestRematchWith: {
            const auto* a = args();
            if (!a) return 0;
            duel.requestRematchWith(a->s0);
            return 1;
        }
        case GamemodeOp::DuelReturnToQueue:
            duel.returnToQueue();
            return 1;
        case GamemodeOp::DuelStartQueue: {
            const auto* a = args();
            if (!a) return 0;
            std::vector<std::string> maps;
            const std::uint32_t count = a->mapCount < 32 ? a->mapCount : 32;
            for (std::uint32_t i = 0; i < count; ++i) {
                if (a->maps[i][0]) maps.emplace_back(a->maps[i]);
            }
            duel.startQueue(a->s0, a->s1, a->s2, maps);
            return 1;
        }
        case GamemodeOp::DuelUpdate: {
            if (!input || inputSize < sizeof(float)) return 0;
            duel.update(*static_cast<const float*>(input));
            return 1;
        }
    }
    return 0;
}

std::int32_t audioInvoke(void*, AudioOp op, const void* input,
                         std::uint32_t inputSize, void* output,
                         std::uint32_t outputSize) {
    auto& audio = ::AudioManager::instance();
    auto& music = ::MusicManager::instance();
    const auto readFloat = [&](float& value) {
        if (!input || inputSize < sizeof(float)) return false;
        value = *static_cast<const float*>(input);
        return true;
    };
    const auto readInt = [&](std::int32_t& value) {
        if (!input || inputSize < sizeof(std::int32_t)) return false;
        value = *static_cast<const std::int32_t*>(input);
        return true;
    };
    const auto readU32 = [&](std::uint32_t& value) {
        if (!input || inputSize < sizeof(std::uint32_t)) return false;
        value = *static_cast<const std::uint32_t*>(input);
        return true;
    };
    const auto writeInt = [&](std::int32_t value) {
        if (output && outputSize >= sizeof(std::int32_t)) {
            *static_cast<std::int32_t*>(output) = value;
        }
    };
    const auto writeFloat = [&](float value) {
        if (output && outputSize >= sizeof(float)) {
            *static_cast<float*>(output) = value;
        }
    };
    const auto writeU32 = [&](std::uint32_t value) {
        if (output && outputSize >= sizeof(std::uint32_t)) {
            *static_cast<std::uint32_t*>(output) = value;
        }
    };
    const auto readString = [&]() -> std::string {
        return input ? std::string(static_cast<const char*>(input)) : std::string();
    };
    const auto textPayload = [&]() -> const AudioTextPayload* {
        if (!input || inputSize < sizeof(AudioTextPayload)) return nullptr;
        return static_cast<const AudioTextPayload*>(input);
    };
    const auto writeString = [&](const std::string& value) {
        if (!output || outputSize == 0) return;
        const std::size_t count =
            value.size() < outputSize - 1 ? value.size() : outputSize - 1;
        std::memcpy(output, value.data(), count);
        static_cast<char*>(output)[count] = 0;
    };
    switch (op) {
        case AudioOp::AudioPlay: {
            const auto* p = static_cast<const AudioEventPayload*>(input);
            if (!p || inputSize < sizeof(AudioEventPayload)) return 0;
            ::AudioEvent event;
            event.name = p->name;
            event.category = static_cast<::AudioCategory>(p->category);
            event.world = p->world != 0;
            event.position = glm::vec3(p->position[0], p->position[1], p->position[2]);
            event.volume = p->volume;
            event.pitch = p->pitch;
            event.maxDistance = p->maxDistance;
            event.ownerId = p->ownerId;
            event.startSeconds = p->startSeconds;
            event.endSeconds = p->endSeconds;
            event.retrigger = p->retrigger != 0;
            audio.play(event);
            return 1;
        }
        case AudioOp::AudioStopOwner: {
            std::uint32_t owner = 0;
            if (!readU32(owner)) return 0;
            audio.stopOwner(owner);
            return 1;
        }
        case AudioOp::AudioStopAll:
            audio.stopAll();
            return 1;
        case AudioOp::AudioSetDebug: {
            std::int32_t enabled = 0;
            if (!readInt(enabled)) return 0;
            audio.setDebug(enabled != 0);
            return 1;
        }
        case AudioOp::AudioDebug:
            writeInt(audio.debug() ? 1 : 0);
            return 1;
        case AudioOp::MusicUpdate: {
            float dt = 0.0f;
            if (!readFloat(dt)) return 0;
            music.update(dt);
            return 1;
        }
        case AudioOp::MusicEnterMenuMode:
            music.enterMenuMode();
            return 1;
        case AudioOp::MusicEnterGameMode:
            music.enterGameMode();
            return 1;
        case AudioOp::MusicPlayModeTrack:
            writeInt(music.playModeTrack(readString()) ? 1 : 0);
            return 1;
        case AudioOp::MusicStopModeTrack:
            music.stopModeTrack();
            return 1;
        case AudioOp::MusicPause:
            music.pause();
            return 1;
        case AudioOp::MusicResume:
            music.resume();
            return 1;
        case AudioOp::MusicSkip:
            music.skip();
            return 1;
        case AudioOp::MusicPrevious:
            music.previous();
            return 1;
        case AudioOp::MusicSetPlaylistMode: {
            std::int32_t mode = 0;
            if (!readInt(mode)) return 0;
            music.setPlaylistMode(static_cast<::MusicManager::PlaylistMode>(mode));
            return 1;
        }
        case AudioOp::MusicSetPersonalMusicOverride: {
            std::int32_t enabled = 0;
            if (!readInt(enabled)) return 0;
            music.setPersonalMusicOverride(enabled != 0);
            return 1;
        }
        case AudioOp::MusicStop:
            music.stop();
            return 1;
        case AudioOp::MusicReload:
            music.reload();
            return 1;
        case AudioOp::MusicSetVolume: {
            float volume = 1.0f;
            if (!readFloat(volume)) return 0;
            music.setVolume(volume);
            return 1;
        }
        case AudioOp::MusicVolume:
            writeFloat(music.volume());
            return 1;
        case AudioOp::MusicSetPlaybackSpeed: {
            float speed = 1.0f;
            if (!readFloat(speed)) return 0;
            music.setPlaybackSpeed(speed);
            return 1;
        }
        case AudioOp::MusicPlaybackSpeed:
            writeFloat(music.playbackSpeed());
            return 1;
        case AudioOp::MusicSetMuted: {
            std::int32_t muted = 0;
            if (!readInt(muted)) return 0;
            music.setMuted(muted != 0);
            return 1;
        }
        case AudioOp::MusicMuted:
            writeInt(music.muted() ? 1 : 0);
            return 1;
        case AudioOp::MusicIsPlaying:
            writeInt(music.isPlaying() ? 1 : 0);
            return 1;
        case AudioOp::MusicCurrentTrackInfo: {
            const std::string info = music.currentTrackInfo();
            if (output && outputSize > 0) {
                const std::size_t count =
                    info.size() < outputSize - 1 ? info.size() : outputSize - 1;
                std::memcpy(output, info.data(), count);
                static_cast<char*>(output)[count] = 0;
            }
            return 1;
        }
        case AudioOp::AudioOwnerKey: {
            const auto* p = textPayload();
            if (!p) return 0;
            writeU32(::audioOwnerKey(p->u0, p->text));
            return 1;
        }
        case AudioOp::PlaySound: {
            const auto* p = textPayload();
            if (!p) return 0;
            ::playSound(p->text, p->f0);
            return 1;
        }
        case AudioOp::PlayEventSound: {
            const auto* p = textPayload();
            if (!p) return 0;
            ::playEventSound(p->text, p->f0);
            return 1;
        }
        case AudioOp::PlaySoundPitched: {
            const auto* p = textPayload();
            if (!p) return 0;
            ::playSoundPitched(p->text, p->f0, p->f1);
            return 1;
        }
        case AudioOp::AudioUpdate: {
            float dt = 0.0f;
            if (!readFloat(dt)) return 0;
            ::audioUpdate(dt);
            return 1;
        }
        case AudioOp::SetGlobalDuck: {
            float duck = 0.0f;
            if (!readFloat(duck)) return 0;
            ::setGlobalDuck(duck);
            return 1;
        }
        case AudioOp::GetGlobalDuck:
            writeFloat(::getGlobalDuck());
            return 1;
        case AudioOp::AudioListenerForward: {
            const glm::vec3 forward = ::audioListenerForward();
            if (output && outputSize >= 3 * sizeof(float)) {
                auto* out = static_cast<float*>(output);
                out[0] = forward.x;
                out[1] = forward.y;
                out[2] = forward.z;
            }
            return 1;
        }
        case AudioOp::SetAudioListener: {
            if (!input || inputSize < sizeof(AudioVec3PairPayload)) return 0;
            const auto* p = static_cast<const AudioVec3PairPayload*>(input);
            ::setAudioListener(glm::vec3(p->a[0], p->a[1], p->a[2]),
                               glm::vec3(p->b[0], p->b[1], p->b[2]));
            return 1;
        }
        case AudioOp::ResolveSoundPath: {
            const auto* p = textPayload();
            if (!p) return 0;
            writeString(::resolveSoundPath(p->text));
            return 1;
        }
        case AudioOp::GetAudioFileDuration: {
            const auto* p = textPayload();
            if (!p) return 0;
            writeFloat(::getAudioFileDuration(p->text));
            return 1;
        }
        case AudioOp::PlayReplayMusicPreview: {
            const auto* p = textPayload();
            if (!p) return 0;
            writeInt(::playReplayMusicPreview(p->text, p->f0) ? 1 : 0);
            return 1;
        }
        case AudioOp::StopReplayMusicPreview:
            ::stopReplayMusicPreview();
            return 1;
        case AudioOp::PauseReplayMusicPreview:
            ::pauseReplayMusicPreview();
            return 1;
        case AudioOp::ResumeReplayMusicPreview:
            ::resumeReplayMusicPreview();
            return 1;
        case AudioOp::SeekReplayMusicPreview: {
            float seconds = 0.0f;
            if (!readFloat(seconds)) return 0;
            writeInt(::seekReplayMusicPreview(seconds) ? 1 : 0);
            return 1;
        }
        case AudioOp::IsReplayMusicPreviewPlaying:
            writeInt(::isReplayMusicPreviewPlaying() ? 1 : 0);
            return 1;
        case AudioOp::SetReplayMusicPreviewSpeed: {
            float speed = 1.0f;
            if (!readFloat(speed)) return 0;
            ::setReplayMusicPreviewSpeed(speed);
            return 1;
        }
        case AudioOp::SetReplayMusicPreviewVolume: {
            float volume = 1.0f;
            if (!readFloat(volume)) return 0;
            ::setReplayMusicPreviewVolume(volume);
            return 1;
        }
        case AudioOp::GetReplayMusicPreviewDuration:
            writeFloat(::getReplayMusicPreviewDuration());
            return 1;
        case AudioOp::SetAudioSpatialWorld: {
            if (!input || inputSize < sizeof(const void*)) return 0;
            ::setAudioSpatialWorld(*static_cast<const ::World* const*>(input));
            return 1;
        }
        case AudioOp::DecodeAudioToPCM: {
            const auto* request = static_cast<const AudioDecodeRequest*>(input);
            auto* result = static_cast<AudioDecodeResult*>(output);
            if (!request || inputSize < sizeof(AudioDecodeRequest) ||
                !result || outputSize < sizeof(AudioDecodeResult)) return 0;
            std::vector<std::int16_t> decoded;
            std::uint32_t rate = 0;
            std::uint32_t channels = 0;
            const bool ok = ::decodeAudioToPCM(
                request->path, decoded, rate, channels,
                request->targetSampleRate, request->targetChannels);
            result->sampleRate = rate;
            result->channels = channels;
            result->sampleCount = static_cast<std::uint32_t>(decoded.size());
            result->ok = ok ? 1 : 0;
            if (!ok) return 0;
            if (request->capacitySamples < decoded.size() || !request->out) {
                return 1;
            }
            std::memcpy(request->out, decoded.data(), decoded.size() * sizeof(std::int16_t));
            return 1;
        }
        case AudioOp::PlayAirJumpSound:
            ::playAirJumpSound();
            return 1;
        case AudioOp::PlayMenuClick:
            ::playMenuClick();
            return 1;
        case AudioOp::PlayMenuHover:
            ::playMenuHover();
            return 1;
        case AudioOp::PlayWorldSound: {
            const auto* p = static_cast<const WorldSoundPayload*>(input);
            if (!p || inputSize < sizeof(WorldSoundPayload)) return 0;
            ::playWorldSound(p->name,
                             glm::vec3(p->position[0], p->position[1], p->position[2]),
                             p->volume, p->pitch, p->maxDistance,
                             static_cast<::AudioCategory>(p->category));
            return 1;
        }
        case AudioOp::PlayWorldSoundOwned: {
            const auto* p = static_cast<const WorldSoundPayload*>(input);
            if (!p || inputSize < sizeof(WorldSoundPayload)) return 0;
            ::playWorldSoundOwned(p->name,
                                  glm::vec3(p->position[0], p->position[1], p->position[2]),
                                  p->volume, p->pitch, p->maxDistance, p->ownerId,
                                  p->retrigger != 0,
                                  static_cast<::AudioCategory>(p->category));
            return 1;
        }
        case AudioOp::AudioListenerPosition: {
            const glm::vec3 position = ::audioListenerPosition();
            if (output && outputSize >= 3 * sizeof(float)) {
                auto* out = static_cast<float*>(output);
                out[0] = position.x;
                out[1] = position.y;
                out[2] = position.z;
            }
            return 1;
        }
        case AudioOp::ComputeImpactAudio: {
            const auto* p = static_cast<const ImpactAudioPayload*>(input);
            if (!p || inputSize < sizeof(ImpactAudioPayload)) return 0;
            float volume = 0.0f;
            float pitch = 0.0f;
            ::computeImpactAudio(p->baseVolume, p->distance, p->severity, volume, pitch);
            if (output && outputSize >= 2 * sizeof(float)) {
                auto* out = static_cast<float*>(output);
                out[0] = volume;
                out[1] = pitch;
            }
            return 1;
        }
    }
    return 0;
}

std::int32_t effectsInvoke(void*, EffectsOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize) {
    auto& effects = ::EffectPartSystem::instance();
    const auto writePointer = [&](::EffectPart* value) {
        if (output && outputSize >= sizeof(std::uintptr_t)) {
            *static_cast<std::uintptr_t*>(output) =
                reinterpret_cast<std::uintptr_t>(value);
        }
    };
    const auto payload = [&]() -> const EffectsSpawnPayload* {
        if (!input || inputSize < sizeof(EffectsSpawnPayload)) return nullptr;
        return static_cast<const EffectsSpawnPayload*>(input);
    };
    const auto vec = [](const EffectsVec3& value) {
        return glm::vec3(value.x, value.y, value.z);
    };
    switch (op) {
        case EffectsOp::Spawn: {
            const auto* effect = static_cast<const ::EffectPart*>(input);
            if (!effect || inputSize < sizeof(::EffectPart)) return 0;
            writePointer(effects.spawn(*effect));
            return 1;
        }
        case EffectsOp::SpawnFootstep: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnFootstep(vec(p->a), p->f0));
            return 1;
        }
        case EffectsOp::SpawnDash: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDash(vec(p->a), p->f0));
            return 1;
        }
        case EffectsOp::SpawnFreeze: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnFreeze(vec(p->a), p->f0));
            return 1;
        }
        case EffectsOp::SpawnDamageImpactSphere: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDamageImpactSphere(vec(p->a), vec(p->b), p->s0));
            return 1;
        }
        case EffectsOp::SpawnMuzzleFlash: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnMuzzleFlash(vec(p->a), p->s0, p->f0, p->s1,
                                                   p->b0 != 0, p->b1 != 0));
            return 1;
        }
        case EffectsOp::SpawnTracer: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnTracer(vec(p->a), vec(p->b), p->s0, p->f0, p->s1));
            return 1;
        }
        case EffectsOp::SpawnBulletImpact: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnBulletImpact(vec(p->a), vec(p->b), p->f0, p->f1,
                                                   p->f2));
            return 1;
        }
        case EffectsOp::SpawnDeathEllipsoid: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDeathEllipsoid(vec(p->a), vec(p->b), p->f0, p->f1,
                                                     p->f2, p->f3));
            return 1;
        }
        case EffectsOp::SpawnImpactSphereTickCfg: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnImpactSphereTickCfg(vec(p->a)));
            return 1;
        }
        case EffectsOp::SpawnImpactSphereTick: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnImpactSphereTick(vec(p->a), vec(p->b), p->f0));
            return 1;
        }
        case EffectsOp::SpawnFreezeTrail: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnFreezeTrail(vec(p->a)));
            return 1;
        }
        case EffectsOp::SpawnDownDash: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnDownDash(vec(p->a)));
            return 1;
        }
        case EffectsOp::SpawnBodyContactSpark: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnBodyContactSpark(vec(p->a), vec(p->b), vec(p->c),
                                                       p->f0));
            return 1;
        }
        case EffectsOp::SpawnWorldCracks: {
            const auto* p = payload();
            if (!p) return 0;
            effects.spawnWorldCracks(vec(p->a), vec(p->b), vec(p->c), p->f0, p->f1,
                                     p->f2);
            return 1;
        }
        case EffectsOp::SpawnExplosionMark: {
            const auto* p = payload();
            if (!p) return 0;
            effects.spawnExplosionMark(vec(p->a), vec(p->b));
            return 1;
        }
        case EffectsOp::QueueWorldHit: {
            const auto* p = payload();
            if (!p) return 0;
            effects.queueWorldHit(vec(p->a), vec(p->b), vec(p->c), p->f0, p->s0,
                                  p->s1);
            return 1;
        }
        case EffectsOp::DrainPendingWorldHits: {
            if (!input || inputSize < sizeof(std::int32_t)) return 0;
            effects.drainPendingWorldHits(*static_cast<const std::int32_t*>(input));
            return 1;
        }
        case EffectsOp::DestroyOwner: {
            if (!input || inputSize < sizeof(std::uint32_t)) return 0;
            effects.destroyOwner(*static_cast<const std::uint32_t*>(input));
            return 1;
        }
        case EffectsOp::PushSurfaceDecal:
            // Private resident operation; game code cannot call it directly.
            return 0;
        case EffectsOp::SpawnEntityImpact: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnEntityImpact(vec(p->a), vec(p->b), p->s0, p->s1,
                                                    p->f0));
            return 1;
        }
        case EffectsOp::SpawnWorldImpact: {
            const auto* p = payload();
            if (!p) return 0;
            writePointer(effects.spawnWorldImpact(vec(p->a), vec(p->b), p->f0, vec(p->c),
                                                   p->f1));
            return 1;
        }
        case EffectsOp::CollectAlive: {
            if (!input || inputSize < sizeof(EffectsCollectAlivePayload)) return 0;
            const auto* p = static_cast<const EffectsCollectAlivePayload*>(input);
            const int count = effects.collectAlive(
                reinterpret_cast<::EffectPartSystem::PartSnapshot*>(p->out),
                p->maxCount, p->minAlpha);
            if (output && outputSize >= sizeof(std::int32_t)) {
                *static_cast<std::int32_t*>(output) = count;
            }
            return 1;
        }
    }
    return 0;
}

void* jsonWatchCreate(void*, const char* const* roots, std::uint32_t rootCount,
                       std::uint32_t intervalMs) {
    std::vector<std::string> paths;
    for (std::uint32_t i = 0; roots && i < rootCount; ++i) {
        if (roots[i] && roots[i][0]) paths.emplace_back(roots[i]);
    }
    return new JsonFileWatch(std::move(paths),
                             std::chrono::milliseconds(intervalMs));
}

void jsonWatchDestroy(void*, void* watch) {
    delete static_cast<JsonFileWatch*>(watch);
}

std::int32_t jsonWatchConsumeDirty(void*, void* watch) {
    auto* typed = static_cast<JsonFileWatch*>(watch);
    return typed && typed->consumeDirty() ? 1 : 0;
}

void jsonWatchMarkDirty(void*, void* watch) {
    if (auto* typed = static_cast<JsonFileWatch*>(watch)) typed->markDirty();
}

std::uintptr_t globalGetPointer(void*, GlobalId id) {
    switch (id) {
        case GlobalId::Player: return reinterpret_cast<std::uintptr_t>(gpPlayer);
        case GlobalId::Camera: return reinterpret_cast<std::uintptr_t>(gpCamera);
        case GlobalId::World: return reinterpret_cast<std::uintptr_t>(gpWorld);
        case GlobalId::NpcSystem: return reinterpret_cast<std::uintptr_t>(gpNpcSystem);
        case GlobalId::Weapons: return reinterpret_cast<std::uintptr_t>(gpWeapons);
        case GlobalId::FreecamEnabled: return reinterpret_cast<std::uintptr_t>(gpFreecamEnabled);
        case GlobalId::DeathPosition: return reinterpret_cast<std::uintptr_t>(gpDeathPosition);
        case GlobalId::SelectedEditorObject: return reinterpret_cast<std::uintptr_t>(gpSelectedEditorObject);
        case GlobalId::EditorMode: return reinterpret_cast<std::uintptr_t>(gpEditorMode);
        case GlobalId::ActiveGameMode: return reinterpret_cast<std::uintptr_t>(gpActiveGameMode);
        case GlobalId::ActiveMapPath: return reinterpret_cast<std::uintptr_t>(gpActiveMapPath);
        case GlobalId::WorldLoaded: return reinterpret_cast<std::uintptr_t>(gpWorldLoaded);
        case GlobalId::ReplayRecorder: return reinterpret_cast<std::uintptr_t>(gpReplayRecorder);
        case GlobalId::ReplayPlayer: return reinterpret_cast<std::uintptr_t>(gpReplayPlayer);
        case GlobalId::ReplayActorModels: return reinterpret_cast<std::uintptr_t>(gpReplayActorModels);
        case GlobalId::ReplayWeaponModels: return reinterpret_cast<std::uintptr_t>(gpReplayWeaponModels);
        case GlobalId::ReplayFactory: return reinterpret_cast<std::uintptr_t>(gpReplayFactory);
        case GlobalId::ReplayBrowser: return reinterpret_cast<std::uintptr_t>(gpReplayBrowser);
        case GlobalId::ReplayTimeline: return reinterpret_cast<std::uintptr_t>(gpReplayTimeline);
        case GlobalId::ReplayCameraMgr: return reinterpret_cast<std::uintptr_t>(gpReplayCameraMgr);
        case GlobalId::ReplayChatStates: return reinterpret_cast<std::uintptr_t>(gpReplayChatStates);
        case GlobalId::ReplayClipsCache: return reinterpret_cast<std::uintptr_t>(gpReplayClipsCache);
        case GlobalId::CommandBinds: return reinterpret_cast<std::uintptr_t>(gpCommandBinds);
        case GlobalId::BindPrev: return reinterpret_cast<std::uintptr_t>(gpBindPrev);
        case GlobalId::DuelConfig: return reinterpret_cast<std::uintptr_t>(gpDuelConfig);
        case GlobalId::MpContext: return reinterpret_cast<std::uintptr_t>(gpMpContext);
        case GlobalId::GameState: return reinterpret_cast<std::uintptr_t>(gpGameState);
        case GlobalId::ChatHistory: return reinterpret_cast<std::uintptr_t>(gpChatHistory);
        case GlobalId::ReplayExportRenderMode: return reinterpret_cast<std::uintptr_t>(&gReplayExportRenderMode);
        case GlobalId::MainmenuDebug: return reinterpret_cast<std::uintptr_t>(&gMainmenuDebug);
        case GlobalId::ReplayCinematicMode: return reinterpret_cast<std::uintptr_t>(&gReplayCinematicMode);
        case GlobalId::NetPresentationDebug: return reinterpret_cast<std::uintptr_t>(&gNetPresentationDebug);
        case GlobalId::NetDebugEntities: return reinterpret_cast<std::uintptr_t>(&gNetDebugEntities);
        case GlobalId::RoomCodeShow: return reinterpret_cast<std::uintptr_t>(&gRoomCodeShow);
        default: return 0;
    }
}

std::int32_t globalGetBool(void*, GlobalId id) {
    switch (id) {
        case GlobalId::ReplayExportRenderMode: return gReplayExportRenderMode ? 1 : 0;
        case GlobalId::MainmenuDebug: return gMainmenuDebug ? 1 : 0;
        case GlobalId::ReplayCinematicMode: return gReplayCinematicMode ? 1 : 0;
        case GlobalId::NetPresentationDebug: return gNetPresentationDebug ? 1 : 0;
        case GlobalId::NetDebugEntities: return gNetDebugEntities ? 1 : 0;
        case GlobalId::RoomCodeShow: return gRoomCodeShow ? 1 : 0;
        default: return 0;
    }
}

Renderer* gRenderDevice = nullptr;

void* renderDeviceAcquire(void*, std::int32_t width, std::int32_t height,
                          const char* title) {
    if (!gRenderDevice) {
        gRenderDevice = new Renderer(width, height, title ? title : "MiMITA");
        HostRuntime::instance().installInputCallbacks(gRenderDevice->window);
    }
    return gRenderDevice;
}

float renderDeviceBeginFrame(void*) {
    return gRenderDevice ? gRenderDevice->beginFrame() : 0.0f;
}

void renderDeviceEndFrame(void*) {
    if (gRenderDevice) gRenderDevice->endFrame();
}

std::int32_t renderDeviceShouldClose(void*) {
    return gRenderDevice && gRenderDevice->shouldClose() ? 1 : 0;
}

void renderDeviceShutdown(void*) {
    if (gRenderDevice) gRenderDevice->shutdown();
}

std::int32_t renderDeviceApplyVideoMode(void*, std::int32_t width,
                                        std::int32_t height, std::int32_t mode) {
    if (!gRenderDevice) return static_cast<std::int32_t>(FullscreenMode::Windowed);
    return static_cast<std::int32_t>(gRenderDevice->applyVideoMode(
        width, height, static_cast<FullscreenMode>(mode)));
}

void renderDeviceSetVSync(void*, std::int32_t enabled) {
    if (gRenderDevice) gRenderDevice->setVSync(enabled != 0);
}

bool renderDeviceReloadWorldShader(void*) {
    return gRenderDevice ? gRenderDevice->reloadWorldShaderIfChanged() : false;
}

void renderDeviceSetCursorVisible(void*, std::int32_t visible) {
    if (!gRenderDevice || !gRenderDevice->window) return;
    glfwSetInputMode(gRenderDevice->window, GLFW_CURSOR,
                     visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

std::int32_t diagnosticsCaptureStack(void*, std::uint32_t threadId, char* out,
                                     std::uint32_t outSize) {
    return ::captureThreadStackTrace(threadId, out, outSize) ? 1 : 0;
}

void* iceCreate(void*) {
    return new ::IceAgent();
}

void iceDestroy(void*, void* handle) {
    delete static_cast<::IceAgent*>(handle);
}

std::int32_t iceInitialize(void*, void* handle, const void* configuration) {
    auto* agent = static_cast<::IceAgent*>(handle);
    const auto* config = static_cast<const ::IceConfiguration*>(configuration);
    if (!agent || !config) return 0;
    return agent->initialize(*config) ? 1 : 0;
}

void iceShutdown(void*, void* handle) {
    if (auto* agent = static_cast<::IceAgent*>(handle)) agent->shutdown();
}

std::int32_t iceGather(void*, void* handle) {
    auto* agent = static_cast<::IceAgent*>(handle);
    return agent && agent->gatherCandidates() ? 1 : 0;
}

std::int32_t iceLocalSdp(void*, void* handle, char* out, std::uint32_t outSize) {
    auto* agent = static_cast<::IceAgent*>(handle);
    if (!agent || !out || outSize == 0) return 0;
    const std::string sdp = agent->localSdp();
    const std::size_t count = sdp.size() < outSize - 1 ? sdp.size() : outSize - 1;
    std::memcpy(out, sdp.data(), count);
    out[count] = 0;
    return 1;
}

std::int32_t iceSetRemote(void*, void* handle, const char* sdp) {
    auto* agent = static_cast<::IceAgent*>(handle);
    return agent && sdp && agent->setRemoteDescription(sdp) ? 1 : 0;
}

std::int32_t iceSend(void*, void* handle, const void* data, std::uint32_t size) {
    auto* agent = static_cast<::IceAgent*>(handle);
    return agent && agent->send(data, size) ? 1 : 0;
}

void iceTick(void*, void* handle) {
    if (auto* agent = static_cast<::IceAgent*>(handle)) agent->tick();
}

std::int32_t iceState(void*, void* handle) {
    auto* agent = static_cast<::IceAgent*>(handle);
    return agent ? static_cast<std::int32_t>(agent->state()) : 0;
}

std::vector<std::uint8_t> gIceEventScratch;

std::int32_t icePollEvents(void*, void* handle, IceEventV1* out,
                           std::uint32_t capacity, std::uint32_t* count) {
    if (count) *count = 0;
    auto* agent = static_cast<::IceAgent*>(handle);
    if (!agent || !out || capacity == 0) return 0;
    std::vector<::IceEvent> events;
    agent->pollEvents(events);
    const std::uint32_t copied =
        events.size() < capacity ? static_cast<std::uint32_t>(events.size()) : capacity;
    std::size_t totalBytes = 0;
    for (std::uint32_t i = 0; i < copied; ++i) totalBytes += events[i].data.size();
    gIceEventScratch.clear();
    gIceEventScratch.reserve(totalBytes);
    for (std::uint32_t i = 0; i < copied; ++i) {
        const std::size_t offset = gIceEventScratch.size();
        gIceEventScratch.insert(gIceEventScratch.end(),
                                events[i].data.begin(), events[i].data.end());
        out[i].kind = static_cast<std::int32_t>(events[i].type);
        out[i].newState = static_cast<std::int32_t>(events[i].newState);
        out[i].dataSize = static_cast<std::uint32_t>(events[i].data.size());
        out[i].data = events[i].data.empty()
            ? nullptr
            : reinterpret_cast<const std::uint8_t*>(gIceEventScratch.data() + offset);
    }
    if (count) *count = copied;
    return 1;
}

void iceLogSelectedPath(void*, void* handle) {
    if (auto* agent = static_cast<::IceAgent*>(handle)) agent->logSelectedPath();
}

MimitaNet::ListenServerState* gTransportSessionState = nullptr;

void* transportServerState(void*) {
    if (!gTransportSessionState) gTransportSessionState = new MimitaNet::ListenServerState();
    return gTransportSessionState;
}

std::int32_t transportServerStart(void*, void* state, std::uint16_t port,
                                  const char* publicIp, const char* hostSessionId,
                                  const void* settings) {
    auto* server = static_cast<MimitaNet::ListenServerState*>(state);
    if (!server) return 0;
    const auto* launch = static_cast<const MimitaNet::ServerLaunchSettings*>(settings);
    return MimitaNet::startListenServer(
        *server, port, publicIp ? publicIp : "", hostSessionId ? hostSessionId : "",
        launch) ? 1 : 0;
}

void transportServerStop(void*, void* state) {
    if (auto* server = static_cast<MimitaNet::ListenServerState*>(state))
        MimitaNet::stopListenServer(*server);
}

void transportServerTick(void*, void* state, float dt) {
    if (auto* server = static_cast<MimitaNet::ListenServerState*>(state))
        MimitaNet::tickListenServer(*server, dt);
}

std::int32_t transportServerInitIce(void*, void* state) {
    auto* server = static_cast<MimitaNet::ListenServerState*>(state);
    return server && MimitaNet::initServerIceListener(*server) ? 1 : 0;
}

std::int32_t transportSetNonBlocking(void*, std::uint64_t socketHandle) {
    return MimitaNet::setNonBlocking(static_cast<SOCKET>(socketHandle)) ? 1 : 0;
}

std::int32_t transportDisableUdpConnReset(void*, std::uint64_t socketHandle) {
    return MimitaNet::disableUdpConnReset(static_cast<SOCKET>(socketHandle)) ? 1 : 0;
}

std::uint32_t transportServerHostPlayerId(void*) {
    return MimitaNet::gServerHostPlayerId;
}

void transportServerSetHostPlayerId(void*, std::uint32_t playerId) {
    MimitaNet::gServerHostPlayerId = playerId;
}

void* authoredRebuildWorker(void*) {
    return &MimitaNet::AuthoredWorldRebuildWorker::instance();
}

void authoredRebuildSubmit(void*, void* worker, const void* request) {
    auto* target = static_cast<MimitaNet::AuthoredWorldRebuildWorker*>(worker);
    const auto* value = static_cast<const MimitaNet::AuthoredRebuildRequest*>(request);
    if (target && value) target->submit(*value);
}

void authoredRebuildTryTake(void*, void* worker, void* outSharedPtr) {
    auto* target = static_cast<MimitaNet::AuthoredWorldRebuildWorker*>(worker);
    auto* output = static_cast<std::shared_ptr<MimitaNet::AuthoredRebuildResult>*>(outSharedPtr);
    if (target && output) *output = target->tryTakeResult();
}

void authoredRebuildRetire(void*, void* worker, void* inoutSharedPtr) {
    auto* target = static_cast<MimitaNet::AuthoredWorldRebuildWorker*>(worker);
    auto* value = static_cast<std::shared_ptr<MimitaNet::AuthoredRebuildResult>*>(inoutSharedPtr);
    if (target && value) target->retire(std::move(*value));
}

void authoredRebuildCancel(void*, void* worker) {
    if (auto* target = static_cast<MimitaNet::AuthoredWorldRebuildWorker*>(worker))
        target->cancelAndWait();
}

std::unordered_map<std::uint64_t, std::ofstream> gFileHandles;
std::uint64_t gNextFileHandle = 1;

std::uint64_t filesOpenWrite(void*, const char* path) {
    if (!path || !*path) return 0;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return 0;
    const std::uint64_t handle = gNextFileHandle++;
    gFileHandles.emplace(handle, std::move(stream));
    return handle;
}

std::int32_t filesWrite(void*, std::uint64_t handle, const void* data,
                        std::uint32_t size) {
    auto it = gFileHandles.find(handle);
    if (it == gFileHandles.end() || (!data && size)) return 0;
    if (size) it->second.write(static_cast<const char*>(data), size);
    return it->second.good() ? 1 : 0;
}

void filesClose(void*, std::uint64_t handle) {
    auto it = gFileHandles.find(handle);
    if (it == gFileHandles.end()) return;
    it->second.flush();
    it->second.close();
    gFileHandles.erase(it);
}

std::unordered_map<std::uint64_t, SOCKET> gSockets;
std::uint64_t gNextSocket = 1;
bool gWsaReady = false;

void ensureWsa() {
    if (gWsaReady) return;
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) == 0) gWsaReady = true;
}

std::uint64_t socketOpenUdp(void*) {
    ensureWsa();
    const SOCKET socketHandle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketHandle == INVALID_SOCKET) return 0;
    const std::uint64_t handle = gNextSocket++;
    gSockets[handle] = socketHandle;
    return handle;
}

void socketClose(void*, std::uint64_t handle) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end()) return;
    ::closesocket(it->second);
    gSockets.erase(it);
}

std::int32_t socketBindAddress(void*, std::uint64_t handle, const void* address,
                               std::uint32_t addressSize) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end() || !address || addressSize < sizeof(sockaddr_in)) return 0;
    return ::bind(it->second, static_cast<const sockaddr*>(address),
                  static_cast<int>(addressSize)) == 0 ? 1 : 0;
}

std::int32_t socketGetLocalAddress(void*, std::uint64_t handle, void* address,
                                   std::uint32_t addressSize) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end() || !address || addressSize < sizeof(sockaddr_in)) return 0;
    int length = static_cast<int>(addressSize);
    return ::getsockname(it->second, static_cast<sockaddr*>(address), &length) == 0 ? 1 : 0;
}

std::int32_t socketSetNonblocking(void*, std::uint64_t handle) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end()) return 0;
    u_long enabled = 1;
    return ::ioctlsocket(it->second, FIONBIO, &enabled) == 0 ? 1 : 0;
}

std::int32_t socketSetReuse(void*, std::uint64_t handle, std::int32_t enabled) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end()) return 0;
    BOOL value = enabled != 0;
    return ::setsockopt(it->second, SOL_SOCKET, SO_REUSEADDR,
                        reinterpret_cast<const char*>(&value), sizeof(value)) == 0 ? 1 : 0;
}

std::int32_t socketSetExclusive(void*, std::uint64_t handle, std::int32_t enabled) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end()) return 0;
    BOOL value = enabled != 0;
    return ::setsockopt(it->second, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                        reinterpret_cast<const char*>(&value), sizeof(value)) == 0 ? 1 : 0;
}

std::int32_t socketSetRecvTimeout(void*, std::uint64_t handle,
                                  std::uint32_t milliseconds) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end()) return 0;
    DWORD value = milliseconds;
    return ::setsockopt(it->second, SOL_SOCKET, SO_RCVTIMEO,
                        reinterpret_cast<const char*>(&value), sizeof(value)) == 0 ? 1 : 0;
}

std::int32_t socketSendTo(void*, std::uint64_t handle, const void* data,
                          std::uint32_t size, const void* address,
                          std::uint32_t addressSize) {
    auto it = gSockets.find(handle);
    if (it == gSockets.end() || !data || addressSize < sizeof(sockaddr_in)) return 0;
    return ::sendto(it->second, static_cast<const char*>(data), static_cast<int>(size),
                    0, static_cast<const sockaddr*>(address),
                    static_cast<int>(addressSize)) != SOCKET_ERROR ? 1 : 0;
}

std::int32_t socketRecvFrom(void*, std::uint64_t handle, void* data,
                            std::uint32_t capacity, void* address,
                            std::uint32_t addressSize, std::uint32_t* received,
                            std::int32_t* wouldBlock) {
    if (received) *received = 0;
    if (wouldBlock) *wouldBlock = 0;
    auto it = gSockets.find(handle);
    if (it == gSockets.end() || !data) return 0;
    int length = static_cast<int>(addressSize);
    const int result = ::recvfrom(it->second, static_cast<char*>(data),
                                  static_cast<int>(capacity), 0,
                                  address ? static_cast<sockaddr*>(address) : nullptr,
                                  address ? &length : nullptr);
    if (result == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            if (wouldBlock) *wouldBlock = 1;
            return 1;
        }
        return 0;
    }
    if (received) *received = static_cast<std::uint32_t>(result);
    return 1;
}

std::uint32_t glGenTexture(void*) { GLuint id = 0; glGenTextures(1, &id); return id; }
void glDeleteTexture(void*, std::uint32_t id) { GLuint value = id; glDeleteTextures(1, &value); }
std::uint32_t glGenBuffer(void*) { GLuint id = 0; glGenBuffers(1, &id); return id; }
void glDeleteBuffer(void*, std::uint32_t id) { GLuint value = id; glDeleteBuffers(1, &value); }
std::uint32_t glGenVertexArray(void*) { GLuint id = 0; glGenVertexArrays(1, &id); return id; }
void glDeleteVertexArray(void*, std::uint32_t id) { GLuint value = id; glDeleteVertexArrays(1, &value); }
std::uint32_t glGenFramebuffer(void*) { GLuint id = 0; glGenFramebuffers(1, &id); return id; }
void glDeleteFramebuffer(void*, std::uint32_t id) { GLuint value = id; glDeleteFramebuffers(1, &value); }
std::uint32_t glGenRenderbuffer(void*) { GLuint id = 0; glGenRenderbuffers(1, &id); return id; }
void glDeleteRenderbuffer(void*, std::uint32_t id) { GLuint value = id; glDeleteRenderbuffers(1, &value); }
std::uint32_t glGenQuery(void*) { GLuint id = 0; glGenQueries(1, &id); return id; }
void glDeleteQuery(void*, std::uint32_t id) { GLuint value = id; glDeleteQueries(1, &value); }
void glDeleteShaderRes(void*, std::uint32_t id) { glDeleteShader(id); }
void glDeleteProgramRes(void*, std::uint32_t id) { glDeleteProgram(id); }
void glGenerateMipmapRes(void*, std::uint32_t target) { glGenerateMipmap(target); }

namespace {

GLenum toGl(mimita::render::Primitive value) {
    switch (value) {
        case mimita::render::Primitive::Points: return GL_POINTS;
        case mimita::render::Primitive::Lines: return GL_LINES;
        case mimita::render::Primitive::LineStrip: return GL_LINE_STRIP;
        case mimita::render::Primitive::Triangles: return GL_TRIANGLES;
        case mimita::render::Primitive::TriangleStrip: return GL_TRIANGLE_STRIP;
        case mimita::render::Primitive::TriangleFan: return GL_TRIANGLE_FAN;
    }
    return GL_TRIANGLES;
}

GLenum toGl(mimita::render::TextureTarget value) {
    switch (value) {
        case mimita::render::TextureTarget::Texture2D: return GL_TEXTURE_2D;
        case mimita::render::TextureTarget::TextureCube: return GL_TEXTURE_CUBE_MAP;
        case mimita::render::TextureTarget::CubePositiveX: return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        case mimita::render::TextureTarget::CubeNegativeX: return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        case mimita::render::TextureTarget::CubePositiveY: return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        case mimita::render::TextureTarget::CubeNegativeY: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        case mimita::render::TextureTarget::CubePositiveZ: return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        case mimita::render::TextureTarget::CubeNegativeZ: return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    }
    return GL_TEXTURE_2D;
}

GLenum toGl(mimita::render::BufferTarget value) {
    switch (value) {
        case mimita::render::BufferTarget::Array: return GL_ARRAY_BUFFER;
        case mimita::render::BufferTarget::ElementArray: return GL_ELEMENT_ARRAY_BUFFER;
    }
    return GL_ARRAY_BUFFER;
}

GLenum toGl(mimita::render::BufferUsage value) {
    switch (value) {
        case mimita::render::BufferUsage::Static: return GL_STATIC_DRAW;
        case mimita::render::BufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
        case mimita::render::BufferUsage::Stream: return GL_STREAM_DRAW;
    }
    return GL_STATIC_DRAW;
}

GLenum toGl(mimita::render::Capability value) {
    switch (value) {
        case mimita::render::Capability::DepthTest: return GL_DEPTH_TEST;
        case mimita::render::Capability::Blend: return GL_BLEND;
        case mimita::render::Capability::CullFace: return GL_CULL_FACE;
        case mimita::render::Capability::ScissorTest: return GL_SCISSOR_TEST;
        case mimita::render::Capability::StencilTest: return GL_STENCIL_TEST;
        case mimita::render::Capability::PolygonOffsetFill: return GL_POLYGON_OFFSET_FILL;
        case mimita::render::Capability::Multisample: return GL_MULTISAMPLE;
    }
    return GL_DEPTH_TEST;
}

GLenum toGl(mimita::render::BlendFactor value) {
    switch (value) {
        case mimita::render::BlendFactor::Zero: return GL_ZERO;
        case mimita::render::BlendFactor::One: return GL_ONE;
        case mimita::render::BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
        case mimita::render::BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case mimita::render::BlendFactor::DstAlpha: return GL_DST_ALPHA;
        case mimita::render::BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        case mimita::render::BlendFactor::SrcColor: return GL_SRC_COLOR;
        case mimita::render::BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case mimita::render::BlendFactor::DstColor: return GL_DST_COLOR;
        case mimita::render::BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
    }
    return GL_ONE;
}

GLenum toGl(mimita::render::DepthFunc value) {
    switch (value) {
        case mimita::render::DepthFunc::Never: return GL_NEVER;
        case mimita::render::DepthFunc::Less: return GL_LESS;
        case mimita::render::DepthFunc::Equal: return GL_EQUAL;
        case mimita::render::DepthFunc::Lequal: return GL_LEQUAL;
        case mimita::render::DepthFunc::Greater: return GL_GREATER;
        case mimita::render::DepthFunc::Notequal: return GL_NOTEQUAL;
        case mimita::render::DepthFunc::Gequal: return GL_GEQUAL;
        case mimita::render::DepthFunc::Always: return GL_ALWAYS;
    }
    return GL_LESS;
}

GLenum toGl(mimita::render::CullFaceMode value) {
    switch (value) {
        case mimita::render::CullFaceMode::Front: return GL_FRONT;
        case mimita::render::CullFaceMode::Back: return GL_BACK;
        case mimita::render::CullFaceMode::FrontAndBack: return GL_FRONT_AND_BACK;
    }
    return GL_BACK;
}

GLenum toGl(mimita::render::PolygonMode value) {
    switch (value) {
        case mimita::render::PolygonMode::Point: return GL_POINT;
        case mimita::render::PolygonMode::Line: return GL_LINE;
        case mimita::render::PolygonMode::Fill: return GL_FILL;
    }
    return GL_FILL;
}

GLenum toGl(mimita::render::IndexType value) {
    switch (value) {
        case mimita::render::IndexType::UInt16: return GL_UNSIGNED_SHORT;
        case mimita::render::IndexType::UInt32: return GL_UNSIGNED_INT;
    }
    return GL_UNSIGNED_INT;
}

GLenum toGl(mimita::render::TextureFormat value) {
    switch (value) {
        case mimita::render::TextureFormat::RGBA: return GL_RGBA;
        case mimita::render::TextureFormat::RGB: return GL_RGB;
        case mimita::render::TextureFormat::RGBA8: return GL_RGBA8;
        case mimita::render::TextureFormat::Depth: return GL_DEPTH_COMPONENT;
        case mimita::render::TextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
    }
    return GL_RGBA;
}

GLenum toGl(mimita::render::PixelFormat value) {
    switch (value) {
        case mimita::render::PixelFormat::RGBA: return GL_RGBA;
        case mimita::render::PixelFormat::RGB: return GL_RGB;
        case mimita::render::PixelFormat::RG: return GL_RG;
        case mimita::render::PixelFormat::Red: return GL_RED;
        case mimita::render::PixelFormat::Depth: return GL_DEPTH_COMPONENT;
    }
    return GL_RGBA;
}

GLenum toGl(mimita::render::PixelType value) {
    switch (value) {
        case mimita::render::PixelType::UnsignedByte: return GL_UNSIGNED_BYTE;
        case mimita::render::PixelType::Float: return GL_FLOAT;
    }
    return GL_UNSIGNED_BYTE;
}

GLenum toGl(mimita::render::TextureParameter value) {
    switch (value) {
        case mimita::render::TextureParameter::MinFilter: return GL_TEXTURE_MIN_FILTER;
        case mimita::render::TextureParameter::MagFilter: return GL_TEXTURE_MAG_FILTER;
        case mimita::render::TextureParameter::WrapS: return GL_TEXTURE_WRAP_S;
        case mimita::render::TextureParameter::WrapT: return GL_TEXTURE_WRAP_T;
        case mimita::render::TextureParameter::WrapR: return GL_TEXTURE_WRAP_R;
        case mimita::render::TextureParameter::MaxLevel: return GL_TEXTURE_MAX_LEVEL;
        case mimita::render::TextureParameter::BorderColor: return GL_TEXTURE_BORDER_COLOR;
    }
    return GL_TEXTURE_MIN_FILTER;
}

GLenum toGl(mimita::render::TextureValue value) {
    switch (value) {
        case mimita::render::TextureValue::Nearest: return GL_NEAREST;
        case mimita::render::TextureValue::Linear: return GL_LINEAR;
        case mimita::render::TextureValue::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
        case mimita::render::TextureValue::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case mimita::render::TextureValue::ClampToBorder: return GL_CLAMP_TO_BORDER;
        case mimita::render::TextureValue::Repeat: return GL_REPEAT;
        case mimita::render::TextureValue::MirroredRepeat: return GL_MIRRORED_REPEAT;
    }
    return GL_LINEAR;
}

GLenum toGl(mimita::render::VertexType value) {
    switch (value) {
        case mimita::render::VertexType::Float: return GL_FLOAT;
        case mimita::render::VertexType::UInt32: return GL_UNSIGNED_INT;
    }
    return GL_FLOAT;
}

GLenum toGl(mimita::render::ShaderStage value) {
    switch (value) {
        case mimita::render::ShaderStage::Vertex: return GL_VERTEX_SHADER;
        case mimita::render::ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
    }
    return GL_VERTEX_SHADER;
}

GLenum toGl(mimita::render::ShaderParameter value) {
    switch (value) {
        case mimita::render::ShaderParameter::CompileStatus: return GL_COMPILE_STATUS;
        case mimita::render::ShaderParameter::LinkStatus: return GL_LINK_STATUS;
        case mimita::render::ShaderParameter::InfoLogLength: return GL_INFO_LOG_LENGTH;
    }
    return GL_COMPILE_STATUS;
}

GLenum toGl(mimita::render::FramebufferTarget value) {
    switch (value) {
        case mimita::render::FramebufferTarget::Framebuffer: return GL_FRAMEBUFFER;
        case mimita::render::FramebufferTarget::Read: return GL_READ_FRAMEBUFFER;
        case mimita::render::FramebufferTarget::Draw: return GL_DRAW_FRAMEBUFFER;
    }
    return GL_FRAMEBUFFER;
}

GLenum toGl(mimita::render::FramebufferAttachment value) {
    switch (value) {
        case mimita::render::FramebufferAttachment::Color0: return GL_COLOR_ATTACHMENT0;
        case mimita::render::FramebufferAttachment::Color1: return GL_COLOR_ATTACHMENT1;
        case mimita::render::FramebufferAttachment::Color2: return GL_COLOR_ATTACHMENT2;
        case mimita::render::FramebufferAttachment::Color3: return GL_COLOR_ATTACHMENT3;
        case mimita::render::FramebufferAttachment::Depth: return GL_DEPTH_ATTACHMENT;
        case mimita::render::FramebufferAttachment::Stencil: return GL_STENCIL_ATTACHMENT;
        case mimita::render::FramebufferAttachment::DepthStencil: return GL_DEPTH_STENCIL_ATTACHMENT;
    }
    return GL_COLOR_ATTACHMENT0;
}

GLenum toGl(mimita::render::RenderbufferFormat value) {
    switch (value) {
        case mimita::render::RenderbufferFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case mimita::render::RenderbufferFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
        case mimita::render::RenderbufferFormat::RGBA8: return GL_RGBA8;
    }
    return GL_DEPTH_COMPONENT24;
}

GLenum toGl(mimita::render::FramebufferBuffer value) {
    switch (value) {
        case mimita::render::FramebufferBuffer::None: return GL_NONE;
        case mimita::render::FramebufferBuffer::Color0: return GL_COLOR_ATTACHMENT0;
        case mimita::render::FramebufferBuffer::Color1: return GL_COLOR_ATTACHMENT1;
        case mimita::render::FramebufferBuffer::Color2: return GL_COLOR_ATTACHMENT2;
        case mimita::render::FramebufferBuffer::Color3: return GL_COLOR_ATTACHMENT3;
    }
    return GL_NONE;
}

GLenum toGl(mimita::render::PixelStoreParameter value) {
    switch (value) {
        case mimita::render::PixelStoreParameter::UnpackAlignment: return GL_UNPACK_ALIGNMENT;
        case mimita::render::PixelStoreParameter::UnpackRowLength: return GL_UNPACK_ROW_LENGTH;
    }
    return GL_UNPACK_ALIGNMENT;
}

struct HostGlState {
    GLboolean depthTest = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLboolean cullFace = GL_FALSE;
    GLboolean scissorTest = GL_FALSE;
    GLboolean depthMask = GL_TRUE;
    GLint depthFunc = GL_LESS;
    GLint cullFaceMode = GL_BACK;
    GLint blendSrcRgb = GL_ONE;
    GLint blendDstRgb = GL_ZERO;
    GLint blendSrcAlpha = GL_ONE;
    GLint blendDstAlpha = GL_ZERO;
    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    GLint viewport[4] = {0, 0, 0, 0};
    GLint activeTexture = GL_TEXTURE0;
    GLint program = 0;
    GLint vertexArray = 0;
    GLint arrayBuffer = 0;
    GLint elementBuffer = 0;
    GLint texture2D = 0;
    GLint framebuffer = 0;
};

std::vector<HostGlState> gHostGlStateStack;

HostGlState captureHostGlState() {
    HostGlState state;
    state.depthTest = glIsEnabled(GL_DEPTH_TEST);
    state.blend = glIsEnabled(GL_BLEND);
    state.cullFace = glIsEnabled(GL_CULL_FACE);
    state.scissorTest = glIsEnabled(GL_SCISSOR_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &state.depthMask);
    glGetIntegerv(GL_DEPTH_FUNC, &state.depthFunc);
    glGetIntegerv(GL_CULL_FACE_MODE, &state.cullFaceMode);
    glGetIntegerv(GL_BLEND_SRC_RGB, &state.blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &state.blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &state.blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &state.blendDstAlpha);
    glGetIntegerv(GL_POLYGON_MODE, state.polygonMode);
    glGetIntegerv(GL_VIEWPORT, state.viewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &state.activeTexture);
    glGetIntegerv(GL_CURRENT_PROGRAM, &state.program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertexArray);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.arrayBuffer);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &state.elementBuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &state.texture2D);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state.framebuffer);
    return state;
}

void restoreHostGlState(const HostGlState& state) {
    glUseProgram(static_cast<GLuint>(state.program));
    glBindVertexArray(static_cast<GLuint>(state.vertexArray));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state.arrayBuffer));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(state.elementBuffer));
    glActiveTexture(static_cast<GLenum>(state.activeTexture));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(state.texture2D));
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(state.framebuffer));
    glDepthMask(state.depthMask);
    glDepthFunc(static_cast<GLenum>(state.depthFunc));
    glCullFace(static_cast<GLenum>(state.cullFaceMode));
    glBlendFuncSeparate(static_cast<GLenum>(state.blendSrcRgb),
                        static_cast<GLenum>(state.blendDstRgb),
                        static_cast<GLenum>(state.blendSrcAlpha),
                        static_cast<GLenum>(state.blendDstAlpha));
    glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(state.polygonMode[0]));
    glViewport(state.viewport[0], state.viewport[1], state.viewport[2],
               state.viewport[3]);
    if (state.depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (state.blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (state.cullFace) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (state.scissorTest) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
}

}  // namespace

HostResult renderBufferData(void*, mimita::render::BufferTarget target,
                            std::uint64_t buffer, const void* data,
                            std::uint32_t size, mimita::render::BufferUsage usage) {
    glBindBuffer(toGl(target), static_cast<GLuint>(buffer));
    glBufferData(toGl(target), static_cast<GLsizeiptr>(size), data, toGl(usage));
    return HostResult::Ok;
}

HostResult renderBufferSubData(void*, mimita::render::BufferTarget target,
                               std::uint64_t buffer, std::uint32_t offset,
                               const void* data, std::uint32_t size) {
    glBindBuffer(toGl(target), static_cast<GLuint>(buffer));
    glBufferSubData(toGl(target), static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size), data);
    return HostResult::Ok;
}

HostResult renderTexImage2D(void*, mimita::render::TextureTarget target,
                            std::int32_t level,
                            mimita::render::TextureFormat format,
                            std::int32_t width, std::int32_t height,
                            std::int32_t border,
                            mimita::render::PixelFormat pixelFormat,
                            mimita::render::PixelType pixelType,
                            const void* data, std::uint32_t) {
    glTexImage2D(toGl(target), level, static_cast<GLint>(toGl(format)), width,
                 height, border, toGl(pixelFormat), toGl(pixelType), data);
    return HostResult::Ok;
}

HostResult renderTexParameteri(void*, mimita::render::TextureTarget target,
                               mimita::render::TextureParameter parameter,
                               std::int32_t value) {
    glTexParameteri(toGl(target), toGl(parameter), static_cast<GLint>(value));
    return HostResult::Ok;
}

HostResult renderVertexAttrib(void*, std::uint32_t index, std::int32_t size,
                              mimita::render::VertexType type,
                              std::uint32_t normalized, std::int32_t stride,
                              std::uint32_t offset) {
    glVertexAttribPointer(index, size, toGl(type), normalized != 0, stride,
                          reinterpret_cast<const void*>(
                              static_cast<std::uintptr_t>(offset)));
    return HostResult::Ok;
}

HostResult renderEnableAttrib(void*, std::uint32_t index, std::uint32_t enabled) {
    if (enabled) glEnableVertexAttribArray(index);
    else glDisableVertexAttribArray(index);
    return HostResult::Ok;
}

HostResult renderUniform1i(void*, std::int32_t location, std::int32_t value) {
    glUniform1i(location, value);
    return HostResult::Ok;
}
HostResult renderUniform1f(void*, std::int32_t location, float value) {
    glUniform1f(location, value);
    return HostResult::Ok;
}
HostResult renderUniform2f(void*, std::int32_t location, float x, float y) {
    glUniform2f(location, x, y);
    return HostResult::Ok;
}
HostResult renderUniform3f(void*, std::int32_t location, float x, float y, float z) {
    glUniform3f(location, x, y, z);
    return HostResult::Ok;
}
HostResult renderUniform4f(void*, std::int32_t location, float x, float y, float z, float w) {
    glUniform4f(location, x, y, z, w);
    return HostResult::Ok;
}
HostResult renderUniform1fv(void*, std::int32_t location, std::int32_t count,
                            const float* values) {
    glUniform1fv(location, static_cast<GLsizei>(count), values);
    return HostResult::Ok;
}
HostResult renderUniform2fv(void*, std::int32_t location, std::int32_t count,
                            const float* values) {
    glUniform2fv(location, static_cast<GLsizei>(count), values);
    return HostResult::Ok;
}
HostResult renderUniform3fv(void*, std::int32_t location, std::int32_t count,
                            const float* values) {
    glUniform3fv(location, static_cast<GLsizei>(count), values);
    return HostResult::Ok;
}
HostResult renderUniformMatrix4(void*, std::int32_t location, std::uint32_t count,
                                const float* values) {
    if (!values) return HostResult::InvalidArgument;
    glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_FALSE, values);
    return HostResult::Ok;
}

std::uint32_t renderCreateShader(void*, mimita::render::ShaderStage stage) {
    return static_cast<std::uint32_t>(glCreateShader(toGl(stage)));
}

HostResult renderShaderSource(void*, std::uint32_t shader, const char* source) {
    if (!source) return HostResult::InvalidArgument;
    const GLchar* sources[1] = {source};
    glShaderSource(static_cast<GLuint>(shader), 1, sources, nullptr);
    return HostResult::Ok;
}

HostResult renderCompileShader(void*, std::uint32_t shader) {
    glCompileShader(static_cast<GLuint>(shader));
    return HostResult::Ok;
}

std::uint32_t renderCreateProgram(void*) {
    return static_cast<std::uint32_t>(glCreateProgram());
}

HostResult renderAttachShader(void*, std::uint32_t program, std::uint32_t shader) {
    glAttachShader(static_cast<GLuint>(program), static_cast<GLuint>(shader));
    return HostResult::Ok;
}

HostResult renderLinkProgram(void*, std::uint32_t program) {
    glLinkProgram(static_cast<GLuint>(program));
    return HostResult::Ok;
}

std::int32_t renderGetUniformLocation(void*, std::uint32_t program,
                                      const char* name) {
    if (!name) return -1;
    return static_cast<std::int32_t>(
        glGetUniformLocation(static_cast<GLuint>(program), name));
}

HostResult renderGetUniformiv(void*, std::uint32_t program,
                              std::int32_t location, std::int32_t* values,
                              std::int32_t count) {
    if (!values || count <= 0) return HostResult::InvalidArgument;
    glGetUniformiv(static_cast<GLuint>(program), location,
                   reinterpret_cast<GLint*>(values));
    return HostResult::Ok;
}

HostResult renderGetProgramiv(void*, std::uint32_t program,
                              mimita::render::ShaderParameter parameter,
                              std::int32_t* value) {
    if (!value) return HostResult::InvalidArgument;
    glGetProgramiv(static_cast<GLuint>(program), toGl(parameter),
                   reinterpret_cast<GLint*>(value));
    return HostResult::Ok;
}

HostResult renderGetShaderiv(void*, std::uint32_t shader,
                             mimita::render::ShaderParameter parameter,
                             std::int32_t* value) {
    if (!value) return HostResult::InvalidArgument;
    glGetShaderiv(static_cast<GLuint>(shader), toGl(parameter),
                  reinterpret_cast<GLint*>(value));
    return HostResult::Ok;
}

HostResult renderGetProgramInfoLog(void*, std::uint32_t program,
                                   std::int32_t capacity, std::int32_t* length,
                                   char* log) {
    if (!log || capacity <= 0) return HostResult::InvalidArgument;
    glGetProgramInfoLog(static_cast<GLuint>(program),
                        static_cast<GLsizei>(capacity),
                        reinterpret_cast<GLsizei*>(length), log);
    return HostResult::Ok;
}

HostResult renderGetShaderInfoLog(void*, std::uint32_t shader,
                                  std::int32_t capacity, std::int32_t* length,
                                  char* log) {
    if (!log || capacity <= 0) return HostResult::InvalidArgument;
    glGetShaderInfoLog(static_cast<GLuint>(shader),
                       static_cast<GLsizei>(capacity),
                       reinterpret_cast<GLsizei*>(length), log);
    return HostResult::Ok;
}

std::uint32_t renderGetError(void*) {
    return static_cast<std::uint32_t>(glGetError());
}

HostResult renderFramebufferTexture2D(
    void*, mimita::render::TextureTarget target,
    mimita::render::FramebufferAttachment attachment, std::uint64_t texture) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, toGl(attachment), toGl(target),
                           static_cast<GLuint>(texture), 0);
    return HostResult::Ok;
}

HostResult renderBindRenderbuffer(void*, std::uint32_t renderbuffer) {
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer));
    return HostResult::Ok;
}

HostResult renderRenderbufferStorage(void*,
                                     mimita::render::RenderbufferFormat format,
                                     std::int32_t width, std::int32_t height) {
    glRenderbufferStorage(GL_RENDERBUFFER, toGl(format), width, height);
    return HostResult::Ok;
}

HostResult renderFramebufferRenderbuffer(
    void*, mimita::render::FramebufferAttachment attachment,
    std::uint32_t renderbuffer) {
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, toGl(attachment), GL_RENDERBUFFER,
                              static_cast<GLuint>(renderbuffer));
    return HostResult::Ok;
}

std::int32_t renderCheckFramebufferStatus(void*) {
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
               ? 1
               : 0;
}

LogCapabilityV1 gLogCapability{
    kLogCapabilityVersion,
    static_cast<std::uint32_t>(sizeof(LogCapabilityV1)),
    &hostLogText,
    &hostLogEvent,
    &hostShouldLog,
};


std::chrono::steady_clock::time_point gStartTime = std::chrono::steady_clock::now();

void emitProbeEvent(const char* eventName, const char* reason, std::uint32_t tick,
                    const nlohmann::json& fields) {
    if (!StructuredLogger::instance().shouldLog(StructuredCategory::Executable,
                                                StructuredLevel::Important)) {
        return;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Executable, StructuredLevel::Important, eventName, "", reason,
        tick, fields, __FILE__, __LINE__, __FUNCTION__);
}

}  // namespace

HostRuntime& HostRuntime::instance() {
    static HostRuntime runtime;
    return runtime;
}

HostRuntime::HostRuntime() : barrier_(hostBarrier()) {}

void* HostRuntime::ensureStateObject(std::uint32_t id, std::uint32_t size,
                                     std::uint32_t alignment, std::int32_t* created) {
    if (created) *created = 0;
    if (size == 0 || alignment == 0 ||
        (alignment & (alignment - 1)) != 0) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(stateObjectMutex_);
    StateObjectRecord& record = stateObjects_[id];
    if (record.storage) {
        if (record.size != size || record.alignment != alignment) return nullptr;
        return record.storage;
    }

    record.storage = _aligned_malloc(size, alignment);
    if (!record.storage) return nullptr;
    std::memset(record.storage, 0, size);
    record.size = size;
    record.alignment = alignment;
    if (created) *created = 1;
    return record.storage;
}

bool HostRuntime::startFromEnvironment() {
    if (started_) {
        return probeEnabled_.load(std::memory_order_acquire);
    }

    const char* enabled = std::getenv("MIMITA_GAME_PROBE");
    if (!enabled || std::strcmp(enabled, "1") != 0) {
        return false;
    }

    const char* readyPath = std::getenv("MIMITA_GAME_READY");
    readyPath_ = (readyPath && *readyPath) ? readyPath : "build/game/ready.json";

    const char* dllPath = std::getenv("MIMITA_GAME_DLL");
    if (!dllPath || !*dllPath) {
        emitProbeEvent("game.host.probe.disabled", "missing MIMITA_GAME_DLL", 0,
                       nlohmann::json{{"env", "MIMITA_GAME_DLL"}});
        return false;
    }

    services_ = HostServicesV1{};
    services_.version = kHostServicesVersion;
    services_.structSize = static_cast<std::uint32_t>(sizeof(HostServicesV1));
    services_.hostUserData = this;
    services_.log = &HostRuntime::logThunk;
    services_.entity_read = &HostRuntime::entityReadThunk;
    services_.entity_write = &HostRuntime::entityWriteThunk;
    services_.tick = &HostRuntime::tickThunk;
    services_.time_seconds = &HostRuntime::timeThunk;
    services_.reserve = &HostRuntime::reserveThunk;
    services_.release = &HostRuntime::releaseThunk;
    services_.get_capability = &HostRuntime::getCapabilityThunk;

    const std::wstring widePath = widenUtf8(dllPath);
    if (widePath.empty()) {
        emitProbeEvent("game.host.probe.failed", "invalid dll path", 0,
                       nlohmann::json{{"path", dllPath}});
        return false;
    }

    printf("[HOST] candidate load begin\n");
    LoadedGeneration candidate;
    const HostResult loaded = loader_.loadCandidate(widePath, &services_, &candidate);
    if (loaded != HostResult::Ok) {
        emitProbeEvent("game.host.probe.failed", "candidate rejected", 0,
                       nlohmann::json{{"path", dllPath},
                                      {"result", static_cast<int>(loaded)}});
        return false;
    }

    const std::uint64_t generation = candidate.generation != 0 ? candidate.generation : 1;
    barrier_.beginGeneration(generation);
    loader_.commit(std::move(candidate));
    probeEnabled_.store(true, std::memory_order_release);
    started_ = true;

    emitProbeEvent("game.host.probe.started", "generation loaded", 0,
                   nlohmann::json{{"generation", generation},
                                  {"dll", dllPath},
                                  {"abi", kGameHostAbiVersion}});
    return true;
}

void HostRuntime::shutdown() {
    if (!started_) {
        return;
    }
    probeEnabled_.store(false, std::memory_order_release);
    barrier_.beginQuiesce(QuiesceMode::Shutdown);
    barrier_.finishQuiesce(2000);
    loader_.unloadActive();
    barrier_.shutdown();
    clearEntityAdapter();
    clearWorldAdapter();
    clearTransportAdapter();
    renderPending_.store(0, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        for (auto& entry : resources_) {
            std::free(entry.second);
        }
        resources_.clear();
    }
    started_ = false;
    emitProbeEvent("game.host.probe.stopped", "generation unloaded", 0,
                   nlohmann::json::object());
}

bool HostRuntime::ownsGameFrame() const {
    if (!started_) return false;
    const char* enabled = std::getenv("MIMITA_GAME_OWN_FRAME");
    if (!enabled || std::strcmp(enabled, "1") != 0) return false;
    const GameModuleV1* api = loader_.active();
    return api && api->frameCapable != 0 && api->tick && api->render;
}

bool HostRuntime::bootstrapGame(const GameStartupV1& startup) {
    const GameModuleV1* api = loader_.active();
    if (!api || !api->bootstrap) return false;
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) return false;
    const HostResult result = api->bootstrap(&startup);
    barrier_.leave();
    if (result != HostResult::Ok) {
        emitProbeEvent("game.host.bootstrap-failed", "game bootstrap returned failure", 0,
                       nlohmann::json{{"generation", generation},
                                      {"result", static_cast<int>(result)}});
        return false;
    }
    return true;
}

bool HostRuntime::gameShouldClose() const {
    const GameModuleV1* api = loader_.active();
    return !api || !api->should_close || api->should_close() != 0;
}

HostResult HostRuntime::driveTick(double deltaSeconds) {
    const GameModuleV1* api = loader_.active();
    if (!api || !api->tick) return HostResult::NotFound;
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) return HostResult::Busy;
    HostResult result = HostResult::InternalError;
    try {
        result = api->tick(deltaSeconds);
    } catch (...) {
        result = HostResult::InternalError;
    }
    barrier_.leave();
    if (result != HostResult::Ok) {
        emitProbeEvent("game.frame.tick-failed", "game tick returned failure", 0,
                       nlohmann::json{{"generation", generation},
                                      {"result", static_cast<int>(result)}});
        return result;
    }
    ++tick_;
    return HostResult::Ok;
}

HostResult HostRuntime::driveRender(void* renderContext) {
    const GameModuleV1* api = loader_.active();
    if (!api || !api->render) return HostResult::NotFound;
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) return HostResult::Busy;
    HostResult result = HostResult::InternalError;
    try {
        result = api->render(renderContext);
    } catch (...) {
        result = HostResult::InternalError;
    }
    barrier_.leave();
    if (result != HostResult::Ok) {
        emitProbeEvent("game.frame.render-failed", "game render returned failure", 0,
                       nlohmann::json{{"generation", generation},
                                      {"result", static_cast<int>(result)}});
    }
    return result;
}

void HostRuntime::installInputCallbacks(GLFWwindow* window) {
    if (!window) return;
    glfwSetKeyCallback(window, &HostRuntime::inputKeyCallback);
    glfwSetCharCallback(window, &HostRuntime::inputCharCallback);
    glfwSetScrollCallback(window, &HostRuntime::inputScrollCallback);
    glfwSetDropCallback(window, &HostRuntime::inputDropCallback);
}

void HostRuntime::enqueueInput(const GameInputEventV1& event) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    inputQueue_.push_back(event);
}

void HostRuntime::dispatchQueuedInput() {
    std::deque<GameInputEventV1> pending;
    {
        std::lock_guard<std::mutex> lock(inputMutex_);
        if (inputQueue_.empty()) return;
        pending.swap(inputQueue_);
    }
    if (!probeEnabled_.load(std::memory_order_acquire)) return;
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) return;
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) return;
    for (GameInputEventV1& event : pending) {
        std::int32_t handled = 0;
        api->on_event(kEventGameInput, &event, sizeof(event), &handled);
    }
    barrier_.leave();
}

void HostRuntime::inputKeyCallback(GLFWwindow*, int key, int scancode,
                                   int action, int mods) {
    GameInputEventV1 event{};
    event.version = 1;
    event.structSize = static_cast<std::uint32_t>(sizeof(event));
    event.kind = GameInputKind::Key;
    event.key = key;
    event.scancode = scancode;
    event.action = action;
    event.mods = mods;
    instance().enqueueInput(event);
}

void HostRuntime::inputCharCallback(GLFWwindow*, unsigned int codepoint) {
    GameInputEventV1 event{};
    event.version = 1;
    event.structSize = static_cast<std::uint32_t>(sizeof(event));
    event.kind = GameInputKind::Char;
    event.codepoint = codepoint;
    instance().enqueueInput(event);
}

void HostRuntime::inputScrollCallback(GLFWwindow*, double xOffset,
                                      double yOffset) {
    GameInputEventV1 event{};
    event.version = 1;
    event.structSize = static_cast<std::uint32_t>(sizeof(event));
    event.kind = GameInputKind::Scroll;
    event.scrollX = xOffset;
    event.scrollY = yOffset;
    instance().enqueueInput(event);
}

void HostRuntime::inputDropCallback(GLFWwindow*, int count, const char** paths) {
    GameInputEventV1 event{};
    event.version = 1;
    event.structSize = static_cast<std::uint32_t>(sizeof(event));
    event.kind = GameInputKind::Drop;
    const int accepted = count < 8 ? (count > 0 ? count : 0) : 8;
    event.pathCount = static_cast<std::uint32_t>(accepted);
    for (int i = 0; i < accepted; ++i) {
        if (paths && paths[i]) {
            std::strncpy(event.paths[i], paths[i], sizeof(event.paths[i]) - 1);
            event.paths[i][sizeof(event.paths[i]) - 1] = '\0';
        }
    }
    instance().enqueueInput(event);
}

HostResult HostRuntime::driveFrame(double deltaSeconds, void* renderContext) {
    dispatchQueuedInput();
    const HostResult tickResult = driveTick(deltaSeconds);
    if (tickResult != HostResult::Ok) return tickResult;
    return driveRender(renderContext);
}

bool HostRuntime::deliverInbound(const TransportMessageV1& message) {
    if (!probeEnabled_.load(std::memory_order_acquire) || !message.payload ||
        message.payloadSize == 0) {
        return false;
    }
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) {
        return false;
    }

    std::lock_guard<std::mutex> lock(probeMutex_);
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) {
        return false;
    }
    TransportMessageV1 mutableMessage = message;
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventTransportMessage, &mutableMessage, sizeof(TransportMessageV1),
        &handled);
    barrier_.leave();
    return result == HostResult::Ok && handled != 0;
}

bool HostRuntime::dispatchInbound(TransportDispatchV1* dispatch) {
    if (!probeEnabled_.load(std::memory_order_acquire) || !dispatch ||
        !dispatch->message.payload || dispatch->message.payloadSize == 0) {
        return false;
    }
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) {
        return false;
    }

    std::lock_guard<std::mutex> lock(probeMutex_);
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) {
        return false;
    }
    dispatch->response.structSize =
        static_cast<std::uint32_t>(sizeof(TransportResponseV1));
    dispatch->response.version = kTransportCapabilityVersion;
    dispatch->response.typeId = 0;
    dispatch->response.flags = 0;
    dispatch->response.payloadSize = 0;
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventTransportDispatch, dispatch, sizeof(TransportDispatchV1), &handled);
    barrier_.leave();
    if (result != HostResult::Ok || handled == 0) {
        return false;
    }
    if (dispatch->response.payloadSize > kTransportMaxResponseBytes) {
        dispatch->response.payloadSize = 0;
        return false;
    }
    return dispatch->response.payloadSize != 0;
}

void HostRuntime::poll() {
    if (!probeEnabled_.load(std::memory_order_acquire)) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - lastPoll_ < std::chrono::milliseconds(250)) {
        return;
    }
    lastPoll_ = now;

    std::lock_guard<std::mutex> swapLock(swapMutex_);
    std::ifstream input(readyPath_);
    if (!input.is_open()) {
        return;
    }
    nlohmann::json ready;
    try {
        input >> ready;
    } catch (const std::exception& error) {
        emitProbeEvent("game.generation.ready-invalid", error.what(), 0,
                       nlohmann::json{{"path", readyPath_}});
        return;
    }
    if (ready.value("status", std::string()) != "ready") {
        return;
    }
    printf("[HOST] ready candidate gen=%llu\n",
           static_cast<unsigned long long>(ready.value("generation", std::uint64_t{0})));
    const std::uint64_t generation = ready.value("generation", std::uint64_t{0});
    const std::string dll = ready.value("dll", std::string());
    if (generation == 0 || dll.empty() ||
        generation == loader_.activeGeneration()) {
        return;
    }
    if (generation == lastFailedGeneration_ && dll == lastFailedDll_ &&
        now - lastFailedAt_ < std::chrono::seconds(5)) {
        return;
    }
    if (!std::filesystem::exists(dll)) {
        printf("[HOST] candidate missing dll=%s\n", dll.c_str());
        lastFailedGeneration_ = generation;
        lastFailedDll_ = dll;
        lastFailedAt_ = now;
        emitProbeEvent("game.generation.activate-failed", "candidate missing", 0,
                       nlohmann::json{{"generation", generation}, {"dll", dll}});
        return;
    }

    const std::uint64_t oldGeneration = loader_.activeGeneration();
    printf("[HOST] swap quiesce begin\n");
    barrier_.beginQuiesce(QuiesceMode::Swap);
    const HostResult quiesced = barrier_.finishQuiesce(2000);
    printf("[HOST] swap quiesce result=%d\n", static_cast<int>(quiesced));
    if (quiesced != HostResult::Ok) {
        printf("[HOST] swap refused busy result=%d\n", static_cast<int>(quiesced));
        emitProbeEvent("game.generation.swap-refused", "generation busy", 0,
                       nlohmann::json{{"generation", generation},
                                      {"result", static_cast<int>(quiesced)}});
        barrier_.resumeGeneration();
        return;
    }

    printf("[HOST] candidate load begin\n");
    LoadedGeneration candidate;
    const HostResult loaded = loader_.loadCandidate(
        widenUtf8(dll.c_str()), &services_, &candidate);
    printf("[HOST] candidate load result=%d\n", static_cast<int>(loaded));
    if (loaded != HostResult::Ok) {
        printf("[HOST] candidate rejected result=%d dll=%s\n",
               static_cast<int>(loaded), dll.c_str());
        lastFailedGeneration_ = generation;
        lastFailedDll_ = dll;
        lastFailedAt_ = now;
        emitProbeEvent("game.generation.activate-failed", "candidate rejected", 0,
                       nlohmann::json{{"generation", generation},
                                      {"dll", dll},
                                      {"result", static_cast<int>(loaded)}});
        barrier_.resumeGeneration();
        return;
    }

    const GameModuleV1* oldApi = loader_.active();
    if (oldApi && oldApi->quiesce) {
        printf("[HOST] old quiesce begin\n");
        oldApi->quiesce(QuiesceMode::Swap);
        printf("[HOST] old quiesce end\n");
    }
    if (oldApi && oldApi->shutdown) {
        printf("[HOST] old shutdown begin\n");
        oldApi->shutdown();
        printf("[HOST] old shutdown end\n");
    }
    // The ready manifest is the generation identity for the swap; a module may
    // hardcode a local version field that is not the build generation.
    candidate.generation = generation;
    printf("[HOST] commit begin\n");
    loader_.commit(std::move(candidate));
    printf("[HOST] commit end\n");
    lastReadyGeneration_ = generation;
    lastFailedGeneration_ = 0;
    lastFailedDll_.clear();
    barrier_.beginGeneration(generation);
    printf("[HOST] generation activated old=%llu new=%llu\n",
           static_cast<unsigned long long>(oldGeneration),
           static_cast<unsigned long long>(generation));
    emitProbeEvent("game.generation.activated", "candidate committed", 0,
                   nlohmann::json{{"old_generation", oldGeneration},
                                  {"generation", generation},
                                  {"dll", dll}});
}

void HostRuntime::setEntityAdapter(const EntityAdapterV1* adapter, void* owner) {
    entityAdapter_ = adapter;
    entityOwner_ = owner;
}

void HostRuntime::clearEntityAdapter() {
    entityAdapter_ = nullptr;
    entityOwner_ = nullptr;
}

void HostRuntime::setWorldAdapter(const WorldAdapterV1* adapter, void* owner) {
    worldAdapter_ = adapter;
    worldOwner_ = owner;
}

void HostRuntime::clearWorldAdapter() {
    worldAdapter_ = nullptr;
    worldOwner_ = nullptr;
}

bool HostRuntime::probeNpcTargetSelect(NpcTargetDecisionV1* decision) {
    if (!decision || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) {
        return false;
    }

    std::lock_guard<std::mutex> lock(probeMutex_);
    tick_ = decision->tick;
    decision->outTargetId = 0;
    decision->outHandled = 0;

    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) {
        return false;
    }
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventNpcTargetSelect, decision, sizeof(NpcTargetDecisionV1), &handled);
    barrier_.leave();

    if (result != HostResult::Ok || handled == 0 || decision->outHandled == 0) {
        return false;
    }
    if (decision->outTargetId == 0) {
        return true;
    }
    const std::uint32_t count = decision->candidateCount < kNpcTargetMaxCandidates
        ? decision->candidateCount : kNpcTargetMaxCandidates;
    for (std::uint32_t index = 0; index < count; ++index) {
        const NpcTargetCandidateV1& candidate = decision->candidates[index];
        if (candidate.id == decision->outTargetId) {
            return candidate.dead == 0;
        }
    }
    return false;
}

void* HostRuntime::getCapabilityThunk(void*, std::uint32_t capability,
                                            std::uint32_t version) {
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Log) &&
        version == kLogCapabilityVersion) {
        return &gLogCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Transport) &&
        version == kTransportCapabilityVersion) {
        static TransportCapabilityV1 transportCapability{
            kTransportCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(TransportCapabilityV1)),
            &HostRuntime::transportSendThunk,
            &HostRuntime::transportBroadcastThunk,
            &HostRuntime::transportSendToPlayerThunk,
            &HostRuntime::transportSessionStateThunk,
            &HostRuntime::transportPeerStateThunk,
            &HostRuntime::transportPeerCountThunk,
            &HostRuntime::transportSendToEndpointThunk,
            &HostRuntime::transportBindSessionThunk,
        };
        return &transportCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Effects) &&
        version == kEffectsCapabilityVersion) {
        static EffectsCapabilityV1 effectsCapability{
            kEffectsCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(EffectsCapabilityV1)),
            &effectsInvoke,
        };
        return &effectsCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::AuthoredRebuild) &&
        version == kAuthoredRebuildCapabilityVersion) {
        static AuthoredRebuildCapabilityV1 authoredRebuildCapability{
            kAuthoredRebuildCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(AuthoredRebuildCapabilityV1)),
            &authoredRebuildWorker,
            &authoredRebuildSubmit,
            &authoredRebuildTryTake,
            &authoredRebuildRetire,
            &authoredRebuildCancel,
        };
        return &authoredRebuildCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::TransportSession) &&
        version == kTransportSessionCapabilityVersion) {
        static TransportSessionCapabilityV1 transportSessionCapability{
            kTransportSessionCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(TransportSessionCapabilityV1)),
            &transportServerState,
            &transportServerStart,
            &transportServerStop,
            &transportServerTick,
            &transportServerInitIce,
            &transportSetNonBlocking,
            &transportDisableUdpConnReset,
            &transportServerHostPlayerId,
            &transportServerSetHostPlayerId,
        };
        return &transportSessionCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::GlResource) &&
        version == kGlResourceCapabilityVersion) {
        static GlResourceCapabilityV1 glResourceCapability{
            kGlResourceCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(GlResourceCapabilityV1)),
            &glGenTexture, &glDeleteTexture,
            &glGenBuffer, &glDeleteBuffer,
            &glGenVertexArray, &glDeleteVertexArray,
            &glGenFramebuffer, &glDeleteFramebuffer,
            &glGenRenderbuffer, &glDeleteRenderbuffer,
            &glGenQuery, &glDeleteQuery,
            &glDeleteShaderRes, &glDeleteProgramRes,
            &glGenerateMipmapRes,
        };
        return &glResourceCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Socket) &&
        version == kSocketCapabilityVersion) {
        static SocketCapabilityV1 socketCapability{
            kSocketCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(SocketCapabilityV1)),
            &socketOpenUdp,
            &socketClose,
            &socketBindAddress,
            &socketGetLocalAddress,
            &socketSetNonblocking,
            &socketSetReuse,
            &socketSetExclusive,
            &socketSetRecvTimeout,
            &socketSendTo,
            &socketRecvFrom,
        };
        return &socketCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Files) &&
        version == kFilesCapabilityVersion) {
        static FilesCapabilityV1 filesCapability{
            kFilesCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(FilesCapabilityV1)),
            &filesOpenWrite,
            &filesWrite,
            &filesClose,
        };
        return &filesCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Ice) &&
        version == kIceCapabilityVersion) {
        static IceCapabilityV1 iceCapability{
            kIceCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(IceCapabilityV1)),
            &iceCreate,
            &iceDestroy,
            &iceInitialize,
            &iceShutdown,
            &iceGather,
            &iceLocalSdp,
            &iceSetRemote,
            &iceSend,
            &iceTick,
            &iceState,
            &icePollEvents,
            &iceLogSelectedPath,
        };
        return &iceCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Diagnostics) &&
        version == kDiagnosticsCapabilityVersion) {
        static DiagnosticsCapabilityV1 diagnosticsCapability{
            kDiagnosticsCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(DiagnosticsCapabilityV1)),
            &diagnosticsCaptureStack,
        };
        return &diagnosticsCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::RenderDevice) &&
        version == kRenderDeviceCapabilityVersion) {
        static RenderDeviceCapabilityV1 renderDeviceCapability{
            kRenderDeviceCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(RenderDeviceCapabilityV1)),
            &renderDeviceAcquire,
            &renderDeviceBeginFrame,
            &renderDeviceEndFrame,
            &renderDeviceShouldClose,
            &renderDeviceShutdown,
            &renderDeviceApplyVideoMode,
            &renderDeviceSetVSync,
            &renderDeviceReloadWorldShader,
            &renderDeviceSetCursorVisible,
        };
        return &renderDeviceCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::State) &&
        version == kStateCapabilityVersion) {
        static StateCapabilityV1 stateCapability{
            kStateCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(StateCapabilityV1)),
            [](void*) { return HostRuntime::instance().menuState(); },
            [](void*, mimita::host::StateObjectId id, std::uint32_t size,
                std::uint32_t alignment, std::int32_t* created) {
                return HostRuntime::instance().ensureStateObject(
                    static_cast<std::uint32_t>(id), size, alignment, created);
            },
        };
        return &stateCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Globals) &&
        version == kGlobalCapabilityVersion) {
        static GlobalCapabilityV1 globalCapability{
            kGlobalCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(GlobalCapabilityV1)),
            &globalGetPointer,
            &globalGetBool,
        };
        return &globalCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Persistence) &&
        version == kPersistenceCapabilityVersion) {
        static PersistenceCapabilityV1 persistenceCapability{
            kPersistenceCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(PersistenceCapabilityV1)),
            &persistenceInvoke,
        };
        return &persistenceCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Gamemode) &&
        version == kGamemodeCapabilityVersion) {
        static GamemodeCapabilityV1 gamemodeCapability{
            kGamemodeCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(GamemodeCapabilityV1)),
            &duelInvoke,
        };
        return &gamemodeCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Entity) &&
        version == kEntityCapabilityVersion) {
        static EntityCapabilityV1 entityCapability{
            kEntityCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(EntityCapabilityV1)),
            &HostRuntime::entityCapabilityReadThunk,
            &HostRuntime::entityCapabilityWriteThunk,
            &HostRuntime::entityCapabilityListThunk,
            &HostRuntime::entityCapabilityCreateThunk,
            &HostRuntime::entityCapabilityResetTransformThunk,
            &HostRuntime::entityCapabilityRemoveThunk,
            &HostRuntime::entityCapabilityFindConnectionThunk,
            &HostRuntime::entityCapabilityFindReconnectTokenThunk,
            &HostRuntime::entityCapabilityFindJoinTokenThunk,
            &HostRuntime::entityCapabilitySendAvatarManifestsThunk,
        };
        return &entityCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::World) &&
        version == kWorldCapabilityVersion) {
        static WorldCapabilityV1 worldCapability{
            kWorldCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(WorldCapabilityV1)),
            &HostRuntime::worldInfoThunk,
            &HostRuntime::worldRaycastThunk,
            &HostRuntime::worldPickSpawnThunk,
        };
        return &worldCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::SessionIdentity) &&
        version == kSessionIdentityCapabilityVersion) {
        static SessionIdentityCapabilityV1 sessionIdentityCapability{
            kSessionIdentityCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(SessionIdentityCapabilityV1)),
            &HostRuntime::sessionGetTextThunk,
            &HostRuntime::sessionSetTextThunk,
            &HostRuntime::sessionGetFlagThunk,
            &HostRuntime::sessionSetFlagThunk,
            &HostRuntime::sessionSetPasswordThunk,
            &HostRuntime::sessionPasswordMatchesThunk,
        };
        return &sessionIdentityCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::SessionEvents) &&
        version == kSessionEventsCapabilityVersion) {
        static SessionEventsCapabilityV1 sessionEventsCapability{
            kSessionEventsCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(SessionEventsCapabilityV1)),
            &HostRuntime::sessionEventsNextIdThunk,
            &HostRuntime::sessionEventsSessionIdThunk,
            &HostRuntime::sessionEventsQueueToPlayerThunk,
            &HostRuntime::sessionEventsQueueToAllThunk,
            &HostRuntime::sessionEventsResetPlayerThunk,
        };
        return &sessionEventsCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Render) &&
        version == kRenderCapabilityVersion) {
        static RenderCapabilityV1 renderCapability{
            kRenderCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(RenderCapabilityV1)),
            &HostRuntime::renderSubmitThunk,
            &HostRuntime::renderFlushThunk,
            &HostRuntime::renderPendingThunk,
            &renderBufferData,
            &renderBufferSubData,
            &renderTexImage2D,
            &renderTexParameteri,
            &renderVertexAttrib,
            &renderEnableAttrib,
            &renderUniform1i,
            &renderUniform1f,
            &renderUniform2f,
            &renderUniform3f,
            &renderUniform4f,
            &renderUniform1fv,
            &renderUniform2fv,
            &renderUniform3fv,
            &renderUniformMatrix4,
            &renderCreateShader,
            &renderShaderSource,
            &renderCompileShader,
            &renderCreateProgram,
            &renderAttachShader,
            &renderLinkProgram,
            &renderGetUniformLocation,
            &renderGetUniformiv,
            &renderGetProgramiv,
            &renderGetShaderiv,
            &renderGetProgramInfoLog,
            &renderGetShaderInfoLog,
            &renderGetError,
            &renderFramebufferTexture2D,
            &renderBindRenderbuffer,
            &renderRenderbufferStorage,
            &renderFramebufferRenderbuffer,
            &renderCheckFramebufferStatus,
        };
        return &renderCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Jobs) &&
        version == kJobsCapabilityVersion) {
        static JobsCapabilityV1 jobsCapability{
            kJobsCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(JobsCapabilityV1)),
            &HostRuntime::jobsAcquireThunk,
            &HostRuntime::jobsReleaseThunk,
            &HostRuntime::jobsBusyThunk,
            &jsonWatchCreate,
            &jsonWatchDestroy,
            &jsonWatchConsumeDirty,
            &jsonWatchMarkDirty,
            &HostRuntime::jobsSubmitThunk,
            &HostRuntime::jobsStartWorkerThunk,
            &HostRuntime::jobsWaitWorkerThunk,
            &HostRuntime::jobsReleaseWorkerThunk,
        };
        return &jobsCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Callbacks) &&
        version == kCallbacksCapabilityVersion) {
        static CallbacksCapabilityV1 callbacksCapability{
            kCallbacksCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(CallbacksCapabilityV1)),
            &HostRuntime::callbackRegisterThunk,
            &HostRuntime::callbackUnregisterThunk,
            &HostRuntime::callbackBusyThunk,
        };
        return &callbacksCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Resources) &&
        version == kResourcesCapabilityVersion) {
        static ResourcesCapabilityV1 resourcesCapability{
            kResourcesCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(ResourcesCapabilityV1)),
            &HostRuntime::resourceAllocateThunk,
            &HostRuntime::resourceReleaseThunk,
            &HostRuntime::resourceBusyThunk,
        };
        return &resourcesCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Audio) &&
        version == kAudioCapabilityVersion) {
        static AudioCapabilityV1 audioCapability{
            kAudioCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(AudioCapabilityV1)),
            &audioInvoke,
        };
        return &audioCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Modules) &&
        version == kModuleCapabilityVersion) {
        static ModuleCapabilityV1 moduleCapability{
            kModuleCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(ModuleCapabilityV1)),
            &moduleInvoke,
        };
        return &moduleCapability;
    }
    return nullptr;
}

void HostRuntime::setTransportAdapter(const TransportAdapterV1* adapter, void* owner) {
    transportAdapter_ = adapter;
    transportOwner_ = owner;
}

void HostRuntime::clearTransportAdapter() {
    transportAdapter_ = nullptr;
    transportOwner_ = nullptr;
}

TransportResult HostRuntime::transportSendThunk(void* userData, PeerHandle peer,
                                                TransportChannel, std::uint32_t,
                                                std::uint32_t,
                                                const std::uint8_t* payload,
                                                std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_) {
        return TransportResult::Unsupported;
    }
    if (runtime->transportAdapter_->send_to_peer) {
        return runtime->transportAdapter_->send_to_peer(
                   runtime->transportOwner_, peer, payload, payloadSize)
            ? TransportResult::Ok : TransportResult::TransportError;
    }
    if (!runtime->transportAdapter_->send_to_server) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->send_to_server(
               runtime->transportOwner_, payload, payloadSize)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportBroadcastThunk(void* userData,
                                                     TransportChannel, std::uint32_t,
                                                     std::uint32_t,
                                                     const std::uint8_t* payload,
                                                     std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->broadcast) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->broadcast(
               runtime->transportOwner_, payload, payloadSize)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportSendToPlayerThunk(void* userData,
                                                        std::uint32_t playerId,
                                                        std::uint32_t,
                                                        std::uint32_t,
                                                        const std::uint8_t* payload,
                                                        std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->send_to_player) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->send_to_player(
               runtime->transportOwner_, playerId, payload, payloadSize)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportSessionStateThunk(void* userData,
                                                        SessionStateV1* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->session_state) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->session_state(runtime->transportOwner_, out)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportPeerStateThunk(void*, PeerHandle, PeerStateV1*) {
    return TransportResult::Unsupported;
}

std::uint32_t HostRuntime::transportPeerCountThunk(void* userData) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->peer_count) {
        return 0;
    }
    return runtime->transportAdapter_->peer_count(runtime->transportOwner_);
}

TransportResult HostRuntime::transportBindSessionThunk(void* userData,
                                                       PeerHandle peer,
                                                       std::uint32_t playerId) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->bind_session) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->bind_session(
               runtime->transportOwner_, peer, playerId)
        ? TransportResult::Ok : TransportResult::TransportError;
}

TransportResult HostRuntime::transportSendToEndpointThunk(
    void* userData, std::uint32_t addressNetworkOrder,
    std::uint16_t portNetworkOrder, TransportChannel, std::uint32_t,
    std::uint32_t, const std::uint8_t* payload, std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->send_to_endpoint) {
        return TransportResult::Unsupported;
    }
    return runtime->transportAdapter_->send_to_endpoint(
               runtime->transportOwner_, addressNetworkOrder, portNetworkOrder,
               payload, payloadSize)
        ? TransportResult::Ok : TransportResult::TransportError;
}

void HostRuntime::logThunk(void* userData, std::int32_t level, const char* message) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    emitProbeEvent("game.module.log", "module_log",
                   runtime ? static_cast<std::uint32_t>(runtime->tick_) : 0,
                   nlohmann::json{{"level", level},
                                  {"message", message ? message : ""}});
}

HostResult HostRuntime::entityReadThunk(void* userData, EntityHandle handle,
                                         EntitySnapshotV1* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ || !runtime->entityAdapter_->read ||
        !out || out->structSize < sizeof(EntitySnapshotV1)) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->read(runtime->entityOwner_, handle, out);
}

HostResult HostRuntime::entityWriteThunk(void* userData, EntityHandle handle,
                                          const EntitySnapshotV1* in) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ || !runtime->entityAdapter_->write ||
        !in || in->structSize < sizeof(EntitySnapshotV1)) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->write(runtime->entityOwner_, handle, in);
}

HostResult HostRuntime::entityCapabilityReadThunk(void* userData,
                                                   EntityHandle handle,
                                                   EntitySnapshotV1* out) {
    return entityReadThunk(userData, handle, out);
}

HostResult HostRuntime::entityCapabilityWriteThunk(void* userData,
                                                    EntityHandle handle,
                                                    const EntitySnapshotV1* in) {
    return entityWriteThunk(userData, handle, in);
}

std::uint32_t HostRuntime::entityCapabilityListThunk(void* userData,
                                                     std::int32_t kind,
                                                     EntityHandle* out,
                                                     std::uint32_t capacity) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ || !runtime->entityAdapter_->list ||
        !out || capacity == 0) {
        return 0;
    }
    return runtime->entityAdapter_->list(runtime->entityOwner_, kind, out,
                                         capacity);
}

namespace {

const std::string* sessionText(std::uint32_t field) {
    switch (static_cast<SessionTextField>(field)) {
        case SessionTextField::MapId: return &sessionMapId();
        case SessionTextField::CoordinatorCode: return &sessionCoordinatorCode();
        case SessionTextField::CoordinatorJoinToken:
            return &sessionCoordinatorJoinToken();
        case SessionTextField::MatchMode: return &sessionMatchMode();
        case SessionTextField::CommunityMode: return &sessionCommunityMode();
    }
    return nullptr;
}

}  // namespace

std::uint32_t HostRuntime::sessionGetTextThunk(void*,
                                               std::uint32_t field,
                                               char* out,
                                               std::uint32_t capacity) {
    const std::string* text = sessionText(field);
    if (!text) {
        return 0;
    }
    if (out && capacity > 0) {
        const std::size_t count =
            text->size() < capacity - 1 ? text->size() : capacity - 1;
        std::memcpy(out, text->data(), count);
        out[count] = '\0';
    }
    return static_cast<std::uint32_t>(text->size());
}

void HostRuntime::sessionSetTextThunk(void*, std::uint32_t field,
                                      const char* value) {
    const std::string text = value ? value : "";
    switch (static_cast<SessionTextField>(field)) {
        case SessionTextField::MapId:
            setSessionMapId(text);
            break;
        case SessionTextField::CoordinatorCode:
            setSessionCoordinatorCode(text);
            break;
        case SessionTextField::CoordinatorJoinToken:
            setSessionCoordinatorJoinToken(text);
            break;
        case SessionTextField::MatchMode:
            setSessionMatchMode(text);
            break;
        case SessionTextField::CommunityMode:
            setSessionCommunityMode(text);
            break;
    }
}

std::uint32_t HostRuntime::sessionGetFlagThunk(void*, std::uint32_t field) {
    switch (static_cast<SessionFlagField>(field)) {
        case SessionFlagField::PasswordProtected:
            return sessionPasswordProtected() ? 1u : 0u;
    }
    return 0;
}

void HostRuntime::sessionSetFlagThunk(void*, std::uint32_t field,
                                      std::uint32_t value) {
    switch (static_cast<SessionFlagField>(field)) {
        case SessionFlagField::PasswordProtected:
            setSessionPasswordState(value != 0, "");
            break;
    }
}

void HostRuntime::sessionSetPasswordThunk(void*, std::uint32_t protectedFlag,
                                          const char* password) {
    setSessionPasswordState(protectedFlag != 0, password ? password : "");
}

std::uint32_t HostRuntime::sessionPasswordMatchesThunk(void*,
                                                       const char* supplied) {
    return sessionPasswordMatches(supplied) ? 1u : 0u;
}

std::uint32_t HostRuntime::sessionEventsNextIdThunk(void*) {
    auto& events = HostServerSessionEvents::instance();
    return events.capability()->next_event_id(events.owner());
}

std::uint32_t HostRuntime::sessionEventsSessionIdThunk(void*,
                                                       std::uint32_t playerId) {
    auto& events = HostServerSessionEvents::instance();
    return events.capability()->session_id(events.owner(), playerId);
}

SessionEventQueueResult HostRuntime::sessionEventsQueueToPlayerThunk(
    void*, std::uint32_t playerId, const std::uint8_t* bytes,
    std::uint32_t size, std::uint32_t eventId, std::uint32_t maxPending) {
    auto& events = HostServerSessionEvents::instance();
    return events.capability()->queue_to_player(
        events.owner(), playerId, bytes, size, eventId, maxPending);
}

SessionEventQueueResult HostRuntime::sessionEventsQueueToAllThunk(
    void*, const std::uint8_t* bytes, std::uint32_t size,
    std::uint32_t eventId, std::uint32_t maxPending) {
    auto& events = HostServerSessionEvents::instance();
    return events.capability()->queue_to_all(events.owner(), bytes, size,
                                             eventId, maxPending);
}

HostResult HostRuntime::sessionEventsResetPlayerThunk(
    void*, std::uint32_t playerId) {
    auto& events = HostServerSessionEvents::instance();
    return events.capability()->reset_player(events.owner(), playerId);
}

HostResult HostRuntime::entityCapabilityCreateThunk(void* userData,
                                                    std::int32_t kind,
                                                    EntityHandle* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ ||
        !runtime->entityAdapter_->create || !out) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->create(runtime->entityOwner_, kind, out);
}

HostResult HostRuntime::entityCapabilityResetTransformThunk(
    void* userData, EntityHandle handle, const float position[3],
    const float velocity[3], float yaw, std::uint32_t onGround,
    std::uint32_t invulnerabilityTicks, std::uint32_t maxAirJumps,
    std::uint32_t awaitAck) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ ||
        !runtime->entityAdapter_->reset_transform) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->reset_transform(
        runtime->entityOwner_, handle, position, velocity, yaw, onGround,
        invulnerabilityTicks, maxAirJumps, awaitAck);
}

HostResult HostRuntime::entityCapabilityFindConnectionThunk(
    void* userData, std::uint64_t peer, EntityHandle* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ ||
        !runtime->entityAdapter_->find_connection) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->find_connection(runtime->entityOwner_,
                                                    peer, out);
}

HostResult HostRuntime::entityCapabilityFindReconnectTokenThunk(
    void* userData, const char* token, std::uint32_t includePrevious,
    EntityHandle* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ ||
        !runtime->entityAdapter_->find_reconnect_token) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->find_reconnect_token(
        runtime->entityOwner_, token, includePrevious, out);
}

HostResult HostRuntime::entityCapabilityFindJoinTokenThunk(
    void* userData, const char* token, EntityHandle* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ ||
        !runtime->entityAdapter_->find_join_token) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->find_join_token(runtime->entityOwner_,
                                                    token, out);
}

HostResult HostRuntime::entityCapabilitySendAvatarManifestsThunk(
    void* userData, EntityHandle receiver) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ ||
        !runtime->entityAdapter_->send_avatar_manifests) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->send_avatar_manifests(
        runtime->entityOwner_, receiver);
}

HostResult HostRuntime::entityCapabilityRemoveThunk(void* userData,
                                                    EntityHandle handle) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->entityAdapter_ ||
        !runtime->entityAdapter_->remove) {
        return HostResult::Unsupported;
    }
    return runtime->entityAdapter_->remove(runtime->entityOwner_, handle);
}

HostResult HostRuntime::worldInfoThunk(void* userData, WorldInfoV1* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->worldAdapter_ || !runtime->worldAdapter_->info ||
        !out || out->structSize < sizeof(WorldInfoV1)) {
        return HostResult::Unsupported;
    }
    return runtime->worldAdapter_->info(runtime->worldOwner_, out);
}

HostResult HostRuntime::worldRaycastThunk(void* userData, const float origin[3],
                                          const float direction[3],
                                          float maxDistance,
                                          WorldRayHitV1* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->worldAdapter_ ||
        !runtime->worldAdapter_->raycast || !origin || !direction || !out ||
        out->structSize < sizeof(WorldRayHitV1)) {
        return HostResult::Unsupported;
    }
    return runtime->worldAdapter_->raycast(runtime->worldOwner_, origin,
                                           direction, maxDistance, out);
}

HostResult HostRuntime::worldPickSpawnThunk(void* userData,
                                            std::uint32_t playerId,
                                            WorldSpawnV1* out) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->worldAdapter_ ||
        !runtime->worldAdapter_->pick_spawn || !out) {
        return HostResult::Unsupported;
    }
    return runtime->worldAdapter_->pick_spawn(runtime->worldOwner_, playerId,
                                              out);
}

HostResult HostRuntime::renderSubmitThunk(void* userData,
                                           const RenderCommandV1* commands,
                                           std::uint32_t count) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !commands || count == 0 ||
        count > kRenderMaxCommandsPerSubmit) {
        return HostResult::InvalidArgument;
    }
    for (std::uint32_t i = 0; i < count; ++i) {
        const RenderCommandV1& command = commands[i];
        switch (static_cast<RenderOp>(command.op)) {
            case RenderOp::Clear: {
                GLbitfield mask = 0;
                if (command.flags & 1u) mask |= GL_COLOR_BUFFER_BIT;
                if (command.flags & 2u) mask |= GL_DEPTH_BUFFER_BIT;
                if (command.flags & 4u) mask |= GL_STENCIL_BUFFER_BIT;
                glClear(mask ? mask : (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
                break;
            }
            case RenderOp::Viewport:
                glViewport(command.ints[0], command.ints[1], command.ints[2], command.ints[3]);
                break;
            case RenderOp::BindTexture:
            case RenderOp::BindTextureTarget:
                glBindTexture(toGl(static_cast<mimita::render::TextureTarget>(command.ints[0])),
                              static_cast<GLuint>(command.handle0));
                break;
            case RenderOp::BindShader:
            case RenderOp::UseProgram:
                glUseProgram(static_cast<GLuint>(command.handle0));
                break;
            case RenderOp::SetState:
                if (command.flags & 1u) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
                if (command.flags & 2u) glEnable(GL_BLEND); else glDisable(GL_BLEND);
                if (command.flags & 4u) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
                break;
            case RenderOp::DrawArrays:
                glDrawArrays(toGl(static_cast<mimita::render::Primitive>(command.ints[0])),
                             command.ints[1], command.ints[2]);
                break;
            case RenderOp::DrawArraysInstanced:
                glDrawArraysInstanced(
                    toGl(static_cast<mimita::render::Primitive>(command.ints[0])),
                    command.ints[1], command.ints[2], command.ints[3]);
                break;
            case RenderOp::VertexAttribDivisor:
                glVertexAttribDivisor(static_cast<GLuint>(command.ints[0]),
                                      static_cast<GLuint>(command.ints[1]));
                break;
            case RenderOp::TexParameterfv:
                glTexParameterfv(
                    toGl(static_cast<mimita::render::TextureTarget>(command.ints[0])),
                    toGl(static_cast<mimita::render::TextureParameter>(command.ints[1])),
                    command.floats);
                break;
            case RenderOp::DrawBuffer:
                glDrawBuffer(toGl(static_cast<mimita::render::FramebufferBuffer>(
                    command.ints[0])));
                break;
            case RenderOp::ReadBuffer:
                glReadBuffer(toGl(static_cast<mimita::render::FramebufferBuffer>(
                    command.ints[0])));
                break;
            case RenderOp::PixelStore:
                glPixelStorei(
                    toGl(static_cast<mimita::render::PixelStoreParameter>(
                        command.ints[0])),
                    command.ints[1]);
                break;
            case RenderOp::GenerateMipmap:
                glGenerateMipmap(
                    toGl(static_cast<mimita::render::TextureTarget>(
                        command.ints[0])));
                break;
            case RenderOp::DrawElements:
                if (command.handle1)
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(command.handle1));
                glDrawElements(toGl(static_cast<mimita::render::Primitive>(command.ints[0])),
                               command.ints[2],
                               toGl(static_cast<mimita::render::IndexType>(command.ints[1])),
                               reinterpret_cast<const void*>(
                                   static_cast<std::uintptr_t>(command.ints[3])));
                break;
            case RenderOp::BindVertexArray:
                glBindVertexArray(static_cast<GLuint>(command.handle0));
                break;
            case RenderOp::BindBuffer:
                glBindBuffer(toGl(static_cast<mimita::render::BufferTarget>(command.ints[0])),
                             static_cast<GLuint>(command.handle0));
                break;
            case RenderOp::BindFramebuffer:
                glBindFramebuffer(toGl(static_cast<mimita::render::FramebufferTarget>(command.ints[0])),
                                  static_cast<GLuint>(command.handle0));
                break;
            case RenderOp::DepthTest:
                if (command.flags) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
                break;
            case RenderOp::Blend:
                if (command.flags) glEnable(GL_BLEND); else glDisable(GL_BLEND);
                break;
            case RenderOp::Cull:
                if (command.flags) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
                break;
            case RenderOp::ActiveTexture:
                glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + command.ints[0]));
                break;
            case RenderOp::EnableCapability:
                glEnable(toGl(static_cast<mimita::render::Capability>(command.ints[0])));
                break;
            case RenderOp::DisableCapability:
                glDisable(toGl(static_cast<mimita::render::Capability>(command.ints[0])));
                break;
            case RenderOp::DepthMask:
                glDepthMask(command.flags ? GL_TRUE : GL_FALSE);
                break;
            case RenderOp::DepthFunc:
                glDepthFunc(toGl(static_cast<mimita::render::DepthFunc>(command.ints[0])));
                break;
            case RenderOp::CullFaceMode:
                glCullFace(toGl(static_cast<mimita::render::CullFaceMode>(command.ints[0])));
                break;
            case RenderOp::BlendFunc:
                glBlendFunc(toGl(static_cast<mimita::render::BlendFactor>(command.ints[0])),
                            toGl(static_cast<mimita::render::BlendFactor>(command.ints[1])));
                break;
            case RenderOp::BlendFuncSeparate:
                glBlendFuncSeparate(
                    toGl(static_cast<mimita::render::BlendFactor>(command.ints[0])),
                    toGl(static_cast<mimita::render::BlendFactor>(command.ints[1])),
                    toGl(static_cast<mimita::render::BlendFactor>(command.ints[2])),
                    toGl(static_cast<mimita::render::BlendFactor>(command.ints[3])));
                break;
            case RenderOp::PolygonMode:
                glPolygonMode(GL_FRONT_AND_BACK,
                              toGl(static_cast<mimita::render::PolygonMode>(command.ints[0])));
                break;
            case RenderOp::Scissor:
                glScissor(command.ints[0], command.ints[1], command.ints[2], command.ints[3]);
                break;
            case RenderOp::LineWidth:
                glLineWidth(command.floats[0]);
                break;
            case RenderOp::ClearColor:
                glClearColor(command.floats[0], command.floats[1], command.floats[2],
                             command.floats[3]);
                break;
            case RenderOp::PushState:
                gHostGlStateStack.push_back(captureHostGlState());
                break;
            case RenderOp::PopState:
                if (!gHostGlStateStack.empty()) {
                    const HostGlState state = gHostGlStateStack.back();
                    gHostGlStateStack.pop_back();
                    restoreHostGlState(state);
                }
                break;
            default:
                return HostResult::Unsupported;
        }
    }
    runtime->renderPending_.fetch_add(count, std::memory_order_acq_rel);
    return HostResult::Ok;
}

HostResult HostRuntime::renderFlushThunk(void* userData) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return HostResult::InvalidArgument;
    }
    runtime->renderPending_.store(0, std::memory_order_release);
    return HostResult::Ok;
}

std::uint32_t HostRuntime::renderPendingThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->renderPending_.load(std::memory_order_acquire) : 0;
}

std::uint64_t HostRuntime::jobsAcquireThunk(void* userData, const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return 0;
    }
    runtime->jobsBusy_.fetch_add(1, std::memory_order_acq_rel);
    return runtime->nextLease_.fetch_add(1, std::memory_order_relaxed);
}

HostResult HostRuntime::jobsReleaseThunk(void* userData, std::uint64_t) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return HostResult::InvalidArgument;
    }
    std::uint32_t current = runtime->jobsBusy_.load(std::memory_order_acquire);
    while (current > 0) {
        if (runtime->jobsBusy_.compare_exchange_weak(
                current, current - 1, std::memory_order_acq_rel)) {
            break;
        }
    }
    return HostResult::Ok;
}

std::uint32_t HostRuntime::jobsBusyThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->jobsBusy_.load(std::memory_order_acquire) : 0;
}

void HostRuntime::jobsSubmitThunk(void* userData, void* job,
                                  void (*invoke)(void*),
                                  void (*destroy)(void*), const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !job || !invoke) return;
    runtime->jobsBusy_.fetch_add(1, std::memory_order_acq_rel);
    gJobExecutor.submit(ResidentJob{job, invoke, destroy,
                                    &HostRuntime::jobsDoneThunk, runtime});
}

void HostRuntime::jobsDoneThunk(void* userData) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) return;
    std::uint32_t current = runtime->jobsBusy_.load(std::memory_order_acquire);
    while (current > 0 &&
           !runtime->jobsBusy_.compare_exchange_weak(
               current, current - 1, std::memory_order_acq_rel)) {
    }
}

std::uint64_t HostRuntime::jobsStartWorkerThunk(
    void* userData, void* job, void (*invoke)(void*), void (*destroy)(void*),
    const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !job || !invoke) return 0;
    auto slot = std::make_shared<HostWorkerSlot>();
    std::uint64_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(gWorkerMutex);
        handle = gNextWorker++;
        gWorkers.emplace(handle, slot);
    }
    runtime->jobsBusy_.fetch_add(1, std::memory_order_acq_rel);
    slot->thread = std::thread([runtime, slot, job, invoke, destroy] {
        try {
            invoke(job);
        } catch (...) {
        }
        if (destroy) destroy(job);
        HostRuntime::jobsDoneThunk(runtime);
    });
    return handle;
}

void HostRuntime::jobsWaitWorkerThunk(void*, std::uint64_t handle) {
    std::shared_ptr<HostWorkerSlot> slot;
    {
        std::lock_guard<std::mutex> lock(gWorkerMutex);
        auto it = gWorkers.find(handle);
        if (it == gWorkers.end()) return;
        slot = it->second;
    }
    if (slot->thread.joinable()) slot->thread.join();
}

void HostRuntime::jobsReleaseWorkerThunk(void*, std::uint64_t handle) {
    std::shared_ptr<HostWorkerSlot> slot;
    {
        std::lock_guard<std::mutex> lock(gWorkerMutex);
        auto it = gWorkers.find(handle);
        if (it == gWorkers.end()) return;
        slot = it->second;
        gWorkers.erase(it);
    }
    if (slot->thread.joinable()) slot->thread.join();
}

std::uint64_t HostRuntime::callbackRegisterThunk(void* userData, std::uint32_t,
                                                  const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return 0;
    }
    runtime->callbacksBusy_.fetch_add(1, std::memory_order_acq_rel);
    return runtime->nextLease_.fetch_add(1, std::memory_order_relaxed);
}

HostResult HostRuntime::callbackUnregisterThunk(void* userData, std::uint64_t) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime) {
        return HostResult::InvalidArgument;
    }
    std::uint32_t current = runtime->callbacksBusy_.load(std::memory_order_acquire);
    while (current > 0) {
        if (runtime->callbacksBusy_.compare_exchange_weak(
                current, current - 1, std::memory_order_acq_rel)) {
            break;
        }
    }
    return HostResult::Ok;
}

std::uint32_t HostRuntime::callbackBusyThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->callbacksBusy_.load(std::memory_order_acquire) : 0;
}

std::uint64_t HostRuntime::resourceAllocateThunk(void* userData,
                                                  std::uint32_t,
                                                  std::size_t bytes,
                                                  std::size_t,
                                                  const char*) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || bytes == 0) {
        return 0;
    }
    void* pointer = std::malloc(bytes);
    if (!pointer) {
        return 0;
    }
    const std::uint64_t handle =
        runtime->nextLease_.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(runtime->resourceMutex_);
        runtime->resources_[handle] = pointer;
    }
    return handle;
}

void HostRuntime::resourceReleaseThunk(void* userData, std::uint64_t resource) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || resource == 0) {
        return;
    }
    void* pointer = nullptr;
    {
        std::lock_guard<std::mutex> lock(runtime->resourceMutex_);
        auto it = runtime->resources_.find(resource);
        if (it == runtime->resources_.end()) {
            return;
        }
        pointer = it->second;
        runtime->resources_.erase(it);
    }
    std::free(pointer);
}

std::uint32_t HostRuntime::resourceBusyThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    if (!runtime) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(const_cast<HostRuntime*>(runtime)->resourceMutex_);
    return static_cast<std::uint32_t>(runtime->resources_.size());
}

std::uint64_t HostRuntime::tickThunk(void* userData) {
    const auto* runtime = static_cast<const HostRuntime*>(userData);
    return runtime ? runtime->tick_ : 0;
}

double HostRuntime::timeThunk(void*) {
    const auto elapsed = std::chrono::steady_clock::now() - gStartTime;
    return std::chrono::duration<double>(elapsed).count();
}

void* HostRuntime::reserveThunk(void*, std::size_t bytes, std::size_t, const char*) {
    return std::malloc(bytes);
}

void HostRuntime::releaseThunk(void*, void* pointer, const char*) {
    std::free(pointer);
}

}  // namespace mimita::host
