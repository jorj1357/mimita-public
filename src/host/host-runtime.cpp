// 2026-11-12
// Resident host runtime implementation. Disabled unless MIMITA_GAME_PROBE=1.

#include "host/host-runtime.h"
#include "renderer/renderer.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "debug/crash-handler.h"
#include "debug/structured-log.h"
#include "host/abi/log-capability.h"
#include "host/abi/audio-capability.h"
#include "host/abi/authored-rebuild-capability.h"
#include "host/abi/effects-capability.h"
#include "host/abi/gamemode-capability.h"
#include "host/abi/global-capability.h"
#include "host/abi/ice-capability.h"
#include "host/abi/transport-session-capability.h"
#include "host/abi/diagnostics-capability.h"
#include "host/abi/render-device-capability.h"
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
#include "network/server-authored-rebuild.h"

namespace mimita::host {
namespace {

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

extern void* gpPlayer;
extern void* gpCamera;
extern void* gpWorld;
extern void* gpNpcSystem;
extern void* gpWeapons;
extern void* gpFreecamEnabled;
extern void* gpDeathPosition;
extern void* gpSelectedEditorObject;
extern void* gpEditorMode;
extern void* gpActiveGameMode;
extern void* gpActiveMapPath;
extern void* gpWorldLoaded;
extern void* gpReplayRecorder;
extern void* gpReplayPlayer;
extern void* gpReplayActorModels;
extern void* gpReplayWeaponModels;
extern void* gpReplayFactory;
extern void* gpReplayBrowser;
extern void* gpReplayTimeline;
extern void* gpReplayCameraMgr;
extern void* gpReplayChatStates;
extern void* gpReplayClipsCache;
extern void* gpCommandBinds;
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
        };
        return &entityCapability;
    }
    if (capability == static_cast<std::uint32_t>(HostCapabilityId::Render) &&
        version == kRenderCapabilityVersion) {
        static RenderCapabilityV1 renderCapability{
            kRenderCapabilityVersion,
            static_cast<std::uint32_t>(sizeof(RenderCapabilityV1)),
            &HostRuntime::renderSubmitThunk,
            &HostRuntime::renderFlushThunk,
            &HostRuntime::renderPendingThunk,
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

TransportResult HostRuntime::transportSendThunk(void* userData, PeerHandle,
                                                TransportChannel, std::uint32_t,
                                                std::uint32_t,
                                                const std::uint8_t* payload,
                                                std::uint32_t payloadSize) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !runtime->transportAdapter_ ||
        !runtime->transportAdapter_->send_to_server) {
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

HostResult HostRuntime::renderSubmitThunk(void* userData,
                                           const RenderCommandV1* commands,
                                           std::uint32_t count) {
    auto* runtime = static_cast<HostRuntime*>(userData);
    if (!runtime || !commands || count == 0 ||
        count > kRenderMaxCommandsPerSubmit) {
        return HostResult::InvalidArgument;
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
