#include "effects/hit-effects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "perf/perf.h"


#include <glm/glm.hpp>
#include <glad/glad.h>


#include "combat/shot-profiler.h"
#include "effects/effect-part.h"
#include "config/impact-decals-config.h"
#include "debug/debug-visuals.h"
#include "debug/debug-log.h"
#include "gui/ui-system.h"
#include "camera.h"
#include "renderer/renderer.h"
#include "replay/replay.h"

extern HitFxConfig gConfig;

constexpr int MAX_BURSTS = 64;
HitBurstEffect gBursts[MAX_BURSTS];
int gBurstCount = 0;
int gGlobalTick = 0;

bool gBloodFXEnabled = false;
bool gHitFxTraceEnabled = false;
bool gDashFXEnabled = true;


extern Renderer* gRenderer;

void HitEffects::onHit(const HitEvent& event)
{
    Perf::ScopedTimer _hitfx("HitFX");
    if (!gConfig.enabled) return;
    if (gShotProfiler) gShotProfiler->hitFxCalls++;

    // 1. Legacy contact sphere
    if (gConfig.legacyContactSphere.enabled) {
        EffectPart e;
        e.position = event.position;
        const auto& l = gConfig.legacyContactSphere;
        e.color = glm::vec4(l.color.x, l.color.y, l.color.z, l.alpha);
        e.maxLifetime = l.lifetimeSeconds;
        e.scale = l.startRadius;
        e.endScale = l.endRadius;
        e.replayType = "contact_sphere";
        EffectPartSystem::instance().spawn(e);
        if (gShotProfiler) gShotProfiler->impactSphereMs += 0.01;
    }

    // 2. Entity impact sphere (red)
    if (event.hitEntity && gConfig.core.entityImpact) {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->impactSphereMs : nullptr);
        EffectPartSystem::instance().spawnEntityImpact(
            event.position, event.normal, event.attacker, event.victim);
    }

    // 3. World impact sphere (gray)
    if (event.hitWorld && gConfig.core.worldImpact) {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->impactSphereMs : nullptr);
        EffectPartSystem::instance().spawnWorldImpact(event.position, event.normal);
    }

    // 3b. World cracks along the surface at the hit point
    if (event.hitWorld && gConfig.core.worldImpact) {
        EffectPartSystem::instance().spawnWorldCracks(event.position, event.normal, event.direction);
    }

    // 4. Bullet impact hole
    if (event.hitWorld && gConfig.core.bulletImpact) {
        EffectPartSystem::instance().spawnBulletImpact(event.position, event.normal);
    }

    // 5. Blood effect (gate: impact_decals.json blood.enabled is the single switch)
    if (event.hitEntity && ImpactDecalsConfig::instance().data().blood.enabled) {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->bloodMs : nullptr);
        const float directness = glm::length(event.direction) > 0.001f &&
                                 glm::length(event.normal) > 0.001f
            ? std::clamp(std::fabs(glm::dot(glm::normalize(-event.direction),
                                            glm::normalize(event.normal))), 0.0f, 1.0f)
            : 1.0f;
        EffectPartSystem::instance().spawnBloodEffect(
            event.position, event.direction, (float)event.damage,
            event.attacker, event.victim, directness, event.hitDistance);
    }

    // 6. Damage number - only for entity hits (not world geometry)
    if (event.hitEntity) {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->damageNumberMs : nullptr);
        EffectPartSystem::instance().spawnDamage(event.position, event.victim, event.damage);
    }

    // 6b. Red impact sphere at hit position
    if (event.hitEntity && glm::length(event.direction) > 0.001f) {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->impactSphereMs : nullptr);
        EffectPartSystem::instance().spawnDamageImpactSphere(event.position, event.direction, event.victim);
    }

    // Debug logging for damage feedback
    Debug::log(Debug::Category::NpcCombat,
        "[DAMAGE FEEDBACK] tick=%d frame=%d entity=%s damage=%d damageNumber=%d impactSphere=%d "
        "hitPos=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) lifetimeTicks=%d alpha=%.2f\n",
        gGlobalTick, gGlobalTick, event.victim.c_str(), event.damage,
        (int)(event.hitEntity && gConfig.core.damageNumbers && gConfig.damageNumber.enabled),
        (int)(event.hitEntity && glm::length(event.direction) > 0.001f),
        event.position.x, event.position.y, event.position.z,
        event.direction.x, event.direction.y, event.direction.z,
        30, 0.5f);

    // 7. HitFX timeline (burst)
    {
        auto ts = ShotProfiler::Scope(gShotProfiler ? &gShotProfiler->hitBurstMs : nullptr);
        spawnHitEffects(event.position, event.direction, event.normal, event.damage,
                        event.attacker, event.victim, false);
    }

    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat,
            "[HITFX] source=%s event=%s damage=%d spawnDamage=%d spawnEntityImpact=%d spawnBlood=%d spawnParticles=%d\n",
            event.weaponSource.c_str(),
            event.hitEntity ? "entity_hit" : "world_hit",
            event.damage,
            (int)gConfig.core.damageNumbers,
            (int)(event.hitEntity && gConfig.core.entityImpact),
            (int)(event.hitEntity && gConfig.core.entityImpact),
            (int)gConfig.particles.enabled);
    }

    if (gShotProfiler) gShotProfiler->objectsSpawnedByHitFx++;
}

void HitEffects::spawnHitEffects(glm::vec3 hitPoint, const glm::vec3& hitDirection,
                                  const glm::vec3& hitNormal, int damage,
                                  const std::string& sourceId,
                                  const std::string& targetId,
                                  bool spawnDamageNumber)
{
    (void)sourceId;
    (void)targetId;
    if (!gConfig.enabled) return;
    if (gBurstCount >= MAX_BURSTS) {
        gBursts[0] = gBursts[gBurstCount - 1];
        gBurstCount--;
    }
    HitBurstEffect& b = gBursts[gBurstCount++];
    b.position = hitPoint;
    b.direction = glm::length(hitDirection) > 0.001f ? glm::normalize(hitDirection) : hitNormal;
    b.normal = glm::length(hitNormal) > 0.001f ? glm::normalize(hitNormal) : -b.direction;
    b.spawnTick = gGlobalTick;
    b.totalTicks = gConfig.core.lifetimeTicks;
    b.alive = true;

    ReplayEffectEvent ev;
    ev.type = "hit_burst";
    ev.position = hitPoint;
    ev.sourceActorId = "hitfx";
    ev.targetActorId = "burst";
    ev.lifetime = (float)b.totalTicks / 60.0f;
    captureReplayEffect(ev);

    if (gConfig.legacyContactSphere.enabled) {
        EffectPart e;
        e.position = hitPoint;
        const auto& l = gConfig.legacyContactSphere;
        e.color = glm::vec4(l.color.x, l.color.y, l.color.z, l.alpha);
        e.maxLifetime = l.lifetimeSeconds;
        e.scale = l.startRadius;
        e.endScale = l.endRadius;
        e.replayType = "contact_sphere";
        EffectPartSystem::instance().spawn(e);
    }
    if (spawnDamageNumber)
        EffectPartSystem::instance().spawnDamage(hitPoint, targetId, damage);
    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=hit-effects.cpp Type=contact_sphere pos=(%.1f,%.1f,%.1f)\n",
                   hitPoint.x, hitPoint.y, hitPoint.z);
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=hit-effects.cpp Type=damage_number pos=(%.1f,%.1f,%.1f) damage=%d\n",
                   hitPoint.x, hitPoint.y, hitPoint.z, damage);
    }

    Debug::log(Debug::Category::NpcCombat, "[HITFX] spawned at (%.1f %.1f %.1f) damage=%d dir=(%.2f %.2f %.2f) normal=(%.2f %.2f %.2f)",
               hitPoint.x, hitPoint.y, hitPoint.z, damage,
               hitDirection.x, hitDirection.y, hitDirection.z,
               hitNormal.x, hitNormal.y, hitNormal.z);
}

void HitEffects::updateHitBursts(float dt)
{
    (void)dt;
    gGlobalTick++;
    for (int i = 0; i < gBurstCount; ++i) {
        HitBurstEffect& b = gBursts[i];
        if (!b.alive) continue;
        int age = gGlobalTick - b.spawnTick;
        if (age >= b.totalTicks) {
            b.alive = false;
            if (i < gBurstCount - 1)
                b = gBursts[gBurstCount - 1];
            gBurstCount--;
            i--;
        }
    }
}

void HitEffects::clearHitBursts()
{
    gBurstCount = 0;
}

int HitEffects::activeBurstCount()
{
    return gBurstCount;
}

int HitEffects::debugBurstCount()
{
    return gBurstCount;
}

int HitEffects::collectBurstSnapshots(HitBurstSnapshot* out, int maxCount)
{
    int count = 0;
    for (int i = 0; i < gBurstCount && count < maxCount; ++i) {
        const HitBurstEffect& b = gBursts[i];
        out[count].position = b.position;
        out[count].ageTicks = gGlobalTick - b.spawnTick;
        out[count].totalTicks = b.totalTicks;
        out[count].alive = b.alive;
        count++;
    }
    return count;
}
