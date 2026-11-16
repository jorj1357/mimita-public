// 2026-11-12
// Resident host runtime implementation. Hot by default; MIMITA_GAME_DISABLE=1
// keeps the resident EXE path.

#include "host/host-runtime.h"
#include "renderer/renderer.h"
#include "render/render-gl-mapping.h"
#include <GLFW/glfw3.h>

#include <algorithm>
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
#include "host/abi/effects-capability.h"
#include "host/abi/global-capability.h"
#include "host/abi/files-capability.h"
#include "host/abi/gl-resource-capability.h"
#include "host/abi/entity-domain-capability.h"
#include "host/abi/session-events-capability.h"
#include "host/abi/session-identity-capability.h"
#include "host/host-server-session-events.h"
#include "host/host-server-entity-adapter.h"
#include "host/abi/socket-capability.h"
#include "host/host-session-identity.h"
#include "host/abi/ice-capability.h"
#include "host/abi/transport-session-capability.h"
#include "host/abi/diagnostics-capability.h"
#include "host/abi/render-device-capability.h"
#include "host/abi/window-capability.h"
#include "host/abi/state-capability.h"
#include "game/state-bridge-module.h"
#include "host/abi/persistence-capability.h"
#include "host/abi/module-capability.h"
#include "host/abi/transport-capability.h"
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
using mimita::render::toGl;
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

std::int32_t effectsInvoke(void*, EffectsOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize) {
    EffectsHostCallV1 call;
    call.op = op;
    call.input = input;
    call.inputSize = inputSize;
    call.output = output;
    call.outputSize = outputSize;
    return HostRuntime::instance().invokeEffectsHost(&call) ? 1 : 0;
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
        // The thread that creates the context is the only thread allowed to
        // call GL. Workers must marshal uploads to the main thread.
        HostRuntime::instance().markGlThread();
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

void renderDeviceSetCursorVisible(void*, std::int32_t visible) {
    if (!gRenderDevice || !gRenderDevice->window) return;
    glfwSetInputMode(gRenderDevice->window, GLFW_CURSOR,
                     visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

std::int32_t windowGetSize(void*, std::int32_t* width, std::int32_t* height) {
    if (!gRenderDevice || !gRenderDevice->window || !width || !height) return 0;
    glfwGetWindowSize(gRenderDevice->window, width, height);
    return 1;
}

std::int32_t windowGetFramebufferSize(void*, std::int32_t* width,
                                      std::int32_t* height) {
    if (!gRenderDevice || !gRenderDevice->window || !width || !height) return 0;
    glfwGetFramebufferSize(gRenderDevice->window, width, height);
    return 1;
}

std::int32_t windowGetPosition(void*, std::int32_t* x, std::int32_t* y) {
    if (!gRenderDevice || !gRenderDevice->window || !x || !y) return 0;
    glfwGetWindowPos(gRenderDevice->window, x, y);
    return 1;
}

std::int32_t windowGetCursorPos(void*, double* x, double* y) {
    if (!gRenderDevice || !gRenderDevice->window || !x || !y) return 0;
    glfwGetCursorPos(gRenderDevice->window, x, y);
    return 1;
}

std::int32_t windowSetCursorPos(void*, double x, double y) {
    if (!gRenderDevice || !gRenderDevice->window) return 0;
    glfwSetCursorPos(gRenderDevice->window, x, y);
    return 1;
}

std::int32_t windowFocus(void*) {
    if (!gRenderDevice || !gRenderDevice->window) return 0;
    glfwFocusWindow(gRenderDevice->window);
    return 1;
}

std::int32_t windowGetCursorMode(void*) {
    if (!gRenderDevice || !gRenderDevice->window) return -1;
    return glfwGetInputMode(gRenderDevice->window, GLFW_CURSOR);
}

std::int32_t windowGetFocused(void*) {
    if (!gRenderDevice || !gRenderDevice->window) return 0;
    return glfwGetWindowAttrib(gRenderDevice->window, GLFW_FOCUSED) ? 1 : 0;
}

std::int32_t windowGetIconified(void*) {
    if (!gRenderDevice || !gRenderDevice->window) return 0;
    return glfwGetWindowAttrib(gRenderDevice->window, GLFW_ICONIFIED) ? 1 : 0;
}

std::int32_t windowSetCursorMode(void*, std::int32_t mode) {
    if (!gRenderDevice || !gRenderDevice->window || mode < 0 || mode > 2) return 0;
    glfwSetInputMode(gRenderDevice->window, GLFW_CURSOR, mode);
    return 1;
}

std::int32_t windowMouseButtonDown(void*, std::int32_t button) {
    if (!gRenderDevice || !gRenderDevice->window || button < 0 ||
        button > GLFW_MOUSE_BUTTON_LAST) return 0;
    return glfwGetMouseButton(gRenderDevice->window, button) == GLFW_PRESS ? 1 : 0;
}

std::int32_t windowKeyDown(void*, std::int32_t key) {
    if (!gRenderDevice || !gRenderDevice->window || key < 0 ||
        key > GLFW_KEY_LAST) return 0;
    return glfwGetKey(gRenderDevice->window, key) == GLFW_PRESS ? 1 : 0;
}

std::int32_t windowClipboardGet(void*, char* out, std::int32_t capacity) {
    if (!gRenderDevice || !gRenderDevice->window || !out || capacity <= 0) return -1;
    const char* text = glfwGetClipboardString(gRenderDevice->window);
    if (!text) return -1;
    const std::int32_t length = static_cast<std::int32_t>(std::strlen(text));
    const std::int32_t copy = std::min(length, capacity - 1);
    if (copy > 0) std::memcpy(out, text, static_cast<std::size_t>(copy));
    out[copy] = '\0';
    return copy;
}

std::int32_t windowClipboardSet(void*, const char* text) {
    if (!gRenderDevice || !gRenderDevice->window || !text) return 0;
    glfwSetClipboardString(gRenderDevice->window, text);
    return 1;
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

std::uint32_t glGenTexture(void*) {
    if (!HostRuntime::instance().onGlThread("glGenTexture")) return 0;
    GLuint id = 0; glGenTextures(1, &id); return id;
}
void glDeleteTexture(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteTexture")) {
        HostRuntime::instance().deferGlDelete(1, id); return;
    }
    GLuint value = id; glDeleteTextures(1, &value);
}
std::uint32_t glGenBuffer(void*) {
    if (!HostRuntime::instance().onGlThread("glGenBuffer")) return 0;
    GLuint id = 0; glGenBuffers(1, &id); return id;
}
void glDeleteBuffer(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteBuffer")) {
        HostRuntime::instance().deferGlDelete(2, id); return;
    }
    GLuint value = id; glDeleteBuffers(1, &value);
}
std::uint32_t glGenVertexArray(void*) {
    if (!HostRuntime::instance().onGlThread("glGenVertexArray")) return 0;
    GLuint id = 0; glGenVertexArrays(1, &id); return id;
}
void glDeleteVertexArray(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteVertexArray")) {
        HostRuntime::instance().deferGlDelete(3, id); return;
    }
    GLuint value = id; glDeleteVertexArrays(1, &value);
}
std::uint32_t glGenFramebuffer(void*) {
    if (!HostRuntime::instance().onGlThread("glGenFramebuffer")) return 0;
    GLuint id = 0; glGenFramebuffers(1, &id); return id;
}
void glDeleteFramebuffer(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteFramebuffer")) {
        HostRuntime::instance().deferGlDelete(4, id); return;
    }
    GLuint value = id; glDeleteFramebuffers(1, &value);
}
std::uint32_t glGenRenderbuffer(void*) {
    if (!HostRuntime::instance().onGlThread("glGenRenderbuffer")) return 0;
    GLuint id = 0; glGenRenderbuffers(1, &id); return id;
}
void glDeleteRenderbuffer(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteRenderbuffer")) {
        HostRuntime::instance().deferGlDelete(5, id); return;
    }
    GLuint value = id; glDeleteRenderbuffers(1, &value);
}
std::uint32_t glGenQuery(void*) {
    if (!HostRuntime::instance().onGlThread("glGenQuery")) return 0;
    GLuint id = 0; glGenQueries(1, &id); return id;
}
void glDeleteQuery(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteQuery")) {
        HostRuntime::instance().deferGlDelete(6, id); return;
    }
    GLuint value = id; glDeleteQueries(1, &value);
}
void glQueryCounterTimestamp(void*, std::uint32_t query) {
    if (!HostRuntime::instance().onGlThread("glQueryCounterTimestamp")) return;
    glQueryCounter(static_cast<GLuint>(query), GL_TIMESTAMP);
}
std::uint32_t glQueryResultAvailable(void*, std::uint32_t query) {
    if (!HostRuntime::instance().onGlThread("glQueryResultAvailable")) return 0;
    GLuint available = 0;
    glGetQueryObjectuiv(static_cast<GLuint>(query), GL_QUERY_RESULT_AVAILABLE,
                        &available);
    return available ? 1u : 0u;
}
std::uint64_t glQueryResultU64(void*, std::uint32_t query) {
    if (!HostRuntime::instance().onGlThread("glQueryResultU64")) return 0;
    GLuint64 value = 0;
    glGetQueryObjectui64v(static_cast<GLuint>(query), GL_QUERY_RESULT, &value);
    return static_cast<std::uint64_t>(value);
}
void glDeleteShaderRes(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteShader")) {
        HostRuntime::instance().deferGlDelete(7, id); return;
    }
    glDeleteShader(id);
}
void glDeleteProgramRes(void*, std::uint32_t id) {
    if (!HostRuntime::instance().onGlThread("glDeleteProgram")) {
        HostRuntime::instance().deferGlDelete(8, id); return;
    }
    glDeleteProgram(id);
}
void glGenerateMipmapRes(void*, std::uint32_t target) {
    if (!HostRuntime::instance().onGlThread("glGenerateMipmap")) return;
    glGenerateMipmap(target);
}

namespace {

struct HostGlState {
    GLboolean depthTest = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLboolean cullFace = GL_FALSE;
    GLboolean scissorTest = GL_FALSE;
    GLint scissorBox[4] = {0, 0, 0, 0};
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
    glGetIntegerv(GL_SCISSOR_BOX, state.scissorBox);
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
    glScissor(state.scissorBox[0], state.scissorBox[1], state.scissorBox[2],
              state.scissorBox[3]);
}

}  // namespace

HostResult renderBufferData(void*, mimita::render::BufferTarget target,
                            std::uint64_t buffer, const void* data,
                            std::uint32_t size, mimita::render::BufferUsage usage) {
    if (!HostRuntime::instance().onGlThread("renderBufferData")) return HostResult::Busy;
    glBindBuffer(toGl(target), static_cast<GLuint>(buffer));
    glBufferData(toGl(target), static_cast<GLsizeiptr>(size), data, toGl(usage));
    return HostResult::Ok;
}

HostResult renderBufferSubData(void*, mimita::render::BufferTarget target,
                               std::uint64_t buffer, std::uint32_t offset,
                               const void* data, std::uint32_t size) {
    if (!HostRuntime::instance().onGlThread("renderBufferSubData")) return HostResult::Busy;
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
    if (!HostRuntime::instance().onGlThread("renderTexImage2D")) return HostResult::Busy;
    glTexImage2D(toGl(target), level, static_cast<GLint>(toGl(format)), width,
                 height, border, toGl(pixelFormat), toGl(pixelType), data);
    return HostResult::Ok;
}

HostResult renderTexParameteri(void*, mimita::render::TextureTarget target,
                               mimita::render::TextureParameter parameter,
                               std::int32_t value) {
    if (!HostRuntime::instance().onGlThread("renderTexParameteri")) return HostResult::Busy;
    glTexParameteri(toGl(target), toGl(parameter), static_cast<GLint>(value));
    return HostResult::Ok;
}

HostResult renderVertexAttrib(void*, std::uint32_t index, std::int32_t size,
                              mimita::render::VertexType type,
                              std::uint32_t normalized, std::int32_t stride,
                              std::uint32_t offset) {
    if (!HostRuntime::instance().onGlThread("renderVertexAttrib")) return HostResult::Busy;
    glVertexAttribPointer(index, size, toGl(type), normalized != 0, stride,
                          reinterpret_cast<const void*>(
                              static_cast<std::uintptr_t>(offset)));
    return HostResult::Ok;
}

HostResult renderEnableAttrib(void*, std::uint32_t index, std::uint32_t enabled) {
    if (!HostRuntime::instance().onGlThread("renderEnableAttrib")) return HostResult::Busy;
    if (enabled) glEnableVertexAttribArray(index);
    else glDisableVertexAttribArray(index);
    return HostResult::Ok;
}

HostResult renderUniform1i(void*, std::int32_t location, std::int32_t value) {
    if (!HostRuntime::instance().onGlThread("renderUniform1i")) return HostResult::Busy;
    glUniform1i(location, value);
    return HostResult::Ok;
}
HostResult renderUniform1f(void*, std::int32_t location, float value) {
    if (!HostRuntime::instance().onGlThread("renderUniform1f")) return HostResult::Busy;
    glUniform1f(location, value);
    return HostResult::Ok;
}
HostResult renderUniform2f(void*, std::int32_t location, float x, float y) {
    if (!HostRuntime::instance().onGlThread("renderUniform2f")) return HostResult::Busy;
    glUniform2f(location, x, y);
    return HostResult::Ok;
}
HostResult renderUniform3f(void*, std::int32_t location, float x, float y, float z) {
    if (!HostRuntime::instance().onGlThread("renderUniform3f")) return HostResult::Busy;
    glUniform3f(location, x, y, z);
    return HostResult::Ok;
}
HostResult renderUniform4f(void*, std::int32_t location, float x, float y, float z, float w) {
    if (!HostRuntime::instance().onGlThread("renderUniform4f")) return HostResult::Busy;
    glUniform4f(location, x, y, z, w);
    return HostResult::Ok;
}
HostResult renderUniform1fv(void*, std::int32_t location, std::int32_t count,
                            const float* values) {
    if (!HostRuntime::instance().onGlThread("renderUniform1fv")) return HostResult::Busy;
    glUniform1fv(location, static_cast<GLsizei>(count), values);
    return HostResult::Ok;
}
HostResult renderUniform2fv(void*, std::int32_t location, std::int32_t count,
                            const float* values) {
    if (!HostRuntime::instance().onGlThread("renderUniform2fv")) return HostResult::Busy;
    glUniform2fv(location, static_cast<GLsizei>(count), values);
    return HostResult::Ok;
}
HostResult renderUniform3fv(void*, std::int32_t location, std::int32_t count,
                            const float* values) {
    if (!HostRuntime::instance().onGlThread("renderUniform3fv")) return HostResult::Busy;
    glUniform3fv(location, static_cast<GLsizei>(count), values);
    return HostResult::Ok;
}
HostResult renderUniformMatrix4(void*, std::int32_t location, std::uint32_t count,
                                const float* values) {
    if (!HostRuntime::instance().onGlThread("renderUniformMatrix4")) return HostResult::Busy;
    if (!values) return HostResult::InvalidArgument;
    glUniformMatrix4fv(location, static_cast<GLsizei>(count), GL_FALSE, values);
    return HostResult::Ok;
}

std::uint32_t renderCreateShader(void*, mimita::render::ShaderStage stage) {
    if (!HostRuntime::instance().onGlThread("renderCreateShader")) return 0;
    return static_cast<std::uint32_t>(glCreateShader(toGl(stage)));
}

HostResult renderShaderSource(void*, std::uint32_t shader, const char* source) {
    if (!HostRuntime::instance().onGlThread("renderShaderSource")) return HostResult::Busy;
    if (!source) return HostResult::InvalidArgument;
    const GLchar* sources[1] = {source};
    glShaderSource(static_cast<GLuint>(shader), 1, sources, nullptr);
    return HostResult::Ok;
}

HostResult renderCompileShader(void*, std::uint32_t shader) {
    if (!HostRuntime::instance().onGlThread("renderCompileShader")) return HostResult::Busy;
    glCompileShader(static_cast<GLuint>(shader));
    return HostResult::Ok;
}

std::uint32_t renderCreateProgram(void*) {
    if (!HostRuntime::instance().onGlThread("renderCreateProgram")) return 0;
    return static_cast<std::uint32_t>(glCreateProgram());
}

HostResult renderAttachShader(void*, std::uint32_t program, std::uint32_t shader) {
    if (!HostRuntime::instance().onGlThread("renderAttachShader")) return HostResult::Busy;
    glAttachShader(static_cast<GLuint>(program), static_cast<GLuint>(shader));
    return HostResult::Ok;
}

HostResult renderLinkProgram(void*, std::uint32_t program) {
    if (!HostRuntime::instance().onGlThread("renderLinkProgram")) return HostResult::Busy;
    glLinkProgram(static_cast<GLuint>(program));
    return HostResult::Ok;
}

std::int32_t renderGetUniformLocation(void*, std::uint32_t program,
                                      const char* name) {
    if (!HostRuntime::instance().onGlThread("renderGetUniformLocation")) return -1;
    if (!name) return -1;
    return static_cast<std::int32_t>(
        glGetUniformLocation(static_cast<GLuint>(program), name));
}

HostResult renderGetUniformiv(void*, std::uint32_t program,
                              std::int32_t location, std::int32_t* values,
                              std::int32_t count) {
    if (!HostRuntime::instance().onGlThread("renderGetUniformiv")) return HostResult::Busy;
    if (!values || count <= 0) return HostResult::InvalidArgument;
    glGetUniformiv(static_cast<GLuint>(program), location,
                   reinterpret_cast<GLint*>(values));
    return HostResult::Ok;
}

HostResult renderGetProgramiv(void*, std::uint32_t program,
                              mimita::render::ShaderParameter parameter,
                              std::int32_t* value) {
    if (!HostRuntime::instance().onGlThread("renderGetProgramiv")) return HostResult::Busy;
    if (!value) return HostResult::InvalidArgument;
    glGetProgramiv(static_cast<GLuint>(program), toGl(parameter),
                   reinterpret_cast<GLint*>(value));
    return HostResult::Ok;
}

HostResult renderGetShaderiv(void*, std::uint32_t shader,
                             mimita::render::ShaderParameter parameter,
                             std::int32_t* value) {
    if (!HostRuntime::instance().onGlThread("renderGetShaderiv")) return HostResult::Busy;
    if (!value) return HostResult::InvalidArgument;
    glGetShaderiv(static_cast<GLuint>(shader), toGl(parameter),
                  reinterpret_cast<GLint*>(value));
    return HostResult::Ok;
}

HostResult renderGetProgramInfoLog(void*, std::uint32_t program,
                                   std::int32_t capacity, std::int32_t* length,
                                   char* log) {
    if (!HostRuntime::instance().onGlThread("renderGetProgramInfoLog")) return HostResult::Busy;
    if (!log || capacity <= 0) return HostResult::InvalidArgument;
    glGetProgramInfoLog(static_cast<GLuint>(program),
                        static_cast<GLsizei>(capacity),
                        reinterpret_cast<GLsizei*>(length), log);
    return HostResult::Ok;
}

HostResult renderGetShaderInfoLog(void*, std::uint32_t shader,
                                  std::int32_t capacity, std::int32_t* length,
                                  char* log) {
    if (!HostRuntime::instance().onGlThread("renderGetShaderInfoLog")) return HostResult::Busy;
    if (!log || capacity <= 0) return HostResult::InvalidArgument;
    glGetShaderInfoLog(static_cast<GLuint>(shader),
                       static_cast<GLsizei>(capacity),
                       reinterpret_cast<GLsizei*>(length), log);
    return HostResult::Ok;
}

std::uint32_t renderGetError(void*) {
    if (!HostRuntime::instance().onGlThread("renderGetError")) return 0;
    return static_cast<std::uint32_t>(glGetError());
}

HostResult renderFramebufferTexture2D(
    void*, mimita::render::TextureTarget target,
    mimita::render::FramebufferAttachment attachment, std::uint64_t texture) {
    if (!HostRuntime::instance().onGlThread("renderFramebufferTexture2D")) return HostResult::Busy;
    glFramebufferTexture2D(GL_FRAMEBUFFER, toGl(attachment), toGl(target),
                           static_cast<GLuint>(texture), 0);
    return HostResult::Ok;
}

HostResult renderBindRenderbuffer(void*, std::uint32_t renderbuffer) {
    if (!HostRuntime::instance().onGlThread("renderBindRenderbuffer")) return HostResult::Busy;
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer));
    return HostResult::Ok;
}

HostResult renderRenderbufferStorage(void*,
                                     mimita::render::RenderbufferFormat format,
                                     std::int32_t width, std::int32_t height) {
    if (!HostRuntime::instance().onGlThread("renderRenderbufferStorage")) return HostResult::Busy;
    glRenderbufferStorage(GL_RENDERBUFFER, toGl(format), width, height);
    return HostResult::Ok;
}

HostResult renderFramebufferRenderbuffer(
    void*, mimita::render::FramebufferAttachment attachment,
    std::uint32_t renderbuffer) {
    if (!HostRuntime::instance().onGlThread("renderFramebufferRenderbuffer")) return HostResult::Busy;
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, toGl(attachment), GL_RENDERBUFFER,
                              static_cast<GLuint>(renderbuffer));
    return HostResult::Ok;
}

std::int32_t renderCheckFramebufferStatus(void*) {
    if (!HostRuntime::instance().onGlThread("renderCheckFramebufferStatus")) return 0;
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
               ? 1
               : 0;
}

HostResult renderReadPixels(void*, std::int32_t x, std::int32_t y,
                            std::int32_t width, std::int32_t height,
                            mimita::render::PixelFormat format,
                            mimita::render::PixelType type, void* data,
                            std::uint32_t) {
    if (!HostRuntime::instance().onGlThread("renderReadPixels")) return HostResult::Busy;
    glReadPixels(x, y, width, height, toGl(format), toGl(type), data);
    return HostResult::Ok;
}

HostResult renderBlitFramebuffer(
    void*, std::int32_t srcX0, std::int32_t srcY0, std::int32_t srcX1,
    std::int32_t srcY1, std::int32_t dstX0, std::int32_t dstY0,
    std::int32_t dstX1, std::int32_t dstY1, mimita::render::BlitMask mask,
    mimita::render::BlitFilter filter) {
    if (!HostRuntime::instance().onGlThread("renderBlitFramebuffer")) return HostResult::Busy;
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1,
                      toGl(mask), toGl(filter));
    return HostResult::Ok;
}

HostResult renderTextureAnisotropy(void*, mimita::render::TextureTarget target,
                                   float requested) {
    if (!HostRuntime::instance().onGlThread("renderTextureAnisotropy")) return HostResult::Busy;
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#ifdef GLAD_GL_EXT_texture_filter_anisotropic
    if (!GLAD_GL_EXT_texture_filter_anisotropic)
        return HostResult::Unsupported;
#endif
    static float maxAnisotropy = 0.0f;
    if (maxAnisotropy <= 0.0f) {
        maxAnisotropy = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
    }
    const float level = requested < 1.0f
                            ? 1.0f
                            : (requested > maxAnisotropy ? maxAnisotropy
                                                         : requested);
    glTexParameterf(toGl(target), GL_TEXTURE_MAX_ANISOTROPY_EXT, level);
    return HostResult::Ok;
}

HostResult renderGetViewport(void*, std::int32_t out[4]) {
    if (!HostRuntime::instance().onGlThread("renderGetViewport")) return HostResult::Busy;
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    for (int i = 0; i < 4; ++i)
        out[i] = static_cast<std::int32_t>(viewport[i]);
    return HostResult::Ok;
}

HostResult renderTexSubImage2D(void*, mimita::render::TextureTarget target,
                               std::int32_t level, std::int32_t x,
                               std::int32_t y, std::int32_t width,
                               std::int32_t height,
                               mimita::render::PixelFormat format,
                               mimita::render::PixelType type, const void* data,
                               std::uint32_t) {
    if (!HostRuntime::instance().onGlThread("renderTexSubImage2D")) return HostResult::Busy;
    glTexSubImage2D(toGl(target), level, x, y, width, height, toGl(format),
                    toGl(type), data);
    return HostResult::Ok;
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
    if (!StructuredLogger::instance().shouldLog(StructuredCategory::General,
                                                StructuredLevel::Important)) {
        return;
    }
    StructuredLogger::instance().writeEvent(
        StructuredCategory::General, StructuredLevel::Important, eventName, "", reason,
        tick, fields, __FILE__, __LINE__, __FUNCTION__);
}

}  // namespace

HostRuntime& HostRuntime::instance() {
    static HostRuntime runtime;
    return runtime;
}

bool HostRuntime::hotnessStrictMode() const {
    const char* value = std::getenv("MIMITA_HOTNESS_STRICT");
    return !value || std::strcmp(value, "0") != 0;
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

void* HostRuntime::ensureStateObjectMigrating(
    std::uint32_t id, std::uint32_t size, std::uint32_t alignment,
    mimita::host::StateMigrateFn migrate, std::int32_t* created) {
    if (created) *created = 0;
    if (size == 0 || alignment == 0 ||
        (alignment & (alignment - 1)) != 0) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(stateObjectMutex_);
    StateObjectRecord& record = stateObjects_[id];
    if (record.storage) {
        if (record.size == size && record.alignment == alignment) {
            return record.storage;
        }
        if (!migrate) {
            return nullptr;
        }
        void* next = _aligned_malloc(size, alignment);
        if (!next) return nullptr;
        std::memset(next, 0, size);
        if (!migrate(record.storage, record.size, next, size)) {
            _aligned_free(next);
            return nullptr;
        }
        _aligned_free(record.storage);
        record.storage = next;
        record.size = size;
        record.alignment = alignment;
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

void HostRuntime::markGlThread() {
    glThreadId_ = std::this_thread::get_id();
    glThreadKnown_.store(true, std::memory_order_release);
}

bool HostRuntime::onGlThread(const char* op) {
    if (!glThreadKnown_.load(std::memory_order_acquire)) {
        // Refuse until the owner thread is known. startFromEnvironment and
        // bootstrapGame mark it before any generation work can run.
        return false;
    }
    if (std::this_thread::get_id() == glThreadId_) {
        return true;
    }
    void* frames[16] = {};
    const USHORT frameCount = CaptureStackBackTrace(1, 16, frames, nullptr);
    nlohmann::json stack = nlohmann::json::array();
    for (USHORT index = 0; index < frameCount; ++index) {
        stack.push_back(reinterpret_cast<std::uintptr_t>(frames[index]));
    }
    emitProbeEvent("game.render.thread-violation", op ? op : "(unknown)", 0,
                   nlohmann::json{{"op", op ? op : ""},
                                  {"action", "refused"},
                                  {"thread", "worker"},
                                  {"stack", stack},
                                  {"exe_base", reinterpret_cast<std::uintptr_t>(
                                                   GetModuleHandle(nullptr))},
                                  {"dll_base", reinterpret_cast<std::uintptr_t>(
                                                   loader_.activeGenerationInfo().module)}});
    return false;
}

void HostRuntime::deferGlDelete(std::uint32_t kind, std::uint32_t id) {
    std::lock_guard<std::mutex> lock(glDeleteMutex_);
    deferredGlDeletes_.push_back(DeferredGlDelete{kind, id});
}

void HostRuntime::processDeferredGlDeletes() {
    if (!gRenderDevice) {
        return;
    }
    std::vector<DeferredGlDelete> pending;
    {
        std::lock_guard<std::mutex> lock(glDeleteMutex_);
        pending.swap(deferredGlDeletes_);
    }
    for (const DeferredGlDelete& item : pending) {
        GLuint value = static_cast<GLuint>(item.id);
        switch (item.kind) {
            case 1: glDeleteTextures(1, &value); break;
            case 2: glDeleteBuffers(1, &value); break;
            case 3: glDeleteVertexArrays(1, &value); break;
            case 4: glDeleteFramebuffers(1, &value); break;
            case 5: glDeleteRenderbuffers(1, &value); break;
            case 6: glDeleteQueries(1, &value); break;
            case 7: glDeleteShader(value); break;
            case 8: glDeleteProgram(value); break;
            default: break;
        }
    }
}

bool HostRuntime::startFromEnvironment() {
    if (started_) {
        return probeEnabled_.load(std::memory_order_acquire);
    }

    // Hot by default: load the newest built generation. MIMITA_GAME_DISABLE=1
    // keeps the resident EXE path for debugging or bisecting.
    const char* disabled = std::getenv("MIMITA_GAME_DISABLE");
    if (disabled && std::strcmp(disabled, "1") == 0) {
        return false;
    }

    // The full-client path bootstraps the game generation before the legacy
    // gameInit runs. Initialize the resident logger first so the client writes
    // its own canonical journal and the dev loop can observe logger.started.
    StructuredLogger::instance().init();

    const char* readyPath = std::getenv("MIMITA_GAME_READY");
    readyPath_ = (readyPath && *readyPath) ? readyPath : "build/game/ready.json";

    std::string dllPath;
    std::uint64_t dllGeneration = 0;
    if (const char* configured = std::getenv("MIMITA_GAME_DLL")) {
        dllPath = configured;
    } else {
        std::ifstream current("build/game/current.json");
        if (current.is_open()) {
            try {
                nlohmann::json metadata;
                current >> metadata;
                dllPath = metadata.value("dll", std::string());
                dllGeneration = metadata.value("generation", std::uint64_t{0});
            } catch (const std::exception&) {
                dllPath.clear();
            }
        }
    }
    if (dllPath.empty() || !std::filesystem::exists(dllPath)) {
        emitProbeEvent("game.host.inactive", "no built generation", 0,
                       nlohmann::json{{"env", "MIMITA_GAME_DLL"},
                                      {"fallback", "resident-exe"}});
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

    // The resident EXE and the game DLL must resolve the same resident state
    // objects. Initialize the EXE-side bridge with the same service table.
    mimita::game::initializeHostState(&services_);

    const std::wstring widePath = widenUtf8(dllPath.c_str());
    if (widePath.empty()) {
        emitProbeEvent("game.host.probe.failed", "invalid dll path", 0,
                       nlohmann::json{{"path", dllPath}});
        return false;
    }

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
    activePublishedGeneration_ = dllGeneration != 0 ? dllGeneration : generation;
    probeEnabled_.store(true, std::memory_order_release);
    started_ = true;

    // The host loop and the window/GL context are owned by this thread. Mark it
    // before any generation initialization can spawn workers and call the
    // render capability, so a worker GL call is refused instead of racing the
    // main thread inside the driver.
    markGlThread();

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
    if (enabled && std::strcmp(enabled, "0") == 0) return false;
    const GameModuleV1* api = loader_.active();
    return api && api->frameCapable != 0 && api->tick && api->render;
}

bool HostRuntime::bootstrapGame(const GameStartupV1& startup) {
    markGlThread();
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
    glfwSetCursorPosCallback(window, &HostRuntime::inputCursorCallback);
    glfwSetScrollCallback(window, &HostRuntime::inputScrollCallback);
    glfwSetDropCallback(window, &HostRuntime::inputDropCallback);
}

void HostRuntime::enqueueInput(const GameInputEventV1& event) {
    std::lock_guard<std::mutex> lock(inputMutex_);
    // Mouse movement is high-frequency. Keep only the newest cursor position
    // between dispatches; key/button events must remain ordered.
    if (event.kind == GameInputKind::Cursor && !inputQueue_.empty() &&
        inputQueue_.back().kind == GameInputKind::Cursor) {
        inputQueue_.back() = event;
        return;
    }
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

void HostRuntime::inputCursorCallback(GLFWwindow*, double x, double y) {
    GameInputEventV1 event{};
    event.version = 2;
    event.structSize = static_cast<std::uint32_t>(sizeof(event));
    event.kind = GameInputKind::Cursor;
    event.cursorX = x;
    event.cursorY = y;
    instance().enqueueInput(event);
}

void HostRuntime::inputKeyCallback(GLFWwindow*, int key, int scancode,
                                   int action, int mods) {
    GameInputEventV1 event{};
    event.version = 2;
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
    event.version = 2;
    event.structSize = static_cast<std::uint32_t>(sizeof(event));
    event.kind = GameInputKind::Char;
    event.codepoint = codepoint;
    instance().enqueueInput(event);
}

void HostRuntime::inputScrollCallback(GLFWwindow*, double xOffset,
                                      double yOffset) {
    GameInputEventV1 event{};
    event.version = 2;
    event.structSize = static_cast<std::uint32_t>(sizeof(event));
    event.kind = GameInputKind::Scroll;
    event.scrollX = xOffset;
    event.scrollY = yOffset;
    instance().enqueueInput(event);
}

void HostRuntime::inputDropCallback(GLFWwindow*, int count, const char** paths) {
    GameInputEventV1 event{};
    event.version = 2;
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

void HostRuntime::queueStartupConnect(const StartupConnectV1& request) {
    std::lock_guard<std::mutex> lock(startupMutex_);
    startupQueue_.push_back(request);
}

void HostRuntime::dispatchQueuedStartup() {
    std::deque<StartupConnectV1> pending;
    {
        std::lock_guard<std::mutex> lock(startupMutex_);
        if (startupQueue_.empty()) return;
        pending.swap(startupQueue_);
    }
    if (!probeEnabled_.load(std::memory_order_acquire)) return;
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) return;
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) return;
    for (StartupConnectV1& request : pending) {
        std::int32_t handled = 0;
        api->on_event(kEventStartupConnect, &request, sizeof(request), &handled);
    }
    barrier_.leave();
}

HostResult HostRuntime::driveFrame(double deltaSeconds, void* renderContext) {
    processDeferredGlDeletes();
    dispatchQueuedStartup();
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

bool HostRuntime::invokeMapEditorHost(MapEditorHostCallV1* call) {
    if (!probeEnabled_.load(std::memory_order_acquire) || !call) return false;
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) return false;
    call->version = kMapEditorHostVersion;
    call->structSize = static_cast<std::uint32_t>(sizeof(MapEditorHostCallV1));
    call->result = 0;
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventMapEditorHost, call, sizeof(MapEditorHostCallV1), &handled);
    return result == HostResult::Ok && handled != 0 && call->result != 0;
}

bool HostRuntime::invokeEffectsHost(EffectsHostCallV1* call) {
    if (!probeEnabled_.load(std::memory_order_acquire) || !call) return false;
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) return false;
    call->version = kEffectsHostVersion;
    call->structSize = static_cast<std::uint32_t>(sizeof(EffectsHostCallV1));
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventEffectsHost, call, sizeof(EffectsHostCallV1), &handled);
    return result == HostResult::Ok && handled != 0;
}

bool HostRuntime::dispatchAgentCommand(const AgentCommandV1& command) {
    if (!probeEnabled_.load(std::memory_order_acquire)) return false;
    const GameModuleV1* api = loader_.active();
    if (!api || !api->on_event) return false;
    const std::uint64_t generation = loader_.activeGeneration();
    if (!barrier_.enter(generation)) return false;
    std::int32_t handled = 0;
    const HostResult result = api->on_event(
        kEventAgentCommand, const_cast<AgentCommandV1*>(&command),
        sizeof(command), &handled);
    barrier_.leave();
    return result == HostResult::Ok && handled != 0;
}

bool HostRuntime::serverApiAvailable() const {
    return probeEnabled_.load(std::memory_order_acquire) &&
           loader_.activeGenerationInfo().serverApi != nullptr;
}

namespace {
struct ServerApiCall {
    mimita::host::HostRuntime& runtime;
    mimita::host::GenerationBarrier& barrier;
    const mimita::host::GameServerV1* api;
    std::unique_lock<std::mutex> lock;
    std::uint64_t generation = 0;
    bool entered = false;

    ServerApiCall(mimita::host::HostRuntime& host,
                  std::mutex& mutex,
                  mimita::host::GenerationBarrier& generationBarrier)
        : runtime(host), barrier(generationBarrier),
          api(host.activeServerApi()), lock(mutex) {
        if (!api) {
            return;
        }
        generation = runtime.activeGeneration();
        entered = barrier.enter(generation);
    }

    ~ServerApiCall() {
        if (entered) {
            barrier.leave();
        }
    }

    bool valid() const { return api != nullptr && entered; }
};
}  // namespace

bool HostRuntime::serverProcessPacket(ServerFrameV1* frame, const void* event,
                                      ServerPacketResultV1* out) {
    if (!frame || !event || !out ||
        !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerApiCall call(*this, probeMutex_, barrier_);
    if (!call.valid()) {
        return false;
    }
    return call.api->process_packet(frame, event, out) != 0;
}

bool HostRuntime::serverSimulatePlayers(ServerFrameV1* frame) {
    if (!frame || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerApiCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->simulate_players(frame) != 0;
}

bool HostRuntime::serverSimulateWorld(ServerFrameV1* frame) {
    if (!frame || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerApiCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->simulate_world(frame) != 0;
}

bool HostRuntime::serverPublish(ServerFrameV1* frame) {
    if (!frame || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerApiCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->publish(frame) != 0;
}

bool HostRuntime::serverGamemode(ServerFrameV1* frame) {
    if (!frame || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerApiCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->gamemode(frame) != 0;
}

namespace {
struct ServerPolicyCall {
    mimita::host::HostRuntime& runtime;
    mimita::host::GenerationBarrier& barrier;
    const mimita::host::GameServerPolicyV1* api;
    std::unique_lock<std::mutex> lock;
    std::uint64_t generation = 0;
    bool entered = false;

    ServerPolicyCall(mimita::host::HostRuntime& host,
                     std::mutex& mutex,
                     mimita::host::GenerationBarrier& generationBarrier)
        : runtime(host), barrier(generationBarrier),
          api(host.activeServerPolicy()), lock(mutex) {
        if (!api) {
            return;
        }
        generation = runtime.activeGeneration();
        entered = barrier.enter(generation);
    }

    ~ServerPolicyCall() {
        if (entered) {
            barrier.leave();
        }
    }

    bool valid() const { return api != nullptr && entered; }
};
}  // namespace

bool HostRuntime::serverPolicyAvailable() const {
    return probeEnabled_.load(std::memory_order_acquire) &&
           loader_.activeGenerationInfo().serverPolicy != nullptr;
}

bool HostRuntime::serverPolicyStartup(const ServerStartupV1* startup,
                                      ServerStartupResultV1* result) {
    if (!startup || !result || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->startup(startup, result) != 0;
}

bool HostRuntime::serverPolicyWorldLoad(const ServerWorldLoadV1* load,
                                        ServerWorldLoadResultV1* result) {
    if (!load || !result || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->world_load(load, result) != 0;
}

bool HostRuntime::serverPolicyConfigure(const ServerConfigureV1* configure,
                                        ServerConfigureResultV1* result) {
    if (!configure || !result ||
        !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->configure(configure, result) != 0;
}

void* HostRuntime::serverPolicyCreatePlayer() {
    if (!probeEnabled_.load(std::memory_order_acquire)) {
        return nullptr;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() ? call.api->create_player() : nullptr;
}

void HostRuntime::serverPolicyDestroyPlayer(void* player) {
    if (!player || !probeEnabled_.load(std::memory_order_acquire)) {
        return;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    if (call.valid()) {
        call.api->destroy_player(player);
    }
}

bool HostRuntime::serverPolicyMakeUdpConnectionId(
    std::uint32_t addressNetwork, std::uint16_t portNetwork,
    ServerConnectionIdV1* out) {
    if (!out || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() &&
           call.api->make_udp_connection_id(addressNetwork, portNetwork, out) != 0;
}

bool HostRuntime::serverPolicyAllocateIceConnectionId(
    ServerConnectionIdV1* out) {
    if (!out || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->allocate_ice_connection_id(out) != 0;
}

bool HostRuntime::serverPolicyLegacyEndpoint(
    const ServerConnectionIdV1* id, std::uint32_t* addressNetwork,
    std::uint16_t* portNetwork) {
    if (!id || !addressNetwork || !portNetwork ||
        !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() &&
           call.api->legacy_endpoint_for_connection(id, addressNetwork,
                                                    portNetwork) != 0;
}

bool HostRuntime::serverPolicyClearActorPreset(std::uint32_t playerId) {
    if (!probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->clear_actor_preset(playerId) != 0;
}

bool HostRuntime::serverPolicyPollReload(void* npcSystem, std::uint32_t tick) {
    if (!probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->poll_reload(npcSystem, tick) != 0;
}

bool HostRuntime::serverPolicyAuthIdentity(ServerAuthIdentityV1* out) {
    if (!out || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->auth_identity(out) != 0;
}

bool HostRuntime::serverPolicyProjectilePerf(ServerProjectilePerfV1* out) {
    if (!out || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->projectile_perf(out) != 0;
}

bool HostRuntime::serverPolicyClearRuntimeObjects(const char* reason) {
    if (!probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->clear_runtime_objects(reason) != 0;
}

bool HostRuntime::serverPolicyCoordinatorIceDone(const char* roomCode) {
    if (!probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    ServerPolicyCall call(*this, probeMutex_, barrier_);
    return call.valid() && call.api->coordinator_ice_done(roomCode) != 0;
}

bool HostRuntime::gameExportAvailable() const {
    return probeEnabled_.load(std::memory_order_acquire) &&
           loader_.activeGenerationInfo().exportApi != nullptr;
}

bool HostRuntime::moduleHostQueryAvailable() const {
    return probeEnabled_.load(std::memory_order_acquire) &&
           loader_.activeGenerationInfo().moduleHostQuery != nullptr;
}

std::uint64_t HostRuntime::moduleHostMapRevision() {
    const GameModuleHostQueryV1* api =
        loader_.activeGenerationInfo().moduleHostQuery;
    if (!api || !probeEnabled_.load(std::memory_order_acquire)) {
        return 0;
    }
    const std::uint64_t generation = activeGeneration();
    if (!barrier_.enter(generation)) {
        return 0;
    }
    const std::uint64_t revision = api->map_revision();
    barrier_.leave();
    return revision;
}

const void* HostRuntime::moduleHostMapObjectiveConfig() {
    const GameModuleHostQueryV1* api =
        loader_.activeGenerationInfo().moduleHostQuery;
    if (!api || !probeEnabled_.load(std::memory_order_acquire)) {
        return nullptr;
    }
    const std::uint64_t generation = activeGeneration();
    if (!barrier_.enter(generation)) {
        return nullptr;
    }
    const void* config = api->map_objective_config();
    barrier_.leave();
    return config;
}

bool HostRuntime::moduleHostProfileFootprint(float outFootprint[3]) {
    const GameModuleHostQueryV1* api =
        loader_.activeGenerationInfo().moduleHostQuery;
    if (!api || !probeEnabled_.load(std::memory_order_acquire)) {
        return false;
    }
    const std::uint64_t generation = activeGeneration();
    if (!barrier_.enter(generation)) {
        return false;
    }
    const bool ok = api->profile_footprint(outFootprint) != 0;
    barrier_.leave();
    return ok;
}

int HostRuntime::moduleHostNpcLineOfSightBlocked(const float from[3],
                                                 const float to[3]) {
    const GameModuleHostQueryV1* api =
        loader_.activeGenerationInfo().moduleHostQuery;
    if (!api || !probeEnabled_.load(std::memory_order_acquire)) {
        return 0;
    }
    const std::uint64_t generation = activeGeneration();
    if (!barrier_.enter(generation)) {
        return 0;
    }
    const int blocked = api->npc_line_of_sight_blocked(from, to);
    barrier_.leave();
    return blocked;
}

int HostRuntime::moduleHostNpcRaycastBlocked(const float from[3],
                                             const float direction[3],
                                             float maxDistance,
                                             float* outDistance) {
    const GameModuleHostQueryV1* api =
        loader_.activeGenerationInfo().moduleHostQuery;
    if (!api || !probeEnabled_.load(std::memory_order_acquire)) {
        return 0;
    }
    const std::uint64_t generation = activeGeneration();
    if (!barrier_.enter(generation)) {
        return 0;
    }
    const int blocked = api->npc_raycast_blocked(
        from, direction, maxDistance, outDistance);
    barrier_.leave();
    return blocked;
}

void HostRuntime::moduleHostSetNpcWorld(const void* world) {
    const GameModuleHostQueryV1* api =
        loader_.activeGenerationInfo().moduleHostQuery;
    if (!api || !probeEnabled_.load(std::memory_order_acquire)) {
        return;
    }
    const std::uint64_t generation = activeGeneration();
    if (!barrier_.enter(generation)) {
        return;
    }
    api->set_npc_world(world);
    barrier_.leave();
}

int HostRuntime::gameRunExport(const char* clipPath, const char* outputPath,
                               int width, int height, int visible,
                               int verbose) {
    const GameExportV1* api = loader_.activeGenerationInfo().exportApi;
    if (!api || !probeEnabled_.load(std::memory_order_acquire)) {
        return 1;
    }
    const std::uint64_t generation = activeGeneration();
    if (!barrier_.enter(generation)) {
        return 1;
    }
    const int result = api->run_export(clipPath, outputPath, width, height,
                                       visible, verbose);
    barrier_.leave();
    return result;
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
    const std::uint64_t generation = ready.value("generation", std::uint64_t{0});
    const std::string dll = ready.value("dll", std::string());
    if (generation == 0 || dll.empty() ||
        generation == activePublishedGeneration_) {
        return;
    }
    {
        std::error_code pathError;
        const auto candidatePath = std::filesystem::absolute(dll, pathError);
        const auto activePath = std::filesystem::path(
            loader_.activeGenerationInfo().path);
        if (!pathError && !activePath.empty() &&
            std::filesystem::equivalent(candidatePath, activePath, pathError) &&
            !pathError) {
            // The manifest may name the same DLL with a different generation
            // label. Reloading it would shut down the active generation and
            // leave its DLL statics initialized, so only update the identity.
            activePublishedGeneration_ = generation;
            lastReadyGeneration_ = generation;
            return;
        }
    }
    if (generation == lastFailedGeneration_ && dll == lastFailedDll_ &&
        now - lastFailedAt_ < std::chrono::seconds(5)) {
        return;
    }
    if (!std::filesystem::exists(dll)) {
        lastFailedGeneration_ = generation;
        lastFailedDll_ = dll;
        lastFailedAt_ = now;
        emitProbeEvent("game.generation.activate-failed", "candidate missing", 0,
                       nlohmann::json{{"generation", generation}, {"dll", dll}});
        return;
    }

    const std::uint64_t oldGeneration = loader_.activeGeneration();
    barrier_.beginQuiesce(QuiesceMode::Swap);
    const HostResult quiesced = barrier_.finishQuiesce(2000);
    if (quiesced != HostResult::Ok) {
        emitProbeEvent("game.generation.swap-refused", "generation busy", 0,
                       nlohmann::json{{"generation", generation},
                                      {"result", static_cast<int>(quiesced)}});
        barrier_.resumeGeneration();
        return;
    }

    LoadedGeneration candidate;
    const HostResult loaded = loader_.loadCandidate(
        widenUtf8(dll.c_str()), &services_, &candidate);
    if (loaded != HostResult::Ok) {
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
        oldApi->quiesce(QuiesceMode::Swap);
    }
    if (oldApi && oldApi->shutdown) {
        oldApi->shutdown();
    }
    // The ready manifest is the generation identity for the swap; a module may
    // hardcode a local version field that is not the build generation.
    candidate.generation = generation;
    loader_.commit(std::move(candidate));
    activePublishedGeneration_ = generation;
    lastReadyGeneration_ = generation;
    lastFailedGeneration_ = 0;
    lastFailedDll_.clear();
    barrier_.beginGeneration(generation);
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
            &glQueryCounterTimestamp, &glQueryResultAvailable,
            &glQueryResultU64,
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
            &renderDeviceSetCursorVisible,
        };
        return &renderDeviceCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Window) &&
        version == kWindowCapabilityVersion) {
        static WindowCapabilityV1 windowCapability{
            kWindowCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(WindowCapabilityV1)),
            &windowGetSize,
            &windowGetFramebufferSize,
            &windowGetPosition,
            &windowGetCursorPos,
            &windowSetCursorPos,
            &windowFocus,
            &windowGetCursorMode,
            &windowGetFocused,
            &windowGetIconified,
            &windowSetCursorMode,
            &windowMouseButtonDown,
            &windowKeyDown,
            &windowClipboardGet,
            &windowClipboardSet,
        };
        return &windowCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::EntityDomain) &&
        version == kEntityDomainCapabilityVersion) {
        static EntityDomainCapabilityV1 domainCapability{
            kEntityDomainCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(EntityDomainCapabilityV1)),
            [](void*, EntityHandle handle,
               mimita::host::state::player_core::Record* out) {
                return HostServerEntityAdapter::instance().readPlayerCore(handle, out);
            },
            [](void*, EntityHandle handle,
               const mimita::host::state::player_core::Record* in) {
                return HostServerEntityAdapter::instance().writePlayerCore(handle, in);
            },
            [](void*, EntityHandle handle,
               mimita::host::state::player_session::Record* out) {
                return HostServerEntityAdapter::instance().readPlayerSession(handle, out);
            },
            [](void*, EntityHandle handle,
               const mimita::host::state::player_session::Record* in) {
                return HostServerEntityAdapter::instance().writePlayerSession(handle, in);
            },
            [](void*, DomainId domain, EntityHandle handle, void* record,
               std::uint32_t recordSize) {
                return HostServerEntityAdapter::instance().readDomain(
                    domain, handle, record, recordSize);
            },
            [](void*, DomainId domain, EntityHandle handle, const void* record,
               std::uint32_t recordSize) {
                return HostServerEntityAdapter::instance().writeDomain(
                    domain, handle, record, recordSize);
            },
        };
        return &domainCapability;
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
            [](void*, mimita::host::StateObjectId id, std::uint32_t size,
                std::uint32_t alignment, mimita::host::StateMigrateFn migrate,
                std::int32_t* created) {
                return HostRuntime::instance().ensureStateObjectMigrating(
                    static_cast<std::uint32_t>(id), size, alignment, migrate,
                    created);
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
            &renderReadPixels,
            &renderBlitFramebuffer,
            &renderTextureAnisotropy,
            &renderGetViewport,
            &renderTexSubImage2D,
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
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Modules) &&
        version == kModuleCapabilityVersion) {
        static ModuleCapabilityV1 moduleCapability{
            kModuleCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(ModuleCapabilityV1)),
            &moduleInvoke,
        };
        return &moduleCapability;
    }
    if (capability ==
        static_cast<std::uint32_t>(HostCapabilityId::EditorServices)) {
        return const_cast<void*>(ModuleHost::instance().capability(
            mimita_module::CAPABILITY_EDITOR, version));
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
    if (!HostRuntime::instance().onGlThread("renderSubmitThunk")) return HostResult::Busy;
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
    runtime->processDeferredGlDeletes();
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
