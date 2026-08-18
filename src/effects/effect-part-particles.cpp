// 08 17 2026, 14 20
/* purpose
* Updates pooled blood particles, surface decals, and queued blood spawns.
* Applies fade and optional color darkening without changing decal geometry.
* Does NOT own hit detection, config file parsing, or world rendering.
*/
#include "effect-part.h"
#include "entities/player.h"
#include "world/world.h"
#include "effects/hit-effects.h"
#include "config/impact-decals-config.h"
#include "debug/debug-log.h"
#include "config.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>

static float randomSignedRange(float amount)
{
    if (amount <= 0.0f) return 0.0f;
    const float t = (float)std::rand() / (float)RAND_MAX;
    return (t * 2.0f - 1.0f) * amount;
}

EffectPart* EffectPartSystem::spawnDamage(glm::vec3 position, const std::string& victim, int damage) {
    const auto& cfg = HitEffects::config();
    const auto& dn = cfg.damageNumber;
    if (!cfg.core.damageNumbers || !dn.enabled) return nullptr;
    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=effect-part-particles.cpp Type=damage_number pos=(%.1f,%.1f,%.1f) damage=%d victim=%s\n",
                   position.x, position.y, position.z, damage, victim.c_str());
    }

    const float horizontalSpread = dn.randomHorizontalSpread + dn.spawnJitter;
    const float verticalSpread = dn.randomVerticalSpread + dn.spawnJitter;
    glm::vec3 jitter{
        randomSignedRange(horizontalSpread),
        randomSignedRange(horizontalSpread),
        randomSignedRange(verticalSpread)
    };

    EffectPart e;
    e.position = position + glm::vec3(dn.worldOffsetX, dn.worldOffsetY, dn.worldOffsetZ) + jitter;
    e.color = damage < 0 ? dn.healingColor : (damage >= 100 ? dn.criticalColor : dn.textColor);
    e.velocity = glm::vec3(dn.moveX, dn.moveY, dn.moveZ) * dn.moveSpeed;
    e.maxLifetime = std::max(0.01f, dn.lifetime);
    e.lifetime = -std::max(0.0f, dn.spawnDelay);
    e.label = damage < 0 ? ("+" + std::to_string(-damage)) : std::to_string(damage);
    e.replayType = "damage_number";
    e.billboardText = true;
    e.scale = std::max(0.0f, dn.startScale);
    e.endScale = std::max(0.0f, dn.endScale);
    e.alpha = std::clamp(dn.startOpacity, 0.0f, 1.0f);
    return spawn(e);
}

void EffectPartSystem::updateBloodParticles(float dt) {
    const glm::vec3 bloodGravity(0.0f, 0.0f, -2.5f);
    constexpr float BLOOD_AIR_DRAG = 0.97f;
    for (BloodParticle& particle : mBloodParticles) {
        particle.position += particle.velocity * dt;
        particle.velocity += bloodGravity * dt;
        particle.velocity *= std::pow(BLOOD_AIR_DRAG, dt * 60.0f);
        particle.age += dt;
        float fadeStart = particle.lifetime * 0.4f;
        if (particle.age > fadeStart) {
            particle.alpha = std::clamp(1.0f - (particle.age - fadeStart) / (particle.lifetime - fadeStart), 0.0f, 1.0f);
        }
    }
    mBloodParticles.erase(
        std::remove_if(
            mBloodParticles.begin(),
            mBloodParticles.end(),
            [](const BloodParticle& particle) { return particle.age >= particle.lifetime; }),
        mBloodParticles.end());
}

void EffectPartSystem::updateSurfaceDecals(float dt) {
    for (SurfaceDecal& decal : mSurfaceDecals) {
        decal.age += dt;
        const float holdTime = std::max(0.0f, decal.lifetime - decal.fadeTime);
        if (decal.age > holdTime && decal.fadeTime > 0.0f) {
            const float fadeT = std::clamp((decal.age - holdTime) / decal.fadeTime, 0.0f, 1.0f);
            decal.alpha = decal.baseAlpha * (1.0f - fadeT);
        }
        if (decal.darkenOverLifetime) {
            const float span = std::max(0.001f, decal.darkenEndSeconds - decal.darkenStartSeconds);
            const float darkenT = std::clamp((decal.age - decal.darkenStartSeconds) / span, 0.0f, 1.0f);
            decal.color = glm::mix(decal.colorStart, decal.colorEnd, darkenT);
        }
    }
    mSurfaceDecals.erase(
        std::remove_if(
            mSurfaceDecals.begin(),
            mSurfaceDecals.end(),
            [](const SurfaceDecal& decal) { return decal.age >= decal.lifetime; }),
        mSurfaceDecals.end());
}

void EffectPartSystem::updatePendingBloodDecals(float dt) {
    (void)dt;
    const auto& cfg = ImpactDecalsConfig::instance().data().blood.stagger;
    if (!cfg.enabled) { mPendingBloodDecals.clear(); return; }
    const int budget = std::max(1, cfg.decalsPerTick);
    int spawned = 0;
    for (auto it = mPendingBloodDecals.begin(); it != mPendingBloodDecals.end() && spawned < budget;) {
        ++it->ageTicks;
        if (it->ageTicks <= cfg.startDelayTicks) { ++it; continue; }
        if (it->ageTicks > cfg.maxTicks) { it = mPendingBloodDecals.erase(it); continue; }
        const auto& blood = ImpactDecalsConfig::instance().data().blood;
        pushSurfaceDecal(it->decal, blood.maxCount);
        it = mPendingBloodDecals.erase(it);
        ++spawned;
    }
}
