// 2026-11-12
// Game-DLL-side partial ModuleHost bridge for CAP_MODULES.
//
// Only the small query/control surface crosses the boundary:
// instance, activeGeneration, module/predicate queries, fixed-tick gate,
// startup/poll/shutdown. Typed policy methods remain UNRESOLVED until their
// operations are added. This bridge owns no module state or module DLLs.

#include <cstring>

#include "host/abi/game-host-abi.h"
#include "host/abi/module-capability.h"
#include "hot-reload/module-host.h"
#include "modules/api/map-editor-api.h"
#include "modules/api/map-geometry-api.h"
#include "modules/api/world-rendering-api.h"
#include "modules/api/module-abi.h"

extern "C" const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaMapEditorModuleAPI(std::uint32_t abiVersion);
extern "C" const mimita_module::BaseV1* MIMITA_MODULE_CALL
GetMimitaMapGeometryModuleAPI(std::uint32_t abiVersion);

namespace mimita::game {

void initializeHostModules(const mimita::host::HostServicesV1* services);

namespace {
const mimita::host::ModuleCapabilityV1* gModules = nullptr;
const mimita::host::HostServicesV1* gHostServices = nullptr;
const mimita_module::ModuleV1* gMapEditor = nullptr;
const mimita_module::ModuleV1* gMapGeometry = nullptr;
const mimita_module::ModuleV1* gWorldRendering = nullptr;
void* gHostData = nullptr;

void MIMITA_MODULE_CALL moduleLog(std::uint32_t, std::uint32_t level,
                                  const char* eventName, const char*,
                                  const char* reason, std::uint32_t,
                                  const char*, const char*, int,
                                  const char*) {
    if (gHostServices && gHostServices->log) {
        gHostServices->log(gHostServices->hostUserData,
                           static_cast<std::int32_t>(level),
                           eventName ? eventName : (reason ? reason : "module"));
    }
}

const void* MIMITA_MODULE_CALL moduleGetCapability(std::uint32_t capability,
                                                   std::uint32_t version) {
    if (!gHostServices || !gHostServices->get_capability) return nullptr;
    if (capability == mimita_module::CAPABILITY_EDITOR) {
        return gHostServices->get_capability(
            gHostServices->hostUserData,
            static_cast<std::uint32_t>(
                mimita::host::HostCapabilityId::EditorServices),
            version);
    }
    return nullptr;
}

std::uint64_t MIMITA_MODULE_CALL moduleRevision() { return 0; }

mimita_module::HostServicesV1 gModuleServices{
    mimita_module::ABI_VERSION,
    static_cast<std::uint32_t>(sizeof(mimita_module::HostServicesV1)),
    &moduleLog,
    &moduleGetCapability,
    &moduleRevision,
};

bool invokeMapEditor(std::uint32_t eventType, void* payload,
                     std::uint32_t payloadSize) {
    if (!gMapEditor || !gMapEditor->onEvent) return false;
    mimita_module::ModuleEventV1 event{};
    event.typeId = eventType;
    event.structSize = sizeof(event);
    event.payload = payload;
    event.payloadSize = payloadSize;
    return gMapEditor->onEvent(&event) != 0;
}

bool invokeMapGeometry(std::uint32_t eventType, void* payload,
                       std::uint32_t payloadSize) {
    if (!gMapGeometry || !gMapGeometry->onEvent) return false;
    mimita_module::ModuleEventV1 event{};
    event.typeId = eventType;
    event.structSize = sizeof(event);
    event.payload = payload;
    event.payloadSize = payloadSize;
    return gMapGeometry->onEvent(&event) != 0;
}

bool invokeWorldRendering(std::uint32_t eventType, void* payload,
                          std::uint32_t payloadSize) {
    if (!gWorldRendering || !gWorldRendering->onEvent) return false;
    mimita_module::ModuleEventV1 event{};
    event.typeId = eventType;
    event.structSize = sizeof(event);
    event.payload = payload;
    event.payloadSize = payloadSize;
    return gWorldRendering->onEvent(&event) != 0;
}

bool invoke(mimita::host::ModuleOp op, const void* input, std::uint32_t inputSize,
            void* output, std::uint32_t outputSize) {
    if (!gModules || !gModules->invoke) {
        return false;
    }
    return gModules->invoke(gHostData, op, input, inputSize, output, outputSize) != 0;
}
bool forward(mimita::host::ModuleOp op, const void* input, std::uint32_t inputSize) {
    return invoke(op, input, inputSize, nullptr, 0);
}

}  // namespace

void initializeHostModules(const mimita::host::HostServicesV1* services) {
    gModules = nullptr;
    gHostServices = services;
    gMapEditor = nullptr;
    gMapGeometry = nullptr;
    gWorldRendering = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    const auto initializeModule = [&](const mimita_module::BaseV1* base,
                                      const char* name)
        -> const mimita_module::ModuleV1* {
        if (!base || base->structSize < sizeof(mimita_module::ModuleV1) ||
            !base->name || std::strcmp(base->name, name) != 0) {
            return nullptr;
        }
        const auto* module =
            reinterpret_cast<const mimita_module::ModuleV1*>(base);
        return module->initialize && module->initialize(&gModuleServices)
            ? module : nullptr;
    };
    gMapEditor = initializeModule(
        GetMimitaMapEditorModuleAPI(mimita_module::ABI_VERSION), "map-editor");
    gMapGeometry = initializeModule(
        GetMimitaMapGeometryModuleAPI(mimita_module::ABI_VERSION),
        "map-geometry");
    auto* capability = static_cast<const mimita::host::ModuleCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Modules),
            mimita::host::kModuleCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kModuleCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::ModuleCapabilityV1)) {
        gModules = capability;
    }
}

}  // namespace mimita::game

ModuleHost& ModuleHost::instance() {
    static ModuleHost host;
    return host;
}

ModuleHost::~ModuleHost() = default;

void ModuleHost::startup() {
    // The resident host owns module startup. Forward the request instead of
    // starting a second module set in the game generation.
    mimita::game::invoke(mimita::host::ModuleOp::Startup, nullptr, 0, nullptr, 0);
}

void ModuleHost::shutdown() {}

void ModuleHost::poll() {
    mimita::game::invoke(mimita::host::ModuleOp::Poll, nullptr, 0, nullptr, 0);
}

void ModuleHost::beginFixedTick() {
    mFixedTickActive.store(true, std::memory_order_release);
    mimita::game::invoke(mimita::host::ModuleOp::BeginFixedTick, nullptr, 0, nullptr, 0);
}

void ModuleHost::endFixedTick() {
    mFixedTickActive.store(false, std::memory_order_release);
    mimita::game::invoke(mimita::host::ModuleOp::EndFixedTick, nullptr, 0, nullptr, 0);
}

bool ModuleHost::moduleLoaded(const char* moduleName) const {
    if (!moduleName) {
        return false;
    }
    if (std::strcmp(moduleName, "map-editor") == 0) {
        return mimita::game::gMapEditor != nullptr;
    }
    if (std::strcmp(moduleName, "map-geometry") == 0) {
        return mimita::game::gMapGeometry != nullptr;
    }
    std::int32_t loaded = 0;
    mimita::game::invoke(mimita::host::ModuleOp::ModuleLoaded, moduleName,
                         static_cast<std::uint32_t>(std::strlen(moduleName) + 1),
                         &loaded, sizeof(loaded));
    return loaded != 0;
}

bool ModuleHost::mapEditorLoaded() const {
    return mimita::game::gMapEditor != nullptr;
}

bool ModuleHost::npcPerceptionLoaded() const {
    std::int32_t loaded = 0;
    mimita::game::invoke(mimita::host::ModuleOp::NpcPerceptionLoaded, nullptr, 0,
                         &loaded, sizeof(loaded));
    return loaded != 0;
}

bool ModuleHost::npcBehaviorLoaded() const {
    std::int32_t loaded = 0;
    mimita::game::invoke(mimita::host::ModuleOp::NpcBehaviorLoaded, nullptr, 0,
                         &loaded, sizeof(loaded));
    return loaded != 0;
}

std::uint32_t ModuleHost::activeGeneration(const char* moduleName) const {
    if (!moduleName) {
        return 0;
    }
    if (std::strcmp(moduleName, "map-editor") == 0) {
        return mimita::game::gMapEditor
            ? mimita::game::gMapEditor->base.generation : 0;
    }
    if (std::strcmp(moduleName, "map-geometry") == 0) {
        return mimita::game::gMapGeometry
            ? mimita::game::gMapGeometry->base.generation : 0;
    }
    std::uint32_t generation = 0;
    mimita::game::invoke(mimita::host::ModuleOp::ActiveGeneration, moduleName,
                         static_cast<std::uint32_t>(std::strlen(moduleName) + 1),
                         &generation, sizeof(generation));
    return generation;
}

bool ModuleHost::worldgenResolve(mimita_worldgen::ResolveV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::WorldgenResolve, request, 0);
}
bool ModuleHost::navigationPolicy(mimita_navigation_policy::NavigationPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NavigationPolicy, request, 0);
}
bool ModuleHost::collisionActorPairPolicy(mimita_collision_policy::ActorPairPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::CollisionActorPairPolicy, request, 0);
}
bool ModuleHost::mapGeometryNodePolicy(mimita_map_geometry::NodePolicyV1* request) {
    if (request && mimita::game::gMapGeometry) {
        return mimita::game::invokeMapGeometry(
            mimita_module::EVENT_MAP_GEOMETRY_NODE_POLICY, request,
            sizeof(*request)) && request->result != 0;
    }
    return mimita::game::forward(mimita::host::ModuleOp::MapGeometryNodePolicy, request, 0);
}
bool ModuleHost::networkHitClaimPolicy(mimita_network_policy::HitClaimPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkHitClaimPolicy, request, 0);
}
bool ModuleHost::networkMovementValidationPolicy(mimita_network_policy::MovementValidationPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkMovementValidationPolicy, request, 0);
}
bool ModuleHost::npcBehaviorSelfTest(mimita_npc_behavior::SelfTestV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcBehaviorSelfTest, request, 0);
}
bool ModuleHost::npcGrenadeDecision(mimita_npc_behavior::GrenadeDecisionV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcGrenadeDecision, request, 0);
}
bool ModuleHost::npcPerceptionStep(mimita_npc_perception::PerceptionRequestV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcPerceptionStep, request, 0);
}
bool ModuleHost::npcWeaponChoice(mimita_npc_behavior::WeaponChoiceV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcWeaponChoice, request, 0);
}
bool ModuleHost::chunkCollisionBuild(mimita_chunk_collision::BuildChunkV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::ChunkCollisionBuild, request, 0);
}
bool ModuleHost::collisionContactPolicy(mimita_collision_policy::ContactPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::CollisionContactPolicy, request, 0);
}
bool ModuleHost::collisionFinalPosePolicy(mimita_collision_policy::FinalPosePolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::CollisionFinalPosePolicy, request, 0);
}
bool ModuleHost::collisionSolveActorWorld(mimita_collision_policy::ActorWorldSolveV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::CollisionSolveActorWorld, request, 0);
}
bool ModuleHost::collisionSolveContacts(mimita_collision_policy::ContactSolveV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::CollisionSolveContacts, request, 0);
}
bool ModuleHost::collisionSurfacePolicy(mimita_collision_policy::SurfacePolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::CollisionSurfacePolicy, request, 0);
}
bool ModuleHost::mapEditorMapCursor(mimita_map_editor::EditorInputV1* request) {
    if (request && mimita::game::gMapEditor) {
        return mimita::game::invokeMapEditor(
            mimita_module::EVENT_EDITOR_INPUT, request, sizeof(*request)) &&
            request->result != 0;
    }
    return mimita::game::forward(mimita::host::ModuleOp::MapEditorMapCursor, request, 0);
}
bool ModuleHost::mapEditorResolveClick(mimita_map_editor::EditorClickV1* request) {
    if (request && mimita::game::gMapEditor) {
        return mimita::game::invokeMapEditor(
            mimita_module::EVENT_EDITOR_CLICK, request, sizeof(*request)) &&
            request->result != 0;
    }
    return mimita::game::forward(mimita::host::ModuleOp::MapEditorResolveClick, request, 0);
}
bool ModuleHost::mapGeometryObjectPlan(mimita_map_geometry::ObjectPlanV1* request) {
    if (request && mimita::game::gMapGeometry) {
        return mimita::game::invokeMapGeometry(
            mimita_module::EVENT_MAP_GEOMETRY_OBJECT_PLAN, request,
            sizeof(*request)) && request->result != 0;
    }
    return mimita::game::forward(mimita::host::ModuleOp::MapGeometryObjectPlan, request, 0);
}
bool ModuleHost::networkCombatValidationPolicy(mimita_network_policy::CombatValidationPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkCombatValidationPolicy, request, 0);
}
bool ModuleHost::networkInterpolationPolicy(mimita_network_policy::InterpolationPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkInterpolationPolicy, request, 0);
}
bool ModuleHost::networkMapSyncPolicy(mimita_network_policy::MapSyncPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkMapSyncPolicy, request, 0);
}
bool ModuleHost::networkMovementCorrection(mimita_network_policy::MovementCorrectionPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkMovementCorrection, request, 0);
}
bool ModuleHost::networkReconcilePolicy(mimita_network_policy::ReconcilePolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkReconcilePolicy, request, 0);
}
bool ModuleHost::networkReconnectPolicy(mimita_network_policy::ReconnectPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkReconnectPolicy, request, 0);
}
bool ModuleHost::networkSessionHealthPolicy(mimita_network_policy::SessionHealthPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkSessionHealthPolicy, request, 0);
}
bool ModuleHost::networkSnapshotPolicy(mimita_network_policy::SnapshotPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NetworkSnapshotPolicy, request, 0);
}
bool ModuleHost::npcActorState(mimita_npc_behavior::ActorStateV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcActorState, request, 0);
}
bool ModuleHost::npcAimPolicy(mimita_npc_behavior::AimPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcAimPolicy, request, 0);
}
bool ModuleHost::npcAttackIntent(mimita_npc_behavior::AttackIntentV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcAttackIntent, request, 0);
}
bool ModuleHost::npcBehaviorAction(mimita_npc_behavior::ActionV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcBehaviorAction, request, 0);
}
bool ModuleHost::npcBehaviorDecision(mimita_npc_behavior::DecisionV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcBehaviorDecision, request, 0);
}
bool ModuleHost::npcDashDecision(mimita_npc_behavior::DashDecisionV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcDashDecision, request, 0);
}
bool ModuleHost::npcFireGate(mimita_npc_behavior::FireGateV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcFireGate, request, 0);
}
bool ModuleHost::npcGoalScore(mimita_npc_behavior::GoalScoreV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcGoalScore, request, 0);
}
bool ModuleHost::npcPayloadRole(mimita_npc_behavior::PayloadRoleV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcPayloadRole, request, 0);
}
bool ModuleHost::npcStateTiming(mimita_npc_behavior::StateTimingV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcStateTiming, request, 0);
}
bool ModuleHost::npcTargetSelect(mimita_npc_behavior::TargetSelectV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcTargetSelect, request, 0);
}
bool ModuleHost::npcTargetingPolicy(mimita_npc_behavior::TargetingPolicyV1* request) {
    return mimita::game::forward(mimita::host::ModuleOp::NpcTargetingPolicy, request, 0);
}
bool ModuleHost::worldRenderingBuildBox(mimita_world_rendering::BoxMeshV1* request) {
    if (request && mimita::game::gWorldRendering) {
        return mimita::game::invokeWorldRendering(
            mimita_module::EVENT_RENDER_BUILD_BOX, request, sizeof(*request)) &&
            request->result != 0;
    }
    return mimita::game::forward(mimita::host::ModuleOp::WorldRenderingBuildBox, request, 0);
}

bool ModuleHost::invokeEvent(std::uint32_t eventType, void* payload,
                             std::uint32_t payloadSize, std::uint64_t tick) {
    mimita::host::ModuleArgs args{};
    args.u0 = eventType;
    args.ptr0 = payload;
    args.u1 = payloadSize;
    args.u2 = static_cast<std::uint32_t>(tick);
    return mimita::game::invoke(mimita::host::ModuleOp::InvokeEvent, &args,
                                sizeof(args), nullptr, 0);
}

bool ModuleHost::mapEditorComputeDragDelta(
    const mimita_map_editor::DragMathV1* input, float outDelta[3]) {
    if (input && outDelta && mimita::game::gMapEditor) {
        mimita_map_editor::EditorDragDeltaV1 payload{};
        payload.input = *input;
        if (!mimita::game::invokeMapEditor(
                mimita_module::EVENT_EDITOR_DRAG_DELTA, &payload,
                sizeof(payload)) || !payload.result) {
            return false;
        }
        std::memcpy(outDelta, payload.outDelta, sizeof(payload.outDelta));
        return true;
    }
    mimita::host::ModuleArgs args{};
    args.ptr0 = const_cast<mimita_map_editor::DragMathV1*>(input);
    args.ptr1 = outDelta;
    return mimita::game::invoke(mimita::host::ModuleOp::MapEditorComputeDragDelta,
                                &args, sizeof(args), nullptr, 0);
}

bool ModuleHost::mapEditorPickGizmoAxis(
    const mimita_map_editor::GizmoPickV1* input, int* outAxis) {
    if (input && outAxis && mimita::game::gMapEditor) {
        mimita_map_editor::EditorGizmoPickV1 payload{};
        payload.input = *input;
        if (!mimita::game::invokeMapEditor(
                mimita_module::EVENT_EDITOR_GIZMO_PICK, &payload,
                sizeof(payload)) || !payload.result) {
            return false;
        }
        *outAxis = payload.outAxis;
        return true;
    }
    mimita::host::ModuleArgs args{};
    args.ptr0 = const_cast<mimita_map_editor::GizmoPickV1*>(input);
    args.ptr1 = outAxis;
    return mimita::game::invoke(mimita::host::ModuleOp::MapEditorPickGizmoAxis,
                                &args, sizeof(args), nullptr, 0);
}

bool ModuleHost::mapEditorAddChunk(const char* requestedId, char* outId,
                                   std::uint32_t outCapacity) {
    if (mimita::game::gMapEditor) {
        mimita_map_editor::EditorAddChunkV1 payload{};
        std::strncpy(payload.id, requestedId ? requestedId : "",
                     sizeof(payload.id) - 1);
        if (!mimita::game::invokeMapEditor(
                mimita_module::EVENT_EDITOR_ADD_CHUNK, &payload,
                sizeof(payload)) || !payload.result || !outId || outCapacity == 0) {
            return false;
        }
        std::strncpy(outId, payload.outId, outCapacity - 1);
        outId[outCapacity - 1] = 0;
        return true;
    }
    mimita::host::ModuleArgs args{};
    std::strncpy(args.s0, requestedId ? requestedId : "", sizeof(args.s0) - 1);
    args.outCapacity = outCapacity;
    const bool ok = mimita::game::invoke(mimita::host::ModuleOp::MapEditorAddChunk,
                                         &args, sizeof(args), nullptr, 0);
    if (ok && outId && outCapacity) {
        std::strncpy(outId, args.outText, outCapacity - 1);
        outId[outCapacity - 1] = 0;
    }
    return ok;
}

bool ModuleHost::mapEditorAddChunkPart(const char* kind, char* outId,
                                       std::uint32_t outCapacity) {
    if (mimita::game::gMapEditor) {
        mimita_map_editor::EditorAddChunkPartV1 payload{};
        std::strncpy(payload.kind, kind ? kind : "",
                     sizeof(payload.kind) - 1);
        if (!mimita::game::invokeMapEditor(
                mimita_module::EVENT_EDITOR_ADD_CHUNK_PART, &payload,
                sizeof(payload)) || !payload.result || !outId || outCapacity == 0) {
            return false;
        }
        std::strncpy(outId, payload.outId, outCapacity - 1);
        outId[outCapacity - 1] = 0;
        return true;
    }
    mimita::host::ModuleArgs args{};
    std::strncpy(args.s0, kind ? kind : "", sizeof(args.s0) - 1);
    args.outCapacity = outCapacity;
    const bool ok = mimita::game::invoke(
        mimita::host::ModuleOp::MapEditorAddChunkPart, &args, sizeof(args), nullptr, 0);
    if (ok && outId && outCapacity) {
        std::strncpy(outId, args.outText, outCapacity - 1);
        outId[outCapacity - 1] = 0;
    }
    return ok;
}

bool ModuleHost::mapEditorAddConnector(const char* side) {
    if (mimita::game::gMapEditor) {
        mimita_map_editor::EditorAddConnectorV1 payload{};
        std::strncpy(payload.side, side ? side : "",
                     sizeof(payload.side) - 1);
        return mimita::game::invokeMapEditor(
            mimita_module::EVENT_EDITOR_ADD_CONNECTOR, &payload,
            sizeof(payload)) && payload.result != 0;
    }
    mimita::host::ModuleArgs args{};
    std::strncpy(args.s0, side ? side : "", sizeof(args.s0) - 1);
    return mimita::game::invoke(mimita::host::ModuleOp::MapEditorAddConnector,
                                &args, sizeof(args), nullptr, 0);
}

bool ModuleHost::mapEditorEvaluateConnectorPair(
    const char* parentChunk, const char* parentConnector,
    const char* childChunk, const char* childConnector,
    float outTransform[16], char* reason, std::uint32_t reasonCapacity) {
    mimita::host::ModuleArgs args{};
    std::strncpy(args.s0, parentChunk ? parentChunk : "", sizeof(args.s0) - 1);
    std::strncpy(args.s1, parentConnector ? parentConnector : "", sizeof(args.s1) - 1);
    std::strncpy(args.s2, childChunk ? childChunk : "", sizeof(args.s2) - 1);
    std::strncpy(args.s3, childConnector ? childConnector : "", sizeof(args.s3) - 1);
    args.outCapacity = reasonCapacity;
    const bool ok = mimita::game::invoke(
        mimita::host::ModuleOp::MapEditorEvaluateConnectorPair,
        &args, sizeof(args), nullptr, 0);
    if (ok) {
        if (outTransform) std::memcpy(outTransform, args.outF, sizeof(args.outF));
        if (reason && reasonCapacity) {
            std::strncpy(reason, args.outText, reasonCapacity - 1);
            reason[reasonCapacity - 1] = 0;
        }
    }
    return ok;
}

bool ModuleHost::mapEditorExecuteCommand(const char* command, const char* arguments) {
    if (mimita::game::gMapEditor) {
        mimita_map_editor::EditorCommandV1 payload{};
        payload.command = command;
        payload.arguments = arguments;
        return mimita::game::invokeMapEditor(
            mimita_module::EVENT_EDITOR_COMMAND, &payload,
            sizeof(payload)) && payload.result != 0;
    }
    mimita::host::ModuleArgs args{};
    std::strncpy(args.s0, command ? command : "", sizeof(args.s0) - 1);
    std::strncpy(args.s1, arguments ? arguments : "", sizeof(args.s1) - 1);
    return mimita::game::invoke(mimita::host::ModuleOp::MapEditorExecuteCommand,
                                &args, sizeof(args), nullptr, 0);
}

bool ModuleHost::mapEditorPreviewConnections(char* report, std::uint32_t capacity) {
    if (mimita::game::gMapEditor) {
        mimita_map_editor::EditorPreviewV1 payload{};
        if (!mimita::game::invokeMapEditor(
                mimita_module::EVENT_EDITOR_PREVIEW_CONNECTIONS, &payload,
                sizeof(payload)) || !payload.result) {
            return false;
        }
        if (report && capacity) {
            std::strncpy(report, payload.report, capacity - 1);
            report[capacity - 1] = 0;
        }
        return true;
    }
    mimita::host::ModuleArgs args{};
    args.outCapacity = capacity;
    const bool ok = mimita::game::invoke(
        mimita::host::ModuleOp::MapEditorPreviewConnections, &args, sizeof(args), nullptr, 0);
    if (ok && report && capacity) {
        std::strncpy(report, args.outText, capacity - 1);
        report[capacity - 1] = 0;
    }
    return ok;
}

bool ModuleHost::mapEditorSetSelectedPosition(const float position[3]) {
    if (position && mimita::game::gMapEditor) {
        mimita_map_editor::EditorSetPositionV1 payload{};
        payload.position[0] = position[0];
        payload.position[1] = position[1];
        payload.position[2] = position[2];
        return mimita::game::invokeMapEditor(
            mimita_module::EVENT_EDITOR_SET_POSITION, &payload,
            sizeof(payload)) && payload.result != 0;
    }
    return mimita::game::invoke(mimita::host::ModuleOp::MapEditorSetSelectedPosition,
                                position, 3 * sizeof(float), nullptr, 0);
}

bool ModuleHost::mapGeometrySignature(const char* bytes, std::uint32_t byteCount,
                                      std::uint64_t* outSignature) {
    if (mimita::game::gMapGeometry) {
        mimita_map_geometry::SignatureV1 payload{};
        payload.version = mimita_map_geometry::ABI_VERSION;
        payload.structSize = sizeof(payload);
        payload.bytes = bytes;
        payload.byteCount = byteCount;
        if (!mimita::game::invokeMapGeometry(
                mimita_module::EVENT_MAP_GEOMETRY_SIGNATURE, &payload,
                sizeof(payload)) || payload.result == 0) {
            return false;
        }
        if (outSignature) *outSignature = payload.outSignature;
        return true;
    }
    mimita::host::ModuleArgs args{};
    args.ptr0 = const_cast<char*>(bytes);
    args.u0 = byteCount;
    args.ptr1 = outSignature;
    return mimita::game::invoke(mimita::host::ModuleOp::MapGeometrySignature,
                                &args, sizeof(args), nullptr, 0);
}

bool ModuleHost::npcBehaviorDashChoice(std::uint32_t actorId, std::uint32_t tick,
                                       float chance) {
    mimita::host::ModuleArgs args{};
    args.u0 = actorId;
    args.u1 = tick;
    args.f0 = chance;
    return mimita::game::invoke(mimita::host::ModuleOp::NpcBehaviorDashChoice,
                                &args, sizeof(args), nullptr, 0);
}

bool ModuleHost::npcBehaviorHoldPursuit(std::uint32_t actorId, std::uint32_t tick) {
    mimita::host::ModuleArgs args{};
    args.u0 = actorId;
    args.u1 = tick;
    return mimita::game::invoke(mimita::host::ModuleOp::NpcBehaviorHoldPursuit,
                                &args, sizeof(args), nullptr, 0);
}

bool ModuleHost::worldRenderingAnalyzeFaces(
    const float transform[16], const float cameraPosition[3],
    mimita_world_rendering::FaceAuditV1* outFaces, std::uint32_t faceCapacity) {
    if (transform && cameraPosition && mimita::game::gWorldRendering) {
        mimita_world_rendering::AnalyzeFacesV1 payload{};
        std::memcpy(payload.transform, transform, sizeof(payload.transform));
        std::memcpy(payload.cameraPosition, cameraPosition,
                    sizeof(payload.cameraPosition));
        payload.capacity = faceCapacity;
        if (!mimita::game::invokeWorldRendering(
                mimita_module::EVENT_RENDER_ANALYZE_FACES, &payload,
                sizeof(payload)) || payload.result == 0) {
            return false;
        }
        const std::uint32_t count =
            faceCapacity < mimita_world_rendering::FACE_COUNT
                ? faceCapacity : mimita_world_rendering::FACE_COUNT;
        if (outFaces) std::memcpy(outFaces, payload.faces,
                                  count * sizeof(payload.faces[0]));
        return true;
    }
    mimita::host::ModuleArgs args{};
    args.ptr0 = const_cast<float*>(transform);
    args.ptr1 = const_cast<float*>(cameraPosition);
    args.ptr2 = outFaces;
    args.u0 = faceCapacity;
    return mimita::game::invoke(mimita::host::ModuleOp::WorldRenderingAnalyzeFaces,
                                &args, sizeof(args), nullptr, 0);
}
