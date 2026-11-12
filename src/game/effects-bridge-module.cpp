// 2026-11-12
// Game-DLL-side partial effects bridge for CAP_EFFECTS.
//
// The resident EffectPartSystem owns the pool; this generation only forwards
// spawn/control requests. Pointer results are resident-owned and must not be
// retained across effect recycling or a generation swap.

#include <cstdint>
#include <cstring>
#include <string>

#include "effects/effect-part.h"
#include "host/abi/effects-capability.h"
#include "host/abi/game-host-abi.h"

namespace mimita::game {

void initializeHostEffects(const mimita::host::HostServicesV1* services);

namespace {
const mimita::host::EffectsCapabilityV1* gEffects = nullptr;
void* gHostData = nullptr;

bool invoke(mimita::host::EffectsOp op, const void* input, std::uint32_t inputSize,
            void* output, std::uint32_t outputSize) {
    if (!gEffects || !gEffects->invoke) {
        return false;
    }
    return gEffects->invoke(gHostData, op, input, inputSize, output, outputSize) != 0;
}

void copyString(char* destination, std::size_t capacity, const std::string& value) {
    if (!destination || capacity == 0) {
        return;
    }
    const std::size_t count = value.size() < capacity - 1 ? value.size() : capacity - 1;
    std::memcpy(destination, value.data(), count);
    destination[count] = 0;
}

void setVec(mimita::host::EffectsVec3& out, const glm::vec3& value) {
    out.x = value.x;
    out.y = value.y;
    out.z = value.z;
}

mimita::host::EffectsSpawnPayload makePayload() {
    return mimita::host::EffectsSpawnPayload{};
}

EffectPart* receivePointer(mimita::host::EffectsOp op,
                           const mimita::host::EffectsSpawnPayload& payload) {
    std::uintptr_t pointer = 0;
    invoke(op, &payload, sizeof(payload), &pointer, sizeof(pointer));
    return reinterpret_cast<EffectPart*>(pointer);
}
}  // namespace

void initializeHostEffects(const mimita::host::HostServicesV1* services) {
    gEffects = nullptr;
    gHostData = services ? services->hostUserData : nullptr;
    if (!services || !services->get_capability) {
        return;
    }
    auto* capability = static_cast<const mimita::host::EffectsCapabilityV1*>(
        services->get_capability(
            services->hostUserData,
            static_cast<std::uint32_t>(mimita::host::HostCapabilityId::Effects),
            mimita::host::kEffectsCapabilityVersion));
    if (capability &&
        capability->version == mimita::host::kEffectsCapabilityVersion &&
        capability->structSize >= sizeof(mimita::host::EffectsCapabilityV1)) {
        gEffects = capability;
    }
}

}  // namespace mimita::game

EffectPartSystem& EffectPartSystem::instance() {
    static EffectPartSystem system;
    return system;
}

EffectPart* EffectPartSystem::spawn(const EffectPart& effect) {
    std::uintptr_t pointer = 0;
    mimita::game::invoke(mimita::host::EffectsOp::Spawn, &effect, sizeof(effect),
                         &pointer, sizeof(pointer));
    return reinterpret_cast<EffectPart*>(pointer);
}

EffectPart* EffectPartSystem::spawnFootstep(glm::vec3 position, float sizeScale) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    payload.f0 = sizeScale;
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnFootstep,
                                        payload);
}

EffectPart* EffectPartSystem::spawnDash(glm::vec3 position, float sizeScale) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    payload.f0 = sizeScale;
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnDash, payload);
}

EffectPart* EffectPartSystem::spawnFreeze(glm::vec3 position, float freezeDuration) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    payload.f0 = freezeDuration;
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnFreeze, payload);
}

EffectPart* EffectPartSystem::spawnDamageImpactSphere(glm::vec3 position,
                                                      glm::vec3 direction,
                                                      const std::string& victim) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, direction);
    mimita::game::copyString(payload.s0, sizeof(payload.s0), victim);
    return mimita::game::receivePointer(
        mimita::host::EffectsOp::SpawnDamageImpactSphere, payload);
}

EffectPart* EffectPartSystem::spawnMuzzleFlash(glm::vec3 position,
                                               const std::string& sourceActorId,
                                               float sizeScale,
                                               const std::string& weaponId,
                                               bool spawnVisual,
                                               bool spawnLighting) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::copyString(payload.s0, sizeof(payload.s0), sourceActorId);
    payload.f0 = sizeScale;
    mimita::game::copyString(payload.s1, sizeof(payload.s1), weaponId);
    payload.b0 = spawnVisual ? 1 : 0;
    payload.b1 = spawnLighting ? 1 : 0;
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnMuzzleFlash,
                                        payload);
}

EffectPart* EffectPartSystem::spawnTracer(glm::vec3 start, glm::vec3 end,
                                          const std::string& sourceActorId,
                                          float sizeScale,
                                          const std::string& weaponId) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, start);
    mimita::game::setVec(payload.b, end);
    mimita::game::copyString(payload.s0, sizeof(payload.s0), sourceActorId);
    payload.f0 = sizeScale;
    mimita::game::copyString(payload.s1, sizeof(payload.s1), weaponId);
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnTracer, payload);
}

EffectPart* EffectPartSystem::spawnBulletImpact(glm::vec3 position, glm::vec3 normal,
                                                float sizeScale,
                                                float lifetimeScale,
                                                float alphaScale) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, normal);
    payload.f0 = sizeScale;
    payload.f1 = lifetimeScale;
    payload.f2 = alphaScale;
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnBulletImpact,
                                        payload);
}

EffectPart* EffectPartSystem::spawnDeathEllipsoid(glm::vec3 position,
                                                  glm::vec3 direction, float length,
                                                  float radius, float lifetime,
                                                  float sizeScale) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, direction);
    payload.f0 = length;
    payload.f1 = radius;
    payload.f2 = lifetime;
    payload.f3 = sizeScale;
    return mimita::game::receivePointer(
        mimita::host::EffectsOp::SpawnDeathEllipsoid, payload);
}

EffectPart* EffectPartSystem::spawnImpactSphereTickCfg(glm::vec3 position) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    return mimita::game::receivePointer(
        mimita::host::EffectsOp::SpawnImpactSphereTickCfg, payload);
}

EffectPart* EffectPartSystem::spawnImpactSphereTick(glm::vec3 position,
                                                    glm::vec3 color, float radius) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, color);
    payload.f0 = radius;
    return mimita::game::receivePointer(
        mimita::host::EffectsOp::SpawnImpactSphereTick, payload);
}

EffectPart* EffectPartSystem::spawnFreezeTrail(glm::vec3 position) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnFreezeTrail,
                                        payload);
}

EffectPart* EffectPartSystem::spawnDownDash(glm::vec3 position) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnDownDash,
                                        payload);
}

EffectPart* EffectPartSystem::spawnBodyContactSpark(glm::vec3 playerPos,
                                                    glm::vec3 contactPoint,
                                                    glm::vec3 velocity,
                                                    float partRadius) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, playerPos);
    mimita::game::setVec(payload.b, contactPoint);
    mimita::game::setVec(payload.c, velocity);
    payload.f0 = partRadius;
    return mimita::game::receivePointer(
        mimita::host::EffectsOp::SpawnBodyContactSpark, payload);
}

void EffectPartSystem::spawnWorldCracks(glm::vec3 position, glm::vec3 normal,
                                        const glm::vec3& direction, float sizeScale,
                                        float lifetimeScale, float alphaScale) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, normal);
    mimita::game::setVec(payload.c, direction);
    payload.f0 = sizeScale;
    payload.f1 = lifetimeScale;
    payload.f2 = alphaScale;
    mimita::game::invoke(mimita::host::EffectsOp::SpawnWorldCracks, &payload,
                         sizeof(payload), nullptr, 0);
}

void EffectPartSystem::spawnExplosionMark(glm::vec3 position, glm::vec3 normal) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, normal);
    mimita::game::invoke(mimita::host::EffectsOp::SpawnExplosionMark, &payload,
                         sizeof(payload), nullptr, 0);
}

void EffectPartSystem::queueWorldHit(glm::vec3 position, glm::vec3 normal,
                                     glm::vec3 direction, float debrisForce,
                                     const std::string& attacker,
                                     const std::string& weaponSource) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, normal);
    mimita::game::setVec(payload.c, direction);
    payload.f0 = debrisForce;
    mimita::game::copyString(payload.s0, sizeof(payload.s0), attacker);
    mimita::game::copyString(payload.s1, sizeof(payload.s1), weaponSource);
    mimita::game::invoke(mimita::host::EffectsOp::QueueWorldHit, &payload,
                         sizeof(payload), nullptr, 0);
}

void EffectPartSystem::drainPendingWorldHits(int maxCount) {
    mimita::game::invoke(mimita::host::EffectsOp::DrainPendingWorldHits, &maxCount,
                         sizeof(maxCount), nullptr, 0);
}

void EffectPartSystem::destroyOwner(unsigned int ownerId) {
    mimita::game::invoke(mimita::host::EffectsOp::DestroyOwner, &ownerId,
                         sizeof(ownerId), nullptr, 0);
}

void EffectPartSystem::pushSurfaceDecal(const SurfaceDecal&, int, const char*,
                                        const std::string&, const std::string&) {
    // Private resident operation; game code cannot call it directly.
}

EffectPart* EffectPartSystem::spawnEntityImpact(glm::vec3 position, glm::vec3 normal,
                                                const std::string& sourceActorId,
                                                const std::string& targetActorId,
                                                float sizeScale) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, normal);
    mimita::game::copyString(payload.s0, sizeof(payload.s0), sourceActorId);
    mimita::game::copyString(payload.s1, sizeof(payload.s1), targetActorId);
    payload.f0 = sizeScale;
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnEntityImpact,
                                        payload);
}

EffectPart* EffectPartSystem::spawnWorldImpact(glm::vec3 position, glm::vec3 normal,
                                               float sizeScale, glm::vec3 direction,
                                               float lifetimeScale) {
    auto payload = mimita::game::makePayload();
    mimita::game::setVec(payload.a, position);
    mimita::game::setVec(payload.b, normal);
    payload.f0 = sizeScale;
    mimita::game::setVec(payload.c, direction);
    payload.f1 = lifetimeScale;
    return mimita::game::receivePointer(mimita::host::EffectsOp::SpawnWorldImpact,
                                        payload);
}

int EffectPartSystem::collectAlive(PartSnapshot* out, int maxCount,
                                   float minAlpha) const {
    mimita::host::EffectsCollectAlivePayload payload{};
    payload.out = out;
    payload.maxCount = maxCount;
    payload.minAlpha = minAlpha;
    std::int32_t count = 0;
    mimita::game::invoke(mimita::host::EffectsOp::CollectAlive, &payload,
                         sizeof(payload), &count, sizeof(count));
    return count;
}
