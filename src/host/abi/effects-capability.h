// 2026-11-12
// Versioned, bounded effects capability for a game.dll generation.
//
// The resident EffectPartSystem owns the pool. The generation forwards spawn
// and control requests. Pointer-returning ops return a host-owned EffectPart*
// valid while the effect remains in the resident pool; call sites must not
// retain it across effects recycling or a generation swap.

#pragma once

#include <cstdint>

namespace mimita::host {

inline constexpr std::uint32_t kEffectsCapabilityVersion = 1u;

enum class EffectsOp : std::uint32_t {
    Spawn = 1,
    SpawnFootstep = 2,
    SpawnDash = 3,
    SpawnFreeze = 4,
    SpawnDamageImpactSphere = 5,
    SpawnMuzzleFlash = 6,
    SpawnTracer = 7,
    SpawnBulletImpact = 8,
    SpawnDeathEllipsoid = 9,
    SpawnImpactSphereTickCfg = 10,
    SpawnImpactSphereTick = 11,
    SpawnFreezeTrail = 12,
    SpawnDownDash = 13,
    SpawnBodyContactSpark = 14,
    SpawnWorldCracks = 15,
    SpawnExplosionMark = 16,
    QueueWorldHit = 17,
    DrainPendingWorldHits = 18,
    DestroyOwner = 19,
    PushSurfaceDecal = 20,
    SpawnEntityImpact = 21,
    SpawnWorldImpact = 22,
    CollectAlive = 23,
};

struct EffectsVec3 {
    float x;
    float y;
    float z;
};

struct EffectsSpawnPayload {
    EffectsVec3 a;
    EffectsVec3 b;
    EffectsVec3 c;
    float f0;
    float f1;
    float f2;
    float f3;
    std::uint32_t u0;
    std::int32_t i0;
    std::int32_t b0;
    std::int32_t b1;
    char s0[128];
    char s1[128];
};

struct EffectsSurfaceDecalPayload {
    std::int32_t maxCount;
    char reason[64];
    char sourceActorId[128];
    char targetActorId[128];
};

struct EffectsCollectAlivePayload {
    void* out;
    std::int32_t maxCount;
    float minAlpha;
};

struct EffectsCapabilityV1 {
    std::uint32_t version;
    std::uint32_t structSize;

    // Returns 1 when handled, 0 when unsupported/failed. input/output are plain
    // bounded values valid only for the call; pointer results are resident-owned.
    std::int32_t (*invoke)(void* hostUserData, EffectsOp op, const void* input,
                           std::uint32_t inputSize, void* output,
                           std::uint32_t outputSize);
};

}  // namespace mimita::host
