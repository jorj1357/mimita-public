// 2026-11-12
// Game-DLL-side effects host bridge.
//
// The game generation owns EffectPartSystem in the whole-game DLL. The
// resident host forwards bounded EffectsOp requests here for module DLLs and
// any remaining host consumers. Pointer results are game-owned and valid only
// while the effect remains in the pool.

#include <cstdint>

#include "effects/effect-part.h"
#include "host/abi/effects-capability.h"
#include "host/abi/game-host-abi.h"

namespace mimita::game {

void initializeHostEffects(const mimita::host::HostServicesV1*) {}

bool handleEffectsHostCall(mimita::host::EffectsHostCallV1& call) {
    auto& effects = ::EffectPartSystem::instance();
    const auto writePointer = [&](::EffectPart* value) {
        if (call.output && call.outputSize >= sizeof(std::uintptr_t)) {
            *static_cast<std::uintptr_t*>(call.output) =
                reinterpret_cast<std::uintptr_t>(value);
        }
    };
    const auto payload = [&]() -> const mimita::host::EffectsSpawnPayload* {
        if (!call.input || call.inputSize <
                               sizeof(mimita::host::EffectsSpawnPayload)) {
            return nullptr;
        }
        return static_cast<const mimita::host::EffectsSpawnPayload*>(call.input);
    };
    const auto vec = [](const mimita::host::EffectsVec3& value) {
        return glm::vec3(value.x, value.y, value.z);
    };
    switch (call.op) {
        case mimita::host::EffectsOp::Spawn: {
            const auto* effect = static_cast<const ::EffectPart*>(call.input);
            if (!effect || call.inputSize < sizeof(::EffectPart)) return false;
            writePointer(effects.spawn(*effect));
            return true;
        }
        case mimita::host::EffectsOp::SpawnFootstep: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnFootstep(vec(p->a), p->f0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnDash: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnDash(vec(p->a), p->f0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnFreeze: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnFreeze(vec(p->a), p->f0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnDamageImpactSphere: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnDamageImpactSphere(vec(p->a), vec(p->b), p->s0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnMuzzleFlash: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnMuzzleFlash(vec(p->a), p->s0, p->f0, p->s1,
                                                   p->b0 != 0, p->b1 != 0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnTracer: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnTracer(vec(p->a), vec(p->b), p->s0, p->f0,
                                              p->s1));
            return true;
        }
        case mimita::host::EffectsOp::SpawnBulletImpact: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnBulletImpact(vec(p->a), vec(p->b), p->f0,
                                                    p->f1, p->f2));
            return true;
        }
        case mimita::host::EffectsOp::SpawnDeathEllipsoid: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnDeathEllipsoid(vec(p->a), vec(p->b), p->f0,
                                                     p->f1, p->f2, p->f3));
            return true;
        }
        case mimita::host::EffectsOp::SpawnImpactSphereTickCfg: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnImpactSphereTickCfg(vec(p->a)));
            return true;
        }
        case mimita::host::EffectsOp::SpawnImpactSphereTick: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnImpactSphereTick(vec(p->a), vec(p->b), p->f0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnFreezeTrail: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnFreezeTrail(vec(p->a)));
            return true;
        }
        case mimita::host::EffectsOp::SpawnDownDash: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnDownDash(vec(p->a)));
            return true;
        }
        case mimita::host::EffectsOp::SpawnBodyContactSpark: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnBodyContactSpark(vec(p->a), vec(p->b),
                                                       vec(p->c), p->f0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnWorldCracks: {
            const auto* p = payload();
            if (!p) return false;
            effects.spawnWorldCracks(vec(p->a), vec(p->b), vec(p->c), p->f0, p->f1,
                                     p->f2);
            return true;
        }
        case mimita::host::EffectsOp::SpawnExplosionMark: {
            const auto* p = payload();
            if (!p) return false;
            effects.spawnExplosionMark(vec(p->a), vec(p->b));
            return true;
        }
        case mimita::host::EffectsOp::QueueWorldHit: {
            const auto* p = payload();
            if (!p) return false;
            effects.queueWorldHit(vec(p->a), vec(p->b), vec(p->c), p->f0, p->s0,
                                  p->s1);
            return true;
        }
        case mimita::host::EffectsOp::DrainPendingWorldHits: {
            if (!call.input || call.inputSize < sizeof(std::int32_t)) return false;
            effects.drainPendingWorldHits(
                *static_cast<const std::int32_t*>(call.input));
            return true;
        }
        case mimita::host::EffectsOp::DestroyOwner: {
            if (!call.input || call.inputSize < sizeof(std::uint32_t)) return false;
            effects.destroyOwner(*static_cast<const std::uint32_t*>(call.input));
            return true;
        }
        case mimita::host::EffectsOp::PushSurfaceDecal:
            return false;
        case mimita::host::EffectsOp::SpawnEntityImpact: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnEntityImpact(vec(p->a), vec(p->b), p->s0,
                                                    p->s1, p->f0));
            return true;
        }
        case mimita::host::EffectsOp::SpawnWorldImpact: {
            const auto* p = payload();
            if (!p) return false;
            writePointer(effects.spawnWorldImpact(vec(p->a), vec(p->b), p->f0,
                                                   vec(p->c), p->f1));
            return true;
        }
        case mimita::host::EffectsOp::CollectAlive: {
            if (!call.input ||
                call.inputSize <
                    sizeof(mimita::host::EffectsCollectAlivePayload)) {
                return false;
            }
            const auto* p =
                static_cast<const mimita::host::EffectsCollectAlivePayload*>(
                    call.input);
            const int count = effects.collectAlive(
                reinterpret_cast<::EffectPartSystem::PartSnapshot*>(p->out),
                p->maxCount, p->minAlpha);
            if (call.output && call.outputSize >= sizeof(std::int32_t)) {
                *static_cast<std::int32_t*>(call.output) = count;
            }
            return true;
        }
    }
    return false;
}

}  // namespace mimita::game
