#include "effect-part.h"
#include "entities/player.h"
#include "world/world.h"
#include "effects/hit-effects.h"
#include "debug/debug-log.h"
#include "config.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

EffectPart* EffectPartSystem::spawnDamage(glm::vec3 position, const std::string& victim, int damage) {
    if (!HitEffects::config().core.damageNumbers) return nullptr;
    if (gHitFxTraceEnabled) {
        Debug::log(Debug::Category::NpcCombat, "[HITFX TRACE] Source=effect-part-particles.cpp Type=damage_number pos=(%.1f,%.1f,%.1f) damage=%d victim=%s\n",
                   position.x, position.y, position.z, damage, victim.c_str());
    }
    EffectPart e;
    e.position = position;
    e.color = {1.0f, 0.0f, 0.0f};
    e.maxLifetime = 1.0f;
    e.label = victim + " took " + std::to_string(damage) + " damage!!";
    e.replayType = "damage_number";
    e.scale = 0.24f;
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
