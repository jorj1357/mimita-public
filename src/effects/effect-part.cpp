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
#include "debug/debug-visuals.h"
#include "gui/ui-system.h"
#include "camera.h"
#include "world/world.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "audio/audio.h"
#include "config.h"
#include "replay/replay.h"
#include "hot-reload/hot-reload-system.h"

namespace {

struct BloodWorldHit {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    const char* surfaceType = "none";
    float distance = 0.0f;
};

bool rayTriangleSegment(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const CollisionTriangle& tri,
    float& distance)
{
    const glm::vec3 e1 = tri.b - tri.a;
    const glm::vec3 e2 = tri.c - tri.a;
    const glm::vec3 p = glm::cross(direction, e2);
    const float determinant = glm::dot(e1, p);
    if (std::fabs(determinant) < 0.000001f)
        return false;

    const float inverseDeterminant = 1.0f / determinant;
    const glm::vec3 offset = origin - tri.a;
    const float u = glm::dot(offset, p) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f)
        return false;

    const glm::vec3 q = glm::cross(offset, e1);
    const float v = glm::dot(direction, q) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f)
        return false;

    distance = glm::dot(e2, q) * inverseDeterminant;
    return distance >= 0.0f && distance <= maxDistance;
}

bool rayAabbSegment(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const glm::vec3& minimum,
    const glm::vec3& maximum,
    float& distance,
    glm::vec3& normal)
{
    float minimumTime = 0.0f;
    float maximumTime = maxDistance;
    normal = glm::vec3(0.0f);

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(direction[axis]) < 0.000001f) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                return false;
            continue;
        }

        const float inverseDirection = 1.0f / direction[axis];
        float nearTime = (minimum[axis] - origin[axis]) * inverseDirection;
        float farTime = (maximum[axis] - origin[axis]) * inverseDirection;
        float normalSign = -1.0f;
        if (nearTime > farTime) {
            std::swap(nearTime, farTime);
            normalSign = 1.0f;
        }
        if (nearTime > minimumTime) {
            minimumTime = nearTime;
            normal = glm::vec3(0.0f);
            normal[axis] = normalSign;
        }
        maximumTime = std::min(maximumTime, farTime);
        if (minimumTime > maximumTime)
            return false;
    }

    distance = minimumTime;
    return distance >= 0.0f && distance <= maxDistance;
}

bool raySphereSegment(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float maxDistance,
    const Sphere& sphere,
    float& distance,
    glm::vec3& normal)
{
    const glm::vec3 offset = origin - sphere.pos;
    const float b = glm::dot(offset, direction);
    const float c = glm::dot(offset, offset) - sphere.radius * sphere.radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f)
        return false;

    distance = -b - std::sqrt(discriminant);
    if (distance < 0.0f || distance > maxDistance)
        return false;
    normal = glm::normalize(origin + direction * distance - sphere.pos);
    return true;
}

bool traceBloodSegment(
    const World& world,
    const glm::vec3& from,
    const glm::vec3& to,
    BloodWorldHit& hit)
{
    const glm::vec3 delta = to - from;
    const float segmentLength = glm::length(delta);
    if (segmentLength < 0.0001f)
        return false;

    const glm::vec3 direction = delta / segmentLength;
    float nearest = segmentLength;
    bool found = false;

    for (const CollisionTriangle& triangle : world.collisionMesh.triangles) {
        float distance = 0.0f;
        if (!rayTriangleSegment(from, direction, nearest, triangle, distance))
            continue;
        nearest = distance;
        hit.normal = triangle.normal;
        hit.surfaceType = "triangle";
        found = true;
    }

    for (const Block& block : world.blocks) {
        float distance = 0.0f;
        glm::vec3 normal(0.0f);
        const glm::vec3 halfSize = block.size * 0.5f;
        if (!rayAabbSegment(
                from, direction, nearest,
                block.pos - halfSize, block.pos + halfSize,
                distance, normal))
            continue;
        nearest = distance;
        hit.normal = normal;
        hit.surfaceType = "block";
        found = true;
    }

    for (const Sphere& sphere : world.spheres) {
        float distance = 0.0f;
        glm::vec3 normal(0.0f);
        if (!raySphereSegment(from, direction, nearest, sphere, distance, normal))
            continue;
        nearest = distance;
        hit.normal = normal;
        hit.surfaceType = "sphere";
        found = true;
    }

    if (!found)
        return false;

    hit.position = from + direction * nearest;
    hit.distance = nearest;
    if (glm::dot(hit.normal, direction) > 0.0f)
        hit.normal = -hit.normal;
    hit.normal = glm::normalize(hit.normal);
    return true;
}

}

EffectPartSystem& EffectPartSystem::instance() {
    static EffectPartSystem sInstance;
    return sInstance;
}

void EffectPartSystem::init() {
    for (auto& slot : mPool)
        slot.alive = false;
    mActiveCount = 0;
    mBloodParticles.clear();
    mBloodParticles.reserve(MAX_BLOOD_PARTICLES);
    mBloodDecals.clear();
    mBloodDecals.reserve(MAX_BLOOD_DECALS);
    printf("[EFFECT PART] Initialized pool size=%u\n", POOL_SIZE);
}

void EffectPartSystem::update(float dt) {
    const GameAPI* gameAPI = HotReloadSystem::instance().gameAPI();
    bool updatedByGameDLL = false;
    if (gameAPI && gameAPI->updateEffects) {
        std::array<GameEffectPartState, POOL_SIZE> states{};
        for (unsigned int i = 0; i < POOL_SIZE; ++i) {
            const EffectPart& effect = mPool[i];
            GameEffectPartState& state = states[i];
            state.position[0] = effect.position.x;
            state.position[1] = effect.position.y;
            state.position[2] = effect.position.z;
            state.velocity[0] = effect.velocity.x;
            state.velocity[1] = effect.velocity.y;
            state.velocity[2] = effect.velocity.z;
            state.lifetime = effect.lifetime;
            state.maxLifetime = effect.maxLifetime;
            state.gravity = effect.gravity;
            state.alive = effect.alive;
            state.sticky = effect.sticky;
            state.affectedByGravity = effect.affectedByGravity;
        }

        gameAPI->updateEffects(
            &HotReloadSystem::instance().gameMemory(), states.data(), POOL_SIZE, dt);

        for (unsigned int i = 0; i < POOL_SIZE; ++i) {
            EffectPart& effect = mPool[i];
            const GameEffectPartState& state = states[i];
            if (!effect.alive)
                continue;
            effect.position = {state.position[0], state.position[1], state.position[2]};
            effect.velocity = {state.velocity[0], state.velocity[1], state.velocity[2]};
            effect.lifetime = state.lifetime;
            if (!state.alive) {
                effect.alive = false;
                effect.resetStrings();
                --mActiveCount;
            }
            if (glm::length(effect.angularVelocity) > 0.0f)
                effect.rotation += effect.angularVelocity * dt;
        }
        updatedByGameDLL = true;
    }

    if (!updatedByGameDLL) {
        for (auto& fx : mPool) {
            if (!fx.alive)
                continue;
            fx.lifetime += dt;
            if (fx.lifetime < 0.0f)
                continue;
            if (!fx.sticky) {
                fx.position += fx.velocity * dt;
                if (fx.affectedByGravity)
                    fx.velocity.z -= (fx.gravity > 0.0f ? fx.gravity : 9.81f) * dt;
            }
            if (glm::length(fx.angularVelocity) > 0.0f)
                fx.rotation += fx.angularVelocity * dt;
            if (fx.lifetime >= fx.maxLifetime) {
                fx.alive = false;
                fx.resetStrings();
                --mActiveCount;
            }
        }
    }

    const glm::vec3 bloodGravity(0.0f, 0.0f, -18.0f);
    for (BloodParticle& particle : mBloodParticles) {
        particle.position += particle.velocity * dt;
        particle.velocity += bloodGravity * dt;
        particle.age += dt;
        particle.alpha = std::clamp(1.0f - particle.age / particle.lifetime, 0.0f, 1.0f);
    }
    mBloodParticles.erase(
        std::remove_if(
            mBloodParticles.begin(),
            mBloodParticles.end(),
            [](const BloodParticle& particle) { return particle.age >= particle.lifetime; }),
        mBloodParticles.end());

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

void EffectPartSystem::spawnBloodEffect(
    glm::vec3 hitPoint,
    glm::vec3 sprayDirection,
    float damage,
    const std::string& sourceActorId,
    const std::string& targetActorId)
{
    damage = std::max(0.0f, damage);
    const glm::vec3 forward = glm::length(sprayDirection) > 0.001f
        ? glm::normalize(sprayDirection)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 tangent = glm::normalize(
        std::fabs(forward.z) < 0.9f
            ? glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f))
            : glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 bitangent = glm::normalize(glm::cross(forward, tangent));
    const int particleCount = std::clamp((int)std::round(damage * 2.0f), 10, 50);
    const float coneDegrees = 30.0f + std::clamp(damage / 100.0f, 0.0f, 1.0f) * 30.0f;
    const float coneRadius = std::tan(glm::radians(coneDegrees));
    const float damageScale = std::clamp(damage / 100.0f, 0.0f, 2.0f);

    if (mBloodParticles.size() + (size_t)particleCount > MAX_BLOOD_PARTICLES) {
        const size_t removeCount =
            mBloodParticles.size() + (size_t)particleCount - MAX_BLOOD_PARTICLES;
        mBloodParticles.erase(
            mBloodParticles.begin(),
            mBloodParticles.begin() + (std::min)(removeCount, mBloodParticles.size()));
    }

    for (int i = 0; i < particleCount; ++i) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float radial = std::sqrt((float)(rand() % 1001) / 1000.0f) * coneRadius;
        const glm::vec3 direction = glm::normalize(
            forward +
            tangent * std::cos(angle) * radial +
            bitangent * std::sin(angle) * radial);

        BloodParticle particle;
        particle.position = hitPoint + direction * 0.03f;
        particle.velocity = direction *
            (5.0f + damageScale * 5.0f + (float)(rand() % 4001) / 1000.0f);
        particle.size = 0.025f + (float)(rand() % 501) / 10000.0f +
            damageScale * 0.015f;
        particle.lifetime = 0.3f + (float)(rand() % 301) / 1000.0f;
        particle.alpha = 0.75f + (float)(rand() % 251) / 1000.0f;
        particle.rotation = (float)(rand() % 6284) / 1000.0f;
        particle.stretch = 0.7f + (float)(rand() % 901) / 1000.0f;
        mBloodParticles.push_back(particle);
    }

    ReplayEffectEvent emitter;
    emitter.type = "blood_spurt_emitter";
    emitter.position = hitPoint;
    emitter.direction = forward;
    emitter.lifetime = 0.6f;
    emitter.color = glm::vec4(0.95f, 0.02f, 0.04f, 1.0f);
    emitter.sourceActorId = sourceActorId;
    emitter.targetActorId = targetActorId;
    captureReplayEffect(emitter);

    if (!mWorld) {
        if (DebugConfig::DEBUG_BLOOD_HITS)
            printf("[BLOOD DECAL] skipped world=null\n");
        return;
    }

    struct SurfaceCluster {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
        glm::vec3 rayDirection{0.0f};
        int hitCount = 0;
    };
    std::array<SurfaceCluster, 3> clusters{};
    int clusterCount = 0;
    const int rayCount = std::clamp(16 + (int)std::round(damage * 0.48f), 16, 64);
    const float castDistance = std::clamp(4.0f + damage * 0.08f, 4.0f, 18.0f);
    mBloodDebugSegmentCount = 0;

    for (int ray = 0; ray < rayCount; ++ray) {
        const float angle = (float)(rand() % 6284) / 1000.0f;
        const float radial = ray == 0
            ? 0.0f
            : std::sqrt((float)(rand() % 1001) / 1000.0f) * coneRadius;
        const glm::vec3 rayDirection = glm::normalize(
            forward +
            tangent * std::cos(angle) * radial +
            bitangent * std::sin(angle) * radial);
        const glm::vec3 start = hitPoint + forward * 0.05f;
        const glm::vec3 end = start + rayDirection * castDistance;
        BloodWorldHit hit;
        const bool hitWorld = traceBloodSegment(*mWorld, start, end, hit);

        if (mBloodDebugSegmentCount < MAX_BLOOD_DEBUG_SEGMENTS) {
            BloodDebugSegment& debug = mBloodDebugSegments[mBloodDebugSegmentCount++];
            debug.from = start;
            debug.to = hitWorld ? hit.position : end;
            debug.normal = hitWorld ? hit.normal : glm::vec3(0.0f);
            debug.hit = hitWorld;
        }
        if (!hitWorld)
            continue;

        int clusterIndex = -1;
        for (int cluster = 0; cluster < clusterCount; ++cluster) {
            const glm::vec3 center = clusters[cluster].position /
                (float)clusters[cluster].hitCount;
            const glm::vec3 normal = glm::normalize(clusters[cluster].normal);
            if (glm::length(center - hit.position) <= 1.5f &&
                glm::dot(normal, hit.normal) >= 0.65f) {
                clusterIndex = cluster;
                break;
            }
        }
        if (clusterIndex < 0) {
            if (clusterCount >= (int)clusters.size())
                continue;
            clusterIndex = clusterCount++;
        }

        SurfaceCluster& cluster = clusters[clusterIndex];
        cluster.position += hit.position;
        cluster.normal += hit.normal;
        cluster.rayDirection += rayDirection;
        ++cluster.hitCount;
    }

    for (int clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex) {
        const SurfaceCluster& cluster = clusters[clusterIndex];
        if (cluster.hitCount <= 0)
            continue;

        const glm::vec3 normal = glm::normalize(cluster.normal);
        const glm::vec3 rayDirection = glm::normalize(cluster.rayDirection);
        const float impactAngle = std::clamp(
            std::fabs(glm::dot(-rayDirection, normal)), 0.15f, 1.0f);
        const float variation = 0.8f + (float)(rand() % 401) / 1000.0f;

        BloodDecal decal;
        decal.position =
            cluster.position / (float)cluster.hitCount + normal * 0.006f;
        decal.normal = normal;
        decal.radius = std::clamp(
            (0.16f + damage * 0.012f) *
                (0.7f + impactAngle * 0.6f) * variation,
            0.16f,
            2.5f);
        decal.lifetime = 30.0f;
        decal.rotation = (float)(rand() % 6284) / 1000.0f;
        decal.stretch = 1.0f + (1.0f - impactAngle) * 1.5f;
        decal.alpha = 0.78f + (float)(rand() % 181) / 1000.0f;

        if (mBloodDecals.size() >= MAX_BLOOD_DECALS)
            mBloodDecals.erase(mBloodDecals.begin());
        mBloodDecals.push_back(decal);

        ReplayEffectEvent decalEvent;
        decalEvent.type = "blood_splatter";
        decalEvent.position = decal.position;
        decalEvent.normal = decal.normal;
        decalEvent.scale = glm::vec3(decal.radius, decal.radius * decal.stretch, 0.01f);
        decalEvent.rotation = glm::vec3(0.0f, 0.0f, decal.rotation);
        decalEvent.color = glm::vec4(0.82f, 0.015f, 0.025f, decal.alpha);
        decalEvent.lifetime = decal.lifetime;
        decalEvent.sourceActorId = sourceActorId;
        decalEvent.targetActorId = targetActorId;
        captureReplayEffect(decalEvent);
    }

    if (DebugConfig::DEBUG_BLOOD_HITS) {
        printf("[BLOOD SPAWN] particles=%d damage=%.1f cone=%.1f\n",
               particleCount, damage, coneDegrees);
        printf("[BLOOD DECAL] rays=%d clusters=%d active=%zu\n",
               rayCount, clusterCount, mBloodDecals.size());
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

void EffectPartSystem::spawnWorldDebris(glm::vec3 position, glm::vec3 normal, float force) {
    force = std::max(force, 0.1f);
    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0, 0, 1);

    glm::vec3 tangent = glm::normalize(
        std::abs(n.z) < 0.9f
            ? glm::cross(n, glm::vec3(0, 0, 1))
            : glm::cross(n, glm::vec3(0, 1, 0)));
    glm::vec3 bitangent = glm::normalize(glm::cross(n, tangent));

    const int count = 24 + (int)(force * 8.0f) + rand() % 8;
    const float coneSpread = 0.8f + force * 1.0f;
    const float baseSpeed = 4.0f + force * 12.0f;
    const float lifetimeBase = 0.5f + force * 0.15f;

    for (int i = 0; i < count; ++i) {
        float randomAngle = (float)(rand() % 6283) / 1000.0f;
        float randomRadius = ((float)(rand() % 1001) / 1000.0f) * coneSpread;
        glm::vec3 dir = n
            + tangent * std::cos(randomAngle) * randomRadius
            + bitangent * std::sin(randomAngle) * randomRadius;
        dir = glm::normalize(dir);

        float speed = baseSpeed + (rand() % 5001) / 1000.0f;
        float sx = 0.04f + force * 0.04f + (rand() % 501) / 3000.0f;
        float sy = 0.04f + force * 0.04f + (rand() % 501) / 3000.0f;
        float sz = 0.04f + force * 0.04f + (rand() % 501) / 3000.0f;

        EffectPart e;
        e.position = position + n * 0.04f + dir * (0.02f + (rand() % 51) / 1000.0f);
        e.replayType = "debris_block";
        e.velocity = dir * speed;
        e.angularVelocity = {
            (float)(rand() % 2001 - 1000) / 80.0f * (1.0f + force * 0.5f),
            (float)(rand() % 2001 - 1000) / 80.0f * (1.0f + force * 0.5f),
            (float)(rand() % 2001 - 1000) / 80.0f * (1.0f + force * 0.5f)
        };
        e.color = {0.42f, 0.40f, 0.38f};
        e.maxLifetime = lifetimeBase + (rand() % 501) / 1000.0f;
        e.alpha = 1.0f;
        e.halfSize = {sx, sy, sz};
        e.rotation = {
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360)
        };
        e.billboardText = false;
        e.gravity = 15.0f;
        e.affectedByGravity = true;
        e.lifetime = 0.0f;
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
    mBloodParticles.clear();
    mBloodDecals.clear();
    mBloodDebugSegmentCount = 0;
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
            DebugVis::drawFilledBox(camera, effect.position, effect.halfSize, drawColor, effect.rotation);
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

    for (const BloodParticle& particle : mBloodParticles) {
        const float dist = glm::length(particle.position - camera.pos);
        if (dist > 40.0f)
            continue;
        const float distFade = dist > 20.0f ? (40.0f - dist) / 20.0f : 1.0f;
        DebugVis::drawFilledBillboard(
            camera,
            particle.position,
            particle.size,
            particle.rotation,
            particle.stretch,
            {0.92f, 0.015f, 0.025f, particle.alpha * distFade});
    }

    for (const BloodDecal& decal : mBloodDecals) {
        const float dist = glm::length(decal.position - camera.pos);
        if (dist > 60.0f)
            continue;
        const float distFade = dist > 40.0f ? (60.0f - dist) / 20.0f : 1.0f;
        DebugVis::drawBloodDecal(
            camera,
            decal.position,
            decal.normal,
            decal.radius,
            decal.rotation,
            decal.stretch,
            {0.78f, 0.01f, 0.02f, decal.alpha * distFade});
    }

    // Blood performance metrics
    if (DebugConfig::DEBUG_BLOOD_HITS) {
        static float bloodPerfTimer = 0.0f;
        bloodPerfTimer -= 0.016f;
        if (bloodPerfTimer <= 0.0f) {
            bloodPerfTimer = 2.0f;
            printf("[BLOOD PERF] particles=%zu decals=%zu collisionUpdates=0\n",
                   mBloodParticles.size(), mBloodDecals.size());
        }
    }

    if (DebugConfig::DEBUG_BLOOD_RAYS) {
        for (unsigned int i = 0; i < mBloodDebugSegmentCount; ++i) {
            const BloodDebugSegment& segment = mBloodDebugSegments[i];
            DebugVis::drawLine(
                camera,
                segment.from,
                segment.to,
                segment.hit
                    ? glm::vec4(1.0f, 0.25f, 0.05f, 1.0f)
                    : glm::vec4(0.8f, 0.02f, 0.04f, 0.85f));
        }
    }
    if (DebugConfig::DEBUG_BLOOD_HITS) {
        for (unsigned int i = 0; i < mBloodDebugSegmentCount; ++i) {
            const BloodDebugSegment& segment = mBloodDebugSegments[i];
            if (!segment.hit)
                continue;
            DebugVis::drawPointCross(camera, segment.to, 0.12f, {1.0f, 1.0f, 0.0f, 1.0f});
            DebugVis::drawLine(
                camera,
                segment.to,
                segment.to + segment.normal * 0.5f,
                {0.2f, 1.0f, 0.2f, 1.0f});
        }
    }

    if (DebugConfig::DEBUG_DEBRIS) {
        int debrisCount = 0;
        glm::vec3 centroid{0.0f};
        for (const auto& effect : mPool) {
            if (!effect.alive || !effect.box)
                continue;
            centroid += effect.position;
            ++debrisCount;
            float velLen = glm::length(effect.velocity);
            if (velLen > 0.01f) {
                DebugVis::drawLine(
                    camera,
                    effect.position,
                    effect.position + glm::normalize(effect.velocity) * std::min(velLen * 0.15f, 2.0f),
                    {1.0f, 0.9f, 0.0f, 0.85f});
            }
            DebugVis::drawPointCross(camera, effect.position, 0.04f, {1.0f, 0.6f, 0.0f, 0.8f});
        }
        if (debrisCount > 0) {
            centroid /= (float)debrisCount;
            char dbgLabel[64];
            snprintf(dbgLabel, sizeof(dbgLabel), "debris:%d", debrisCount);
            DebugVis::drawWorldLabel(centroid + glm::vec3(0, 0, 0.3f), dbgLabel, {1.0f, 0.9f, 0.0f, 1.0f});
        }
    }
}

#endif
