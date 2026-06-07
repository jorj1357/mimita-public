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
#include "replay/replay.h"

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
        if (effect.lifetime < 0.0f)
            continue;
        if (!effect.sticky)
            effect.position += effect.velocity * dt;
        if (effect.affectedByGravity)
            effect.velocity.z -= (effect.gravity > 0.0f ? effect.gravity : 9.81f) * dt;
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
    e.replayType = "damage_number";
    e.scale = 0.24f;
    return spawn(e);
}

void EffectPartSystem::spawnBlood(glm::vec3 position, glm::vec3 direction, float amount) {
    spawnStickyBlood(position, -direction, amount);
}

void EffectPartSystem::spawnStickyBlood(glm::vec3 position, glm::vec3 normal, float force, unsigned int ownerId) {
    force = std::clamp(force, 0.35f, 1.5f);
    bool highForce = force >= 0.7f;
    int bigCount = highForce ? 8 : 4;
    int smallCount = highForce ? 36 : 18;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0,0,1);
    glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0,0,1))
                                                             : glm::cross(n, glm::vec3(0,1,0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));
    for (int i = 0; i < bigCount + smallCount; ++i) {
        bool big = i < bigCount;
        float angle = i * 2.399963f + ((rand() % 1000) / 1000.0f - 0.5f) * 0.8f;
        float radius = (big ? 0.16f * i : 0.35f + 0.08f * (i - bigCount));
        radius *= 0.8f + (rand() % 401) / 1000.0f;
        EffectPart e;
        e.position = position + tangent * std::cos(angle) * radius + bitangent * std::sin(angle) * radius
                   + n * (0.006f + (rand() % 8) * 0.001f);
        e.normal = n;
        e.rotation = {
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360)
        };
        e.replayType = "blood_cylinder";
        e.color = {1.0f, 0.015f, 0.025f};
        e.maxLifetime = 30.0f;
        e.lifetime = -(0.01f + (rand() % 91) * 0.001f);
        e.scale = big ? (0.65f + force * 0.55f) : (0.18f + force * 0.22f);
        e.endScale = e.scale;
        e.billboardText = false;
        e.sticky = true;
        e.cylinderDecal = true;
        e.cylinderHeight = 0.01f;
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
    float force = std::clamp(damage / 100.0f * bodyPartLethality * distanceFactor, 0.35f, 1.5f);
    
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
    
    // Fallback: search a short cone around the bullet direction for nearby world geometry.
    if (hitPoints.empty()) {
        const glm::vec3 offsets[] = {
            glm::vec3(0.25f, 0, 0), glm::vec3(-0.25f, 0, 0),
            glm::vec3(0, 0.25f, 0), glm::vec3(0, -0.25f, 0),
            glm::vec3(0, 0, 0.25f), glm::vec3(0, 0, -0.25f)
        };
        float bestDistance = 2.5f;
        for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
            glm::vec3 triCenter = (tri.a + tri.b + tri.c) / 3.0f;
            for (const glm::vec3& offset : offsets) {
                float candidateDistance = glm::length(triCenter - (hitPosition + offset));
                if (candidateDistance < bestDistance) {
                    bestDistance = candidateDistance;
                    hitPoints = {triCenter};
                    hitNormals = {tri.normal};
                }
            }
        }
    }

    if (hitPoints.size() > 1) {
        size_t nearestIndex = 0;
        float nearestDistance = glm::length(hitPoints[0] - rayStart);
        for (size_t i = 1; i < hitPoints.size(); ++i) {
            float candidateDistance = glm::length(hitPoints[i] - rayStart);
            if (candidateDistance < nearestDistance) {
                nearestDistance = candidateDistance;
                nearestIndex = i;
            }
        }
        glm::vec3 nearestPoint = hitPoints[nearestIndex];
        glm::vec3 nearestNormal = hitNormals[nearestIndex];
        hitPoints = {nearestPoint};
        hitNormals = {nearestNormal};
    }

    // Spawn blood on all hit surfaces
    for (size_t s = 0; s < hitPoints.size(); ++s) {
        glm::vec3 n = glm::normalize(hitNormals[s]);
        glm::vec3 pos = hitPoints[s] + n * 0.01f;
        spawnStickyBlood(pos, n, force, 0);
    }
    
}

void EffectPartSystem::spawnBloodSpurt(
    glm::vec3 position,
    glm::vec3 direction,
    const std::string& sourceActorId,
    const std::string& targetActorId)
{
    glm::vec3 forward = glm::length(direction) > 0.001f
        ? glm::normalize(direction)
        : glm::vec3(0.0f, 1.0f, 0.0f);

    ReplayEffectEvent emitter;
    emitter.type = "blood_spurt_emitter";
    emitter.position = position;
    emitter.direction = forward;
    emitter.lifetime = 0.2f;
    emitter.color = glm::vec4(0.85f, 0.0f, 0.015f, 1.0f);
    emitter.sourceActorId = sourceActorId;
    emitter.targetActorId = targetActorId;
    captureReplayEffect(emitter);

    for (int i = 0; i < 2; ++i) {
        glm::vec3 randomSpread{
            (rand() % 2001 - 1000) / 2200.0f,
            (rand() % 2001 - 1000) / 2200.0f,
            0.25f + (rand() % 751) / 1000.0f
        };
        glm::vec3 velocityDirection = glm::normalize(forward * 0.75f + randomSpread);

        EffectPart particle;
        particle.position = position;
        particle.replayType = "blood_sphere_particle";
        particle.velocity = velocityDirection * (2.5f + (rand() % 2501) / 1000.0f);
        particle.color = {0.85f, 0.0f, 0.015f};
        particle.maxLifetime = 3.0f;
        particle.lifetime = -0.1f * (float)i;
        particle.scale = 0.075f + (rand() % 41) / 1000.0f;
        particle.endScale = particle.scale * 0.35f;
        particle.alpha = 1.0f;
        particle.gravity = 9.81f;
        particle.affectedByGravity = true;
        particle.billboardText = false;
        particle.sourceActorId = sourceActorId;
        particle.targetActorId = targetActorId;
        spawn(particle);
    }
}

EffectPart* EffectPartSystem::spawnEntityImpact(
    glm::vec3 position,
    glm::vec3 normal,
    const std::string& sourceActorId,
    const std::string& targetActorId)
{
    EffectPart effect;
    effect.position = position;
    effect.normal = normal;
    effect.replayType = "impact_entity";
    effect.color = {0.9f, 0.02f, 0.02f};
    effect.maxLifetime = 0.18f;
    effect.scale = 0.12f;
    effect.endScale = 0.4f;
    effect.billboardText = false;
    effect.sticky = true;
    effect.sourceActorId = sourceActorId;
    effect.targetActorId = targetActorId;
    return spawn(effect);
}

EffectPart* EffectPartSystem::spawnWorldImpact(glm::vec3 position, glm::vec3 normal) {
    EffectPart e;
    e.position = position;
    e.normal = normal;
    e.replayType = "impact_world";
    e.color = {0.55f, 0.55f, 0.55f};
    e.maxLifetime = 0.5f;
    e.scale = 0.1f;
    e.endScale = 5.0f;
    e.alpha = 0.5f;
    e.billboardText = false;
    e.sticky = true;
    EffectPart* spawned = spawn(e);
    return spawned;
}

EffectPart* EffectPartSystem::spawnMuzzleFlash(glm::vec3 position, const std::string& sourceActorId) {
    EffectPart e;
    e.position = position;
    e.replayType = "muzzle_flash";
    e.color = {1.0f, 1.0f, 1.0f};
    e.maxLifetime = 0.1f;
    e.scale = 0.5f;
    e.endScale = 0.35f;
    e.billboardText = false;
    e.sticky = true;
    e.sourceActorId = sourceActorId;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnTracer(glm::vec3 start, glm::vec3 end, const std::string& sourceActorId) {
    EffectPart e;
    e.position = start;
    e.replayType = "tracer";
    e.endPosition = end;
    e.color = {1.0f, 0.82f, 0.05f};
    e.maxLifetime = 0.5f;
    e.scale = 0.2f;
    e.endScale = 0.0f;
    e.thickness = 0.2f;
    e.endThickness = 0.0f;
    e.billboardText = false;
    e.sticky = true;
    e.beam = true;
    e.sourceActorId = sourceActorId;
    return spawn(e);
}

EffectPart* EffectPartSystem::spawnBulletImpact(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.replayType = "impact_sphere";
    e.color = {0.55f, 0.55f, 0.58f};
    e.maxLifetime = 0.25f;
    e.scale = 0.1f;
    e.endScale = 1.0f;
    e.billboardText = false;
    e.sticky = true;
    return spawn(e);
}

void EffectPartSystem::spawnWorldDebris(glm::vec3 position, glm::vec3 normal) {
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0, 0, 1);
    for (int i = 0; i < 12; ++i) {
        glm::vec3 randomDir{
            (rand() % 2001 - 1000) / 1000.0f,
            (rand() % 2001 - 1000) / 1000.0f,
            0.35f + (rand() % 651) / 1000.0f
        };
        randomDir = glm::normalize(randomDir + n * 0.8f);
        EffectPart e;
        e.position = position + n * 0.04f;
        e.replayType = "debris_block";
        e.velocity = randomDir * (1.5f + (rand() % 2001) / 1000.0f);
        e.color = {0.42f, 0.40f, 0.38f};
        e.maxLifetime = 1.0f;
        e.alpha = 1.0f;
        float size = 0.35f + (rand() % 501) / 1000.0f;
        e.halfSize = glm::vec3(size);
        e.rotation = {
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360)
        };
        e.billboardText = false;
        e.gravity = 9.81f;
        e.affectedByGravity = true;
        e.box = true;
        spawn(e);
    }
}

void EffectPartSystem::destroyOwner(unsigned int ownerId) {
    mEffects.erase(std::remove_if(mEffects.begin(), mEffects.end(),
        [ownerId](const EffectPart& e) { return e.ownerId == ownerId; }), mEffects.end());
}

EffectPart* EffectPartSystem::spawn(const EffectPart& effect) {
    ReplayEffectEvent event;
    event.type = effect.replayType;
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
    captureReplayEffect(event);

    mEffects.push_back(effect);
    return &mEffects.back();
}

EffectPart* EffectPartSystem::spawnFootstep(glm::vec3 position) {
    EffectPart e;
    e.position = position;
    e.replayType = "footstep";
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
    e.replayType = "dash";
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

void EffectPartSystem::clear() {
    mEffects.clear();
}

void EffectPartSystem::render(const Camera& camera) const {
    if (mEffects.empty()) return;
    
    // Render spheres for each effect
    for (const auto& effect : mEffects) {
        if (!effect.alive) continue;
        if (effect.lifetime < 0.0f) continue;
        if (effect.debugVisual && !DebugVis::masterEnabled()) continue;
        
        float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
        float alpha = effect.alpha * (1.0f - t);
        alpha = std::max(0.0f, alpha);
        float drawScale = effect.scale + (effect.endScale - effect.scale) * t;
        
        glm::vec4 drawColor{effect.color.x, effect.color.y, effect.color.z, alpha};
        
        // Cylinder-style blood decal — filled decal aligned to surface normal
        if (effect.beam) {
            DebugVis::drawFilledBeam(camera, effect.position, effect.endPosition, drawScale, drawColor);
        }
        else if (effect.box) {
            DebugVis::drawFilledBox(camera, effect.position, effect.halfSize, drawColor);
        }
        else if (effect.cylinderDecal) {
            DebugVis::drawFilledCylinder(camera, effect.position, effect.normal, drawScale, effect.cylinderHeight, drawColor);
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
