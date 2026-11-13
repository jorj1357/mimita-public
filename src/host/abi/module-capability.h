// 2026-11-12
// Purpose: generic, versioned module-host capability for a game.dll generation.
//
// This is the first partial family bridge: only the small, frequently used
// ModuleHost queries/control methods cross the boundary. Typed policy requests
// remain UNRESOLVED until their operations are added. No silent stubs.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kModuleCapabilityVersion = 1u;

enum class ModuleOp : std::uint32_t {
    ActiveGeneration = 1,
    ModuleLoaded = 2,
    MapEditorLoaded = 3,
    NpcPerceptionLoaded = 4,
    NpcBehaviorLoaded = 5,
    BeginFixedTick = 6,
    EndFixedTick = 7,
    Startup = 8,
    Poll = 9,
    WorldgenResolve = 10,
    InvokeEvent = 11,
    NavigationPolicy = 12,
    CollisionActorPairPolicy = 13,
    MapEditorComputeDragDelta = 14,
    MapEditorPickGizmoAxis = 15,
    MapGeometryNodePolicy = 16,
    NetworkHitClaimPolicy = 17,
    NetworkMovementValidationPolicy = 18,
    NpcBehaviorSelfTest = 19,
    NpcGrenadeDecision = 20,
    NpcPerceptionStep = 21,
    NpcWeaponChoice = 22,
    ChunkCollisionBuild = 23,
    CollisionContactPolicy = 24,
    CollisionFinalPosePolicy = 25,
    CollisionSolveActorWorld = 26,
    CollisionSolveContacts = 27,
    CollisionSurfacePolicy = 28,
    MapEditorAddChunk = 29,
    MapEditorAddChunkPart = 30,
    MapEditorAddConnector = 31,
    MapEditorEvaluateConnectorPair = 32,
    MapEditorExecuteCommand = 33,
    MapEditorMapCursor = 34,
    MapEditorPreviewConnections = 35,
    MapEditorResolveClick = 36,
    MapEditorSetSelectedPosition = 37,
    MapGeometryObjectPlan = 38,
    MapGeometrySignature = 39,
    NetworkCombatValidationPolicy = 40,
    NetworkInterpolationPolicy = 41,
    NetworkMapSyncPolicy = 42,
    NetworkMovementCorrection = 43,
    NetworkReconcilePolicy = 44,
    NetworkReconnectPolicy = 45,
    NetworkSessionHealthPolicy = 46,
    NetworkSnapshotPolicy = 47,
    NpcActorState = 48,
    NpcAimPolicy = 49,
    NpcAttackIntent = 50,
    NpcBehaviorAction = 51,
    NpcBehaviorDashChoice = 52,
    NpcBehaviorDecision = 53,
    NpcBehaviorHoldPursuit = 54,
    NpcDashDecision = 55,
    NpcFireGate = 56,
    NpcGoalScore = 57,
    NpcPayloadRole = 58,
    NpcStateTiming = 59,
    NpcTargetSelect = 60,
    NpcTargetingPolicy = 61,
    WorldRenderingAnalyzeFaces = 62,
    WorldRenderingBuildBox = 63,
};

struct ModuleArgs {
    void* ptr0;
    void* ptr1;
    void* ptr2;
    std::uint32_t u0;
    std::uint32_t u1;
    std::uint32_t u2;
    float f0;
    char s0[256];
    char s1[256];
    char s2[256];
    char s3[256];
    char outText[512];
    float outF[16];
    std::uint32_t outCapacity;
    std::uint32_t reserved;
};

struct ModuleCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Returns 1 when handled, 0 when unsupported/failed. input/output are plain
    // bounded values valid only for the call; the host must not retain them.
    std::int32_t (*invoke)(void* hostUserData, ModuleOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize);
};

}  // namespace mimita::host
