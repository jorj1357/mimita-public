#include "effect-part.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "camera.h"
#include <algorithm>
#include <cstdio>

EffectPartSystem& EffectPartSystem::instance() {
    static EffectPartSystem sInstance;
    return sInstance;
}

void EffectPartSystem::init() {
    printf("[EFFECT PART] Initialized\n");
}

void EffectPartSystem::update(float dt) {
    for (auto& effect : mEffects) {
        effect.lifetime += dt;
        effect.position += effect.velocity * dt;
        effect.velocity.z -= 4.0f * dt;
        if (effect.lifetime >= effect.maxLifetime) {
            effect.alive = false;
        }
    }
    
    mEffects.erase(
        std::remove_if(mEffects.begin(), mEffects.end(),
            [](const EffectPart& e) { return !e.alive; }),
        mEffects.end()
    );
}

EffectPart* EffectPartSystem::spawnDamage(glm::vec3 position, const std::string& victim, int damage) {
    EffectPart e;
    e.position = position;
    e.color = {1.0f, 0.0f, 0.0f};
    e.maxLifetime = 1.0f;
    e.label = victim + " took " + std::to_string(damage) + " damage!!";
    e.scale = 0.24f;
    return spawn(e);
}

void EffectPartSystem::spawnBlood(glm::vec3 position, glm::vec3 direction, float amount) {
    int count = std::clamp((int)(amount * 8.0f), 2, 14);
    glm::vec3 base = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(1,0,0);
    for (int i = 0; i < count; ++i) {
        float side = ((i % 3) - 1) * 0.22f;
        EffectPart e;
        e.position = position;
        e.velocity = base * (1.5f + amount * 3.0f + i * 0.08f) + glm::vec3(-base.y, base.x, 0.3f) * side;
        e.color = {0.75f, 0.0f, 0.02f};
        e.maxLifetime = 0.5f + amount * 0.5f;
        e.scale = 0.035f + amount * 0.04f;
        e.billboardText = false;
        spawn(e);
    }
}

EffectPart* EffectPartSystem::spawn(const EffectPart& effect) {
    mEffects.push_back(effect);
    return &mEffects.back();
}

EffectPart* EffectPartSystem::spawnFootstep(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.color = {1.0f, 1.0f, 1.0f};
    e.maxLifetime = 0.5f;
    e.label = "walk()";
    e.scale = 0.15f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnDash(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.color = {0.2f, 0.6f, 1.0f};
    e.maxLifetime = 0.8f;
    e.label = "dash()";
    e.scale = 0.25f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnFreeze(glm::vec3 position, float freezeDuration) {
    EffectPart e;
    e.position = position;
    e.color = {0.2f, 1.0f, 0.3f};
    e.maxLifetime = 0.1f;
    char buf[64];
    snprintf(buf, sizeof(buf), "freeze(%.2f)", freezeDuration);
    e.label = buf;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnImpact(glm::vec3 position, glm::vec3 color, const char* label) {
    EffectPart e;
    e.position = position;
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
    e.color = color;
    e.maxLifetime = lifetime;
    e.label = label;
    e.scale = 0.2f;
    e.billboardText = true;
    return spawn(e);
}

void EffectPartSystem::clear() {
    mEffects.clear();
}

void EffectPartSystem::render(const Camera& camera) const {
    if (mEffects.empty()) return;
    
    // Render spheres for each effect
    for (const auto& effect : mEffects) {
        if (!effect.alive) continue;
        
        float alpha = 1.0f - (effect.lifetime / effect.maxLifetime);
        alpha = std::max(0.0f, alpha);
        
        // Draw sphere using debug visuals
        DebugVis::drawWireSphere(camera, effect.position, effect.scale, 
            {effect.color.x, effect.color.y, effect.color.z, alpha});
        
        // Draw billboard text label
        if (effect.billboardText && !effect.label.empty()) {
            float x, y;
            if (DebugVis::projectToScreen(camera, effect.position + glm::vec3(0, 0, effect.scale + 0.15f), x, y)) {
                glm::vec4 textColor = {effect.color.x, effect.color.y, effect.color.z, alpha};
                uiDrawText(effect.label.c_str(), x, y, 0.3f * effect.scale, textColor);
            }
        }
    }
}
