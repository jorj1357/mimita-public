#include "effect-part.h"
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "camera.h"
#include "world/world.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "audio/audio.h"

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
        if (!effect.sticky)
            effect.position += effect.velocity * dt;
        if (effect.affectedByGravity)
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
    spawnStickyBlood(position, -direction, amount);
}

void EffectPartSystem::spawnStickyBlood(glm::vec3 position, glm::vec3 normal, float force, unsigned int ownerId) {
    bool highForce = force >= 0.55f;
    int bigCount = highForce ? 5 : 1;
    int smallCount = highForce ? 25 : 5;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0,0,1);
    glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0,0,1))
                                                             : glm::cross(n, glm::vec3(0,1,0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
    for (int i = 0; i < bigCount + smallCount; ++i) {
        bool big = i < bigCount;
        float angle = i * 2.399963f;
        float radius = big ? 0.08f * i : 0.12f + 0.025f * (i - bigCount);
        EffectPart e;
        e.position = position + tangent * std::cos(angle) * radius + bitangent * std::sin(angle) * radius + n * 0.003f;
        e.normal = n;
        e.color = {0.75f, 0.0f, 0.02f};
        e.maxLifetime = 30.0f;
        // CHANGED: Increased scale for bigger blood splats, jun 6 2026
        e.scale = big ? (0.5f + force * 0.5f) : (0.1f + force * 0.15f);
        e.endScale = e.scale;
        e.billboardText = false;
        e.sticky = true;
        e.flatDecal = true;
        e.ownerId = ownerId;
        // CHANGED: No longer debug-only — renders always as solid decal, jun 6 2026
        e.debugVisual = false;
        spawn(e);
    }
}

void EffectPartSystem::spawnProjectedBlood(glm::vec3 hitPosition, glm::vec3 direction, float damage, float distance, const std::string& bodyPart, const World& world) {
    // Calculate force from damage, distance, body part lethality
    float bodyPartLethality = 1.0f;
    if (bodyPart == "head") bodyPartLethality = 2.0f;
    else if (bodyPart.find("Arm") != std::string::npos) bodyPartLethality = 0.6f;
    else if (bodyPart.find("Leg") != std::string::npos) bodyPartLethality = 0.7f;
    else if (bodyPart == "torso") bodyPartLethality = 1.2f;
    
    float distanceFactor = std::clamp(1.0f - distance / 110.0f, 0.1f, 1.0f);
    float force = std::clamp(damage / 100.0f * bodyPartLethality * distanceFactor, 0.1f, 1.0f);
    
    // Raycast from hit position forward into world to find surfaces behind target
    const float RAY_LENGTH = 3.0f;
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(0,0,-1);
    glm::vec3 rayStart = hitPosition + dir * 0.1f;
    
    std::vector<glm::vec3> hitPoints;
    std::vector<glm::vec3> hitNormals;
    
    // Check world collision mesh triangles
    for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
        glm::vec3 e1 = tri.b - tri.a;
        glm::vec3 e2 = tri.c - tri.a;
        glm::vec3 p = glm::cross(dir, e2);
        float det = glm::dot(e1, p);
        if (std::fabs(det) < 0.000001f) continue;
        float inv = 1.0f / det;
        glm::vec3 tVec = rayStart - tri.a;
        float u = glm::dot(tVec, p) * inv;
        if (u < 0.0f || u > 1.0f) continue;
        glm::vec3 q = glm::cross(tVec, e1);
        float v = glm::dot(dir, q) * inv;
        if (v < 0.0f || u + v > 1.0f) continue;
        float t = glm::dot(e2, q) * inv;
        if (t > 0.001f && t < RAY_LENGTH) {
            hitPoints.push_back(rayStart + dir * t);
            hitNormals.push_back(tri.normal);
        }
    }
    
    // Check AABB blocks
    for (const Block& block : world.blocks) {
        glm::vec3 mn = block.pos - block.size * 0.5f;
        glm::vec3 mx = block.pos + block.size * 0.5f;
        float tmin = 0.0f;
        float tmax = RAY_LENGTH;
        glm::vec3 normal(0.0f);
        bool hit = true;
        for (int axis = 0; axis < 3; ++axis) {
            if (std::fabs(dir[axis]) < 0.000001f) {
                if (rayStart[axis] < mn[axis] || rayStart[axis] > mx[axis]) { hit = false; break; }
                continue;
            }
            float invD = 1.0f / dir[axis];
            float a = (mn[axis] - rayStart[axis]) * invD;
            float b = (mx[axis] - rayStart[axis]) * invD;
            float sign = -1.0f;
            if (a > b) { std::swap(a, b); sign = 1.0f; }
            if (a > tmin) { tmin = a; normal = glm::vec3(0.0f); normal[axis] = sign; }
            tmax = std::min(tmax, b);
            if (tmin > tmax) { hit = false; break; }
        }
        if (hit && tmin > 0.001f && tmin < RAY_LENGTH) {
            hitPoints.push_back(rayStart + dir * tmin);
            hitNormals.push_back(normal);
        }
    }
    
    // Fallback: if no surface found, use hit position
    if (hitPoints.empty()) {
        hitPoints.push_back(hitPosition);
        hitNormals.push_back(glm::vec3(0, 0, 1));
    }
    
    // Spawn blood on all hit surfaces
    for (size_t s = 0; s < hitPoints.size(); ++s) {
        glm::vec3 n = glm::normalize(hitNormals[s]);
        glm::vec3 pos = hitPoints[s] + n * 0.01f;
        spawnStickyBlood(pos, n, force, 0);
    }
    
    // Also spawn some blood at original hit point
    spawnStickyBlood(hitPosition + glm::vec3(0, 0, 0.01f), glm::vec3(0, 0, 1), force * 0.5f, 0);
}

EffectPart* EffectPartSystem::spawnWorldImpact(glm::vec3 position, glm::vec3 normal) {
    EffectPart e;
    e.position = position;
    e.normal = normal;
    e.color = {0.55f, 0.55f, 0.55f};
    e.maxLifetime = 0.5f;
    e.scale = 0.1f;
    e.endScale = 0.5f;
    e.alpha = 0.5f;
    e.billboardText = false;
    e.sticky = true;
    EffectPart* spawned = spawn(e);
    AudioManager::instance().play({"world_impact", AudioCategory::Impacts, true, position, 0.7f, 1.0f, 30.0f});
    return spawned;
}

void EffectPartSystem::destroyOwner(unsigned int ownerId) {
    mEffects.erase(std::remove_if(mEffects.begin(), mEffects.end(),
        [ownerId](const EffectPart& e) { return e.ownerId == ownerId; }), mEffects.end());
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
    e.scale = 0.18f;
    e.endScale = 0.06f;
    e.billboardText = false;
    e.flatDecal = false;
    e.sticky = true;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnDash(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.color = {0.2f, 0.6f, 1.0f};
    e.maxLifetime = 0.8f;
    e.scale = 0.35f;
    e.endScale = 0.1f;
    e.billboardText = false;
    e.flatDecal = false;
    e.sticky = true;
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
        if (effect.debugVisual && !DebugVis::masterEnabled()) continue;
        
        float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
        float alpha = effect.alpha * (1.0f - t);
        alpha = std::max(0.0f, alpha);
        float drawScale = effect.scale + (effect.endScale - effect.scale) * t;
        
        glm::vec4 drawColor{effect.color.x, effect.color.y, effect.color.z, alpha};
        
        // Cylinder-style blood decal — filled decal aligned to surface normal
        if (effect.cylinderDecal) {
            DebugVis::drawFilledDecal(camera, effect.position, effect.normal, drawScale, drawColor);
        }
        // Flat decal (blood splats on surfaces)
        else if (effect.flatDecal) {
            DebugVis::drawFilledDecal(camera, effect.position, effect.normal, drawScale, drawColor);
        }
        // Solid filled sphere (footsteps, dash effects)
        else {
            DebugVis::drawFilledSphere(camera, effect.position, drawScale, drawColor);
        }
        
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
