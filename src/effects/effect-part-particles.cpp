#include "effect-part.h"
#include "entities/player.h"
#include "world/world.h"
#include "effects/hit-effects.h"
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

void EffectPartSystem::updateBloodDecals(float dt) {
    for (BloodDecal& decal : mBloodDecals) {
        decal.age += dt;
        const float fade = std::clamp((decal.age - 25.0f) / 5.0f, 0.0f, 1.0f);
        decal.alpha = 1.0f - fade;
    }
    mBloodDecals.erase(
        std::remove_if(
            mBloodDecals.begin(),
            mBloodDecals.end(),
            [](const BloodDecal& decal) { return decal.age >= decal.lifetime; }),
        mBloodDecals.end());
}
