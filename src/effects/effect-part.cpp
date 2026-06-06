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