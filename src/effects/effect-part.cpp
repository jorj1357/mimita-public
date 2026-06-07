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
    for (auto& slot : mPool)
        slot.alive = false;
    mActiveCount = 0;
    printf("[EFFECT PART] Initialized pool size=%u\n", POOL_SIZE);
}

void EffectPartSystem::update(float dt) {
    for (auto& fx : mPool) {
        if (!fx.alive) continue;
        fx.lifetime += dt;
        if (fx.lifetime < 0.0f)
            continue;
        if (!fx.sticky)
            fx.position += fx.velocity * dt;
        if (fx.affectedByGravity)
            fx.velocity.z -= (fx.gravity > 0.0f ? fx.gravity : 9.81f) * dt;
        if (fx.lifetime >= fx.maxLifetime) {
            fx.alive = false;
            fx.resetStrings();
            --mActiveCount;
        }
    }
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
    glm::vec3 n = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(0,0,1);
    n += glm::vec3(
        (rand() % 201 - 100) / 1000.0f,
        (rand() % 201 - 100) / 1000.0f,
        (rand() % 201 - 100) / 1000.0f
    );
    n = glm::normalize(n);
    spawnStickyBlood(position, -n, amount * 1.2f);
}

void EffectPartSystem::spawnStickyBlood(glm::vec3 position, glm::vec3 normal, float force, unsigned int ownerId) {
    force = std::clamp(force, 0.35f, 1.5f);
    bool highForce = force >= 0.7f;
    int bigCount = highForce ? 16 : 8;
    int smallCount = 0;
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0,0,1);
    glm::vec3 tangent = glm::normalize(std::fabs(n.z) < 0.9f ? glm::cross(n, glm::vec3(0,0,1))
                                                             : glm::cross(n, glm::vec3(0,1,0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));

    constexpr float MERGE_RADIUS = 0.3f;

    for (int i = 0; i < bigCount + smallCount; ++i) {
        bool big = i < bigCount;
        float angle = i * 2.399963f + ((rand() % 1000) / 1000.0f - 0.5f) * 0.8f;
        float radius = (big ? 0.16f * i : 0.35f + 0.08f * (i - bigCount));
        radius *= 0.8f + (rand() % 401) / 1000.0f;
        glm::vec3 newPos = position + tangent * std::cos(angle) * radius + bitangent * std::sin(angle) * radius
                   + n * (0.015f + (rand() % 12) * 0.002f);

        bool merged = false;
        for (auto& existing : mPool) {
            if (!existing.alive || !existing.cylinderDecal) continue;
            if (existing.ownerId != ownerId) continue;
            float dist = glm::length(existing.position - newPos);
            if (dist < MERGE_RADIUS && glm::dot(existing.normal, n) > 0.9f) {
                existing.scale *= 1.12f;
                existing.position = (existing.position + newPos) * 0.5f;
                merged = true;
                break;
            }
        }
        if (merged) continue;

        EffectPart e;
        e.position = newPos;
        e.normal = n;
        e.rotation = {
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360)
        };
        e.replayType = "blood_cylinder";
        e.color = {1.0f, 0.015f, 0.025f};
        e.maxLifetime = 30.0f;
        // e.maxLifetime = 5.0f;
        e.lifetime = -(0.01f + (rand() % 91) * 0.001f);
        e.scale = big ? (0.65f + force * 0.55f) : (0.18f + force * 0.22f);
        e.endScale = e.scale;
        e.billboardText = false;
        e.sticky = true;
        e.cylinderDecal = true;
        e.cylinderHeight = 0.01f;
        e.ownerId = ownerId;
        e.debugVisual = false;
        spawn(e);
    }
}

void EffectPartSystem::spawnProjectedBlood(glm::vec3 hitPosition, glm::vec3 direction, float damage, float distance, const std::string& bodyPart, const World& world) {
    float bodyPartLethality = 1.0f;
    if (bodyPart == "head") bodyPartLethality = 2.0f;
    else if (bodyPart.find("Arm") != std::string::npos) bodyPartLethality = 0.6f;
    else if (bodyPart.find("Leg") != std::string::npos) bodyPartLethality = 0.7f;
    else if (bodyPart == "torso") bodyPartLethality = 1.2f;
    
    float distanceFactor = std::clamp(1.0f - distance / 110.0f, 0.1f, 1.0f);
    float force = std::clamp(damage / 100.0f * bodyPartLethality * distanceFactor, 0.35f, 1.5f);
    
    glm::vec3 dir = glm::length(direction) > 0.001f ? glm::normalize(direction) : glm::vec3(0,0,-1);
    glm::vec3 rayStart = hitPosition + dir * 0.25f;
    
    constexpr float MAX_RAY = 25.0f;
    constexpr int CONE_RAYS = 24;
    constexpr float CONE_ANGLE = 0.6f;
    
    auto traceRay = [&](glm::vec3 origin, glm::vec3 d, float& outT, glm::vec3& outNormal) -> bool {
        float bestT = MAX_RAY;
        bool hit = false;
        glm::vec3 bestNormal;
        for (const CollisionTriangle& tri : world.collisionMesh.triangles) {
            glm::vec3 e1 = tri.b - tri.a;
            glm::vec3 e2 = tri.c - tri.a;
            glm::vec3 p = glm::cross(d, e2);
            float det = glm::dot(e1, p);
            if (std::fabs(det) < 0.000001f) continue;
            float inv = 1.0f / det;
            glm::vec3 tVec = origin - tri.a;
            float u = glm::dot(tVec, p) * inv;
            if (u < 0.0f || u > 1.0f) continue;
            glm::vec3 q = glm::cross(tVec, e1);
            float v = glm::dot(d, q) * inv;
            if (v < 0.0f || u + v > 1.0f) continue;
            float t = glm::dot(e2, q) * inv;
            if (t > 0.01f && t < bestT) {
                bestT = t;
                bestNormal = tri.normal;
                hit = true;
            }
        }
        for (const Block& block : world.blocks) {
            glm::vec3 mn = block.pos - block.size * 0.5f;
            glm::vec3 mx = block.pos + block.size * 0.5f;
            float tmin = 0.0f, tmax = MAX_RAY;
            glm::vec3 normal(0.0f);
            bool h = true;
            for (int axis = 0; axis < 3; ++axis) {
                if (std::fabs(d[axis]) < 0.000001f) {
                    if (origin[axis] < mn[axis] || origin[axis] > mx[axis]) { h = false; break; }
                    continue;
                }
                float invD = 1.0f / d[axis];
                float a = (mn[axis] - origin[axis]) * invD;
                float b = (mx[axis] - origin[axis]) * invD;
                float sign = -1.0f;
                if (a > b) { std::swap(a, b); sign = 1.0f; }
                if (a > tmin) { tmin = a; normal = glm::vec3(0.0f); normal[axis] = sign; }
                tmax = std::min(tmax, b);
                if (tmin > tmax) { h = false; break; }
            }
            if (h && tmin > 0.01f && tmin < bestT) {
                bestT = tmin;
                bestNormal = normal;
                hit = true;
            }
        }
        outT = bestT;
        outNormal = bestNormal;
        return hit;
    };
    
    glm::vec3 primaryNormal;
    float primaryT;
    if (traceRay(rayStart, dir, primaryT, primaryNormal)) {
        glm::vec3 n = glm::normalize(primaryNormal);
        spawnStickyBlood(rayStart + dir * primaryT + n * 0.02f, n, force * 0.7f, 0);
    }
    
    float seed = (float)(rand() % 10000) / 10000.0f;
    for (int i = 0; i < CONE_RAYS; ++i) {
        float angle1 = seed + (float)i * 2.399963f;
        float angle2 = seed + (float)(i * 7) * 0.618034f;
        glm::vec3 coneDir = dir;
        coneDir.x += std::cos(angle1) * std::sin(angle2) * CONE_ANGLE;
        coneDir.y += std::sin(angle1) * std::sin(angle2) * CONE_ANGLE;
        coneDir.z += (std::cos(angle2) - 1.0f) * CONE_ANGLE;
        coneDir = glm::normalize(coneDir);
        
        float t;
        glm::vec3 normal;
        if (traceRay(rayStart, coneDir, t, normal)) {
            glm::vec3 n = glm::normalize(normal);
            float spreadForce = force * (0.4f + (rand() % 1001) / 1000.0f * 0.6f);
            spawnStickyBlood(rayStart + coneDir * t + n * 0.025f, n, spreadForce, 0);
        }
    }
}

void EffectPartSystem::spawnBloodSphereBurst(
    glm::vec3 hitPoint,
    glm::vec3 shotDirection,
    float force,
    const std::string& sourceActorId,
    const std::string& targetActorId)
{
    force = std::clamp(force, 0.1f, 2.0f);
    int count;
    if (force < 0.3f)
        count = 4 + rand() % 3;
    else if (force < 0.7f)
        count = 8 + rand() % 7;
    else
        count = 18 + rand() % 11;

    glm::vec3 dir = glm::length(shotDirection) > 0.001f
        ? glm::normalize(shotDirection)
        : glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 worldUp(0.0f, 0.0f, 1.0f);
    glm::vec3 perpendicular = glm::normalize(glm::cross(dir, worldUp));
    if (glm::length(perpendicular) < 0.001f)
        perpendicular = glm::normalize(glm::cross(dir, glm::vec3(1.0f, 0.0f, 0.0f)));

    for (int i = 0; i < count; ++i) {
        float coneAngle = 0.4f + force * 0.3f;
        float randomAngle = (float)(rand() % 6283) / 1000.0f;
        float randomRadius = ((float)(rand() % 1001) / 1000.0f) * coneAngle;
        glm::vec3 velDir = dir
            + perpendicular * std::cos(randomAngle) * randomRadius
            + worldUp * std::sin(randomAngle) * randomRadius;
        velDir = glm::normalize(velDir);
        velDir.z = std::max(velDir.z, 0.15f);

        EffectPart p;
        p.position = hitPoint + velDir * (0.03f + (rand() % 31) / 1000.0f);
        p.replayType = "blood_sphere_particle";
        p.velocity = velDir * (14.0f + force * 10.0f + (rand() % 4001) / 1000.0f);
        p.color = {0.35f, 0.01f, 0.02f};
        p.maxLifetime = 0.6f + (rand() % 401) / 1000.0f;
        p.lifetime = -((float)(rand() % 101) / 1000.0f);
        p.scale = 0.04f + force * 0.06f + (rand() % 51) / 1000.0f;
        p.endScale = p.scale * 0.5f;
        p.alpha = 1.0f;
        p.gravity = 25.0f;
        p.affectedByGravity = true;
        p.billboardText = false;
        p.sourceActorId = sourceActorId;
        p.targetActorId = targetActorId;
        spawn(p);
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
    for (int i = 0; i < 16; ++i) {
        glm::vec3 randomDir{
            (rand() % 2001 - 1000) / 1000.0f,
            (rand() % 2001 - 1000) / 1000.0f,
            0.35f + (rand() % 651) / 1000.0f
        };
        randomDir = glm::normalize(randomDir + n * 0.8f);
        EffectPart e;
        e.position = position + n * 0.04f + randomDir * (0.02f + (rand() % 51) / 1000.0f);
        e.replayType = "debris_block";
        e.velocity = randomDir * (2.0f + (rand() % 3001) / 1000.0f);
        e.color = {0.42f, 0.40f, 0.38f};
        e.maxLifetime = 1.5f + (rand() % 1001) / 1000.0f;
        e.alpha = 1.0f;
        float sx = 0.12f + (rand() % 501) / 1000.0f;
        float sy = 0.12f + (rand() % 501) / 1000.0f;
        float sz = 0.12f + (rand() % 501) / 1000.0f;
        e.halfSize = {sx, sy, sz};
        e.rotation = {
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360)
        };
        e.billboardText = false;
        e.gravity = 9.81f;
        e.affectedByGravity = true;
        e.lifetime = -((float)(rand() % 51) / 1000.0f);
        e.box = true;
        spawn(e);
    }
}

void EffectPartSystem::destroyOwner(unsigned int ownerId) {
    for (auto& fx : mPool) {
        if (!fx.alive) continue;
        if (fx.ownerId == ownerId) {
            fx.alive = false;
            fx.resetStrings();
            --mActiveCount;
        }
    }
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

    for (auto& slot : mPool) {
        if (!slot.alive) {
            slot = effect;
            slot.alive = true;
            ++mActiveCount;
            return &slot;
        }
    }
    return nullptr;
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
    for (auto& slot : mPool) {
        if (slot.alive) {
            slot.alive = false;
            slot.resetStrings();
        }
    }
    mActiveCount = 0;
}

void EffectPartSystem::render(const Camera& camera) const {
    for (const auto& effect : mPool) {
        if (!effect.alive) continue;
        if (effect.lifetime < 0.0f) continue;
        if (effect.debugVisual && !DebugVis::masterEnabled()) continue;

        float dist = glm::length(effect.position - camera.pos);
        if (dist > 40.0f) continue;
        float distFade = (dist > 20.0f) ? (40.0f - dist) / 20.0f : 1.0f;

        float t = std::clamp(effect.lifetime / effect.maxLifetime, 0.0f, 1.0f);
        float alpha = effect.alpha * (1.0f - t) * distFade;
        alpha = std::max(0.0f, alpha);
        float drawScale = effect.scale + (effect.endScale - effect.scale) * t;
        
        glm::vec4 drawColor{effect.color.x, effect.color.y, effect.color.z, alpha};
        
        if (effect.beam) {
            DebugVis::drawFilledBeam(camera, effect.position, effect.endPosition, drawScale, drawColor);
        }
        else if (effect.box) {
            DebugVis::drawFilledBox(camera, effect.position, effect.halfSize, drawColor);
        }
        else if (effect.cylinderDecal) {
            DebugVis::drawFilledCylinder(camera, effect.position, effect.normal, drawScale, effect.cylinderHeight, drawColor);
        }
        else if (effect.flatDecal) {
            DebugVis::drawFilledDecal(camera, effect.position, effect.normal, drawScale, drawColor);
        }
        else {
            DebugVis::drawFilledSphere(camera, effect.position, drawScale, drawColor);
        }
        
        if (effect.billboardText && !effect.label.empty()) {
            float x, y;
            if (DebugVis::projectToScreen(camera, effect.position + glm::vec3(0, 0, effect.scale + 0.15f), x, y)) {
                glm::vec4 textColor = {effect.color.x, effect.color.y, effect.color.z, alpha};
                uiDrawText(effect.label.c_str(), x, y, 0.3f * effect.scale, textColor);
            }
        }
    }
}
