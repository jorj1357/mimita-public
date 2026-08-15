// 07 31 2026, 14 50
/* purpose
* Implements the EffectPartSystem spawn helpers and the hot-reload game DLL effect updater.
* Provides pool-backed effect creation for tracers, muzzle flashes, impacts, and tick spheres.
* Does NOT own server weapon authority, packet send/receive, or damage validation.
* Does NOT render effects; rendering lives in effect-part-render.cpp.
*/
#if defined(MIMITA_GAME_DLL)

#include "hot-reload/game-api.h"

#include <algorithm>

namespace {

bool MIMITA_GAME_CALL gameOnReload(GameMemory* memory)
{
    return memory && memory->apiVersion == MIMITA_GAME_API_VERSION;
}

void MIMITA_GAME_CALL gameBeforeUnload(GameMemory*)
{
}

void MIMITA_GAME_CALL gameUpdateEffects(
    GameMemory*,
    GameEffectPartState* effects,
    std::uint32_t effectCount,
    float dt)
{
    if (!effects || dt <= 0.0f)
        return;

    dt = (std::min)(dt, 0.1f);
    for (std::uint32_t i = 0; i < effectCount; ++i) {
        GameEffectPartState& effect = effects[i];
        if (!effect.alive)
            continue;

        effect.lifetime += dt;
        if (effect.lifetime < 0.0f)
            continue;

        if (!effect.sticky) {
            effect.position[0] += effect.velocity[0] * dt;
            effect.position[1] += effect.velocity[1] * dt;
            effect.position[2] += effect.velocity[2] * dt;
        }
        if (effect.affectedByGravity)
            effect.velocity[2] -= (effect.gravity > 0.0f ? effect.gravity : 9.81f) * dt;
        if (effect.lifetime >= effect.maxLifetime)
            effect.alive = 0;
    }
}

}

MIMITA_GAME_EXPORT bool MIMITA_GAME_CALL GetGameAPI(
    std::uint32_t requestedVersion,
    GameAPI* outAPI)
{
    if (!outAPI || requestedVersion != MIMITA_GAME_API_VERSION)
        return false;

    *outAPI = {};
    outAPI->version = MIMITA_GAME_API_VERSION;
    outAPI->structSize = sizeof(GameAPI);
    outAPI->onReload = gameOnReload;
    outAPI->beforeUnload = gameBeforeUnload;
    outAPI->updateEffects = gameUpdateEffects;
    return true;
}

#else

#include "effect-part.h"
#include "combat/shot-profiler.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-types.h"
#include "config/size-scaling-config.h"
#include "config/weapon-tracers-config.h"
#include "config/impact-decals-config.h"
#include "debug/debug-log.h"
#include "effects/hit-effects.h"
#include "effects/muzzle-flash-config.h"
#include "config.h"
#include "replay/replay.h"
#include "hot-reload/hot-reload-system.h"
#include "perf/perf-spike.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>

EffectPart* EffectPartSystem::spawnEntityImpact(
    glm::vec3 position,
    glm::vec3 normal,
    const std::string& sourceActorId,
    const std::string& targetActorId,
    float sizeScale)
{
    if (!HitEffects::config().core.entityImpact) return nullptr;
    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=effect-part.cpp Type=impact_entity pos=(%.1f,%.1f,%.1f)\n",
                   position.x, position.y, position.z);
    }
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    EffectPart effect;
    effect.position = position;
    effect.normal = normal;
    effect.replayType = "impact_entity";
    effect.color = {0.9f, 0.02f, 0.02f};
    effect.maxLifetime = 0.18f;
    effect.scale = 0.12f * sfx;
    effect.endScale = 0.4f * sfx;
    effect.billboardText = false;
    effect.sticky = true;
    effect.sourceActorId = sourceActorId;
    effect.targetActorId = targetActorId;
    return spawn(effect);
}

EffectPart* EffectPartSystem::spawnWorldImpact(glm::vec3 position, glm::vec3 normal, float sizeScale) {
    if (!HitEffects::config().core.worldImpact) return nullptr;
    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=effect-part.cpp Type=impact_world pos=(%.1f,%.1f,%.1f)\n",
                   position.x, position.y, position.z);
    }
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    EffectPart e;
    e.position = position;
    e.normal = normal;
    e.replayType = "impact_world";
    e.color = {0.55f, 0.55f, 0.55f};
    e.maxLifetime = 0.5f;
    e.scale = 0.1f * sfx;
    e.endScale = 5.0f * sfx;
    e.alpha = 0.5f;
    e.billboardText = false;
    e.sticky = true;
    EffectPart* spawned = spawn(e);
    return spawned;
}

EffectPart* EffectPartSystem::spawnMuzzleFlash(glm::vec3 position, const std::string& sourceActorId, float sizeScale) {
    const MuzzleFlashSettings& cfg = MuzzleFlashConfig::instance().data();
    if (!cfg.enabled) return nullptr;
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    EffectPart e;
    e.position = position;
    e.replayType = "muzzle_flash";
    e.color = cfg.color;
    e.maxLifetime = std::max(0.001f, cfg.lifetime);
    e.scale = cfg.scale * sfx;
    e.endScale = cfg.endScale * sfx;
    e.alpha = cfg.alpha;
    e.billboardText = false;
    e.sticky = true;
    e.sourceActorId = sourceActorId;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnImpactSphereTick(glm::vec3 position, glm::vec3 color, float radius) {
    EffectPart e;
    e.position = position;
    e.replayType = "impact_tick";
    e.color = color;
    e.maxLifetime = 1.0f / 60.0f;
    e.lifetime = 0.0f;
    e.scale = radius;
    e.endScale = radius;
    e.alpha = 1.0f;
    e.billboardText = false;
    e.sticky = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnTracer(glm::vec3 start, glm::vec3 end, const std::string& sourceActorId, float sizeScale, const std::string& weaponId) {
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    const WeaponTracerConfig& tc = WeaponTracersConfig::instance().forWeapon(weaponId);
    // Default the visible beam thickness to the weapon's beam_thickness
    // (weapons.json) so the visual matches the hitbox; the tracer JSON can
    // override it explicitly per weapon.
    float beamThickness = tc.thickness;
    if (!tc.thicknessSet)
    {
        const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
        if (def && def->beamThickness > 0.0f)
            beamThickness = def->beamThickness;
    }
    EffectPart e;
    e.position = start;
    e.replayType = "tracer";
    e.endPosition = end;
    e.color = tc.color;
    e.maxLifetime = std::max(0.01f, tc.lifetime);
    e.scale = beamThickness * sfx;
    e.endScale = tc.endScale * sfx;
    e.thickness = beamThickness * sfx;
    e.endThickness = tc.endThickness * sfx;
    e.alpha = tc.startAlpha;
    e.billboardText = false;
    e.sticky = true;
    e.beam = true;
    e.sourceActorId = sourceActorId;
    if (!tc.enabled)
        return nullptr;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnDeathEllipsoid(glm::vec3 position, glm::vec3 direction,
                                                   float length, float radius, float lifetime, float sizeScale)
{
    const auto& deCfg = HitEffects::config().deathEllipsoid;
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    EffectPart e;
    e.position = position;
    // Store direction indicator in endPosition so the renderer can compute elongation
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(1.0f, 0.0f, 0.0f);
    e.endPosition = position + dir * length * sfx;
    e.replayType = "death_ellipsoid";
    e.color = glm::vec3(deCfg.color);
    e.alpha = deCfg.baseAlpha;
    e.maxLifetime = lifetime;
    e.scale = radius * sfx;
    e.endScale = radius * sfx;
    e.sticky = true;
    e.billboardText = false;
    e.affectedByGravity = false;
    Debug::log(Debug::Category::NpcCombat, "[DEATH ELLIPSOID] victim=%s pos=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) lifetime=%.1f length=%.1f radius=%.1f",
               "", position.x, position.y, position.z, dir.x, dir.y, dir.z, lifetime, length, radius);
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnBulletImpact(glm::vec3 position, glm::vec3 normal, float sizeScale) {
    (void)sizeScale;
    const auto& cfg = ImpactDecalsConfig::instance().data();
    if (!HitEffects::config().core.bulletImpact) return nullptr;
    if (!cfg.enabled || !cfg.bulletHoles.enabled) return nullptr;
    const auto& b = cfg.bulletHoles;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0.0f, 0.0f, 1.0f);
    SurfaceDecal decal;
    decal.position = position + n * 0.01f;
    decal.normal = n;
    decal.color = b.color;
    decal.kind = SurfaceDecalKind::BulletHole;
    decal.radius = b.radius;
    decal.height = b.height;
    decal.lifetime = b.lifetime;
    decal.fadeTime = b.fadeTime;
    decal.alpha = b.alpha;
    decal.baseAlpha = b.alpha;
    pushSurfaceDecal(decal, b.maxCount);
    return nullptr;
}

void EffectPartSystem::spawnWorldCracks(glm::vec3 position, glm::vec3 normal,
                                        const glm::vec3& direction, float sizeScale) {
    (void)sizeScale;
    const auto& cfg = ImpactDecalsConfig::instance().data();
    if (!cfg.enabled || !cfg.worldCracks.enabled) return;
    const auto& c = cfg.worldCracks;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangent = glm::normalize(
        std::fabs(n.z) < 0.9f
            ? glm::cross(n, glm::vec3(0.0f, 0.0f, 1.0f))
            : glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
    // Preferred axis: shot direction projected onto the surface plane so cracks
    // lie flat along the surface on any surface. Falls back to a random tangent
    // when the shot hits the surface head-on (projection is degenerate).
    glm::vec3 preferred(0.0f);
    if (glm::length(direction) > 0.001f) {
        const glm::vec3 d = glm::normalize(direction);
        preferred = d - n * glm::dot(d, n);
        if (glm::length(preferred) < 0.1f)
            preferred = glm::vec3(0.0f);
        else
            preferred = glm::normalize(preferred);
    }
    for (int i = 0; i < c.count; ++i) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float offset = ((float)(rand() % 1001) / 1000.0f) * 0.3f;
        glm::vec3 axis;
        if (glm::length(preferred) > 0.001f) {
            // Mostly along the projected shot, with a small random spread.
            const float spread = 0.3f;
            const glm::vec3 jitter = glm::normalize(
                tangent * std::cos(angle) + bitangent * std::sin(angle));
            axis = glm::normalize(preferred + jitter * spread);
        } else {
            axis = glm::normalize(tangent * std::cos(angle) + bitangent * std::sin(angle));
        }
        SurfaceDecal decal;
        decal.position = position + n * 0.005f + axis * offset;
        decal.normal = n;
        decal.axis = axis;
        decal.color = c.color;
        decal.kind = SurfaceDecalKind::Crack;
        decal.radius = c.thickness;
        decal.height = c.length;
        decal.lifetime = c.lifetime;
        decal.fadeTime = c.fadeTime;
        decal.alpha = c.alpha;
        decal.baseAlpha = c.alpha;
        pushSurfaceDecal(decal, c.maxCount);
    }
}

void EffectPartSystem::pushSurfaceDecal(const SurfaceDecal& decal, int maxCount) {
    if (maxCount <= 0)
        maxCount = 256;
    while ((int)mSurfaceDecals.size() >= maxCount)
        mSurfaceDecals.erase(mSurfaceDecals.begin());
    mSurfaceDecals.push_back(decal);
}

EffectPart* EffectPartSystem::spawnDamageImpactSphere(glm::vec3 position, glm::vec3 direction, const std::string& victim)
{
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(0.0f, 0.0f, 1.0f);
    float length = 2.67f;
    float radius = 0.5f;
    EffectPart e;
    e.position = position;
    e.endPosition = position + dir * length;
    e.replayType = "damage_impact_sphere";
    e.color = {1.0f, 0.3f, 0.3f};
    e.alpha = 0.5f;
    e.maxLifetime = 0.5f;
    e.scale = radius;
    e.endScale = radius;
    e.sticky = true;
    e.billboardText = false;
    e.targetActorId = victim;
    Debug::log(Debug::Category::NpcCombat, "[DAMAGE IMPACT] victim=%s pos=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) length=%.2f radius=%.2f lifetime=%.2f alpha=%.2f",
               victim.c_str(), position.x, position.y, position.z, dir.x, dir.y, dir.z, length, radius, e.maxLifetime, e.alpha);
    return spawn(e);
}

void EffectPartSystem::destroyOwner(unsigned int ownerId) {
    for (unsigned int i = 0; i < POOL_SIZE; ++i) {
        EffectPart& fx = mPool[i];
        if (!fx.alive) continue;
        if (fx.ownerId == ownerId) {
            fx.alive = false;
            fx.resetStrings();
            --mActiveCount;
            mFreeSlots.push_back(i);
        }
    }
}

EffectPart* EffectPartSystem::spawn(const EffectPart& effect) {
    MIMITA_PERF_SCOPE("EffectPart::Spawn");
    if (gShotProfiler) {
        gShotProfiler->effectsSpawned++;
        gShotProfiler->poolLinearScans++;
    }

    ReplayEffectEvent event;
    event.type = effect.replayType;
    event.label = effect.label;
    event.position = effect.position;
    event.from = effect.position;
    event.to = effect.endPosition;
    event.rotation = effect.rotation;
    event.scale = effect.box ? effect.halfSize * 2.0f : glm::vec3(effect.scale);
    event.endScale = glm::vec3(effect.endScale);
    event.color = glm::vec4(effect.color, effect.alpha);
    event.velocity = effect.velocity;
    event.normal = effect.normal;
    event.direction = glm::length(effect.endPosition - effect.position) > 0.001f
        ? glm::normalize(effect.endPosition - effect.position)
        : glm::vec3(0.0f);
    event.lifetime = effect.maxLifetime;
    event.startDelay = std::max(0.0f, -effect.lifetime);
    event.alpha = effect.alpha;
    event.thickness = effect.thickness;
    event.endThickness = effect.endThickness;
    event.gravity = effect.gravity;
    event.sourceActorId = effect.sourceActorId;
    event.targetActorId = effect.targetActorId;
    event.texturePath = effect.texturePath;
    event.materialName = effect.materialName;
    event.billboardText = effect.billboardText;
    event.beam = effect.beam;
    {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->replayRecordMs : nullptr);
        if (effect.replayType != "debris" && effect.replayType != "debris_block" && effect.replayType != "debris_batch")
            captureReplayEffect(event);
        if (gShotProfiler) gShotProfiler->replayEventsCreated++;
    }

    if (!mFreeSlots.empty()) {
        unsigned int idx = mFreeSlots.back();
        mFreeSlots.pop_back();
        mPool[idx] = effect;
        mPool[idx].alive = true;
        ++mActiveCount;
        mSpawnCursor = (idx + 1) % POOL_SIZE;
        if (gShotProfiler) {
            gShotProfiler->poolHits++;
            gShotProfiler->poolLinearScans++;
        }
        return &mPool[idx];
    }

    for (unsigned int i = 0; i < POOL_SIZE; ++i) {
        unsigned int idx = (mSpawnCursor + i) % POOL_SIZE;
        if (!mPool[idx].alive) {
            mPool[idx] = effect;
            mPool[idx].alive = true;
            ++mActiveCount;
            mSpawnCursor = (idx + 1) % POOL_SIZE;
            if (gShotProfiler) {
                gShotProfiler->poolHits++;
                gShotProfiler->poolLinearScans += (i + 1);
            }
            return &mPool[idx];
        }
    }
    if (gShotProfiler) gShotProfiler->poolLinearScans += POOL_SIZE;
    return nullptr;
}

EffectPart* EffectPartSystem::spawnFootstep(glm::vec3 position, float sizeScale) {
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    EffectPart e;
    e.position = position;
    e.replayType = "footstep";
    e.color = {1.0f, 1.0f, 1.0f};
    e.maxLifetime = 0.5f;
    e.scale = 0.18f * sfx;
    e.endScale = 0.06f * sfx;
    e.billboardText = false;
    e.flatDecal = false;
    e.sticky = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnDash(glm::vec3 position, float sizeScale) {
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    EffectPart e;
    e.position = position;
    e.replayType = "dash";
    e.color = {0.2f, 0.6f, 1.0f};
    e.maxLifetime = 0.8f;
    e.scale = 0.35f * sfx;
    e.endScale = 0.1f * sfx;
    e.billboardText = false;
    e.flatDecal = false;
    e.sticky = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnPerfectDash(glm::vec3 position, float sizeScale) {
    const auto& sc = SizeScalingConfig::instance().data();
    float ss = std::max(sizeScale, 0.001f);
    float sfx = sc.scale(1.0f, sc.hitfxRadiusExponent, ss);
    EffectPart e;
    e.position = position;
    e.replayType = "dash";
    e.color = {1.0f, 0.8f, 0.1f};
    e.maxLifetime = 1.2f;
    e.scale = 0.7f * sfx;
    e.endScale = 0.05f * sfx;
    e.billboardText = false;
    e.flatDecal = false;
    e.sticky = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnFreeze(glm::vec3 position, float freezeDuration) {
    EffectPart e;
    e.position = position;
    e.replayType = "freeze";
    e.color = {0.2f, 1.0f, 0.3f};
    e.maxLifetime = 0.1f;
    char buf[64];
    snprintf(buf, sizeof(buf), "freeze(%.2f)", freezeDuration);
    e.label = buf;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnFreezeTrail(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.endPosition = position + glm::vec3(0.0f, 0.0f, 2.0f);
    e.replayType = "freeze_trail";
    e.color = {0.1f, 0.1f, 0.4f};
    e.maxLifetime = 0.05f;
    e.lifetime = 0.0f;
    e.scale = 0.4f;
    e.endScale = 0.4f;
    e.alpha = 0.4f;
    e.sticky = true;
    e.billboardText = false;
    e.flatDecal = false;
    Debug::log(Debug::Category::Physics, "[VFX] freeze trail spawned pos=(%.2f %.2f %.2f) alpha=%.2f lifetime=%.2f\n",
               position.x, position.y, position.z, e.alpha, e.maxLifetime);
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnDownDash(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.endPosition = position + glm::vec3(0.0f, 0.0f, 3.0f);
    e.replayType = "down_dash";
    e.color = {0.1f, 0.8f, 0.8f};
    e.maxLifetime = 0.2f;
    e.lifetime = 0.0f;
    e.scale = 0.5f;
    e.endScale = 0.5f;
    e.alpha = 0.35f;
    e.sticky = true;
    e.billboardText = false;
    e.flatDecal = false;
    Debug::log(Debug::Category::Physics, "[VFX] down dash spawned pos=(%.2f %.2f %.2f) alpha=%.2f lifetime=%.2f\n",
               position.x, position.y, position.z, e.alpha, e.maxLifetime);
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnImpact(glm::vec3 position, glm::vec3 color, const char* label) {
    EffectPart e;
    e.position = position;
    e.replayType = label ? label : "impact";
    e.color = color;
    e.maxLifetime = 1.0f;
    e.label = label;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnCustom(glm::vec3 position, glm::vec3 color, float lifetime, const char* label) {
    EffectPart e;
    e.position = position;
    e.replayType = label ? label : "custom";
    e.color = color;
    e.maxLifetime = lifetime;
    e.label = label;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

void EffectPartSystem::queueWorldHit(glm::vec3 position, glm::vec3 normal, glm::vec3 direction,
                                      float debrisForce, const std::string& attacker,
                                      const std::string& weaponSource)
{
    int count = mPendingTail - mPendingHead;
    if (count >= MAX_PENDING_HITS)
        return;
    int idx = mPendingTail % MAX_PENDING_HITS;
    mPendingHits[idx].position = position;
    mPendingHits[idx].normal = normal;
    mPendingHits[idx].direction = direction;
    mPendingHits[idx].debrisForce = debrisForce;
    mPendingHits[idx].attacker = attacker;
    mPendingHits[idx].weaponSource = weaponSource;
    ++mPendingTail;
}

void EffectPartSystem::drainPendingWorldHits(int maxCount)
{
    int count = mPendingTail - mPendingHead;
    int toDrain = std::min(count, maxCount);
    for (int i = 0; i < toDrain; ++i) {
        int idx = mPendingHead % MAX_PENDING_HITS;
        const auto& h = mPendingHits[idx];
        HitEvent ev;
        ev.position = h.position;
        ev.normal = h.normal;
        ev.direction = h.direction;
        ev.hitWorld = true;
        ev.damage = 0;
        ev.attacker = h.attacker;
        ev.weaponSource = h.weaponSource;
        HitEffects::onHit(ev);
        spawnWorldDebris(h.position, h.normal, h.debrisForce);
        ++mPendingHead;
    }
}

void EffectPartSystem::clear() {
    for (unsigned int i = 0; i < POOL_SIZE; ++i) {
        EffectPart& slot = mPool[i];
        if (slot.alive) {
            slot.alive = false;
            slot.resetStrings();
        }
    }
    mActiveCount = 0;
    mFreeSlots.clear();
    mFreeSlots.reserve(POOL_SIZE);
    for (unsigned int i = 0; i < POOL_SIZE; ++i)
        mFreeSlots.push_back(i);
    mBloodParticles.clear();
    mSurfaceDecals.clear();
    mBloodDebugSegmentCount = 0;
    mPendingHead = 0;
    mPendingTail = 0;
}

int EffectPartSystem::collectAlive(PartSnapshot* out, int maxCount, float minAlpha) const
{
    int count = 0;
    for (const auto& e : mPool) {
        if (!e.alive || e.lifetime < 0.0f) continue;
        float t = std::clamp(e.lifetime / e.maxLifetime, 0.0f, 1.0f);
        float alpha = e.alpha;
        if (e.replayType != "death_ellipsoid" || HitEffects::config().deathEllipsoid.fade)
            alpha *= (1.0f - t);
        if (alpha < minAlpha) continue;
        if (count >= maxCount) break;
        out[count].position = e.position;
        out[count].scale = e.scale + (e.endScale - e.scale) * t;
        out[count].alpha = alpha;
        count++;
    }
    return count;
}

#endif
