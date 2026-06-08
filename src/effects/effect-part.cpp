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

unsigned int bloodGridHash(const glm::ivec3& cell)
{
    const unsigned int x = (unsigned int)cell.x * 73856093u;
    const unsigned int y = (unsigned int)cell.y * 19349663u;
    const unsigned int z = (unsigned int)cell.z * 83492791u;
    return (x ^ y ^ z) & 511u;
}

glm::ivec3 bloodGridCell(const glm::vec3& position)
{
    constexpr float CELL_SIZE = 0.5f;
    return glm::ivec3(glm::floor(position / CELL_SIZE));
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
    printf("[EFFECT PART] Initialized pool size=%u\n", POOL_SIZE);
}

void EffectPartSystem::update(float dt) {
    const GameAPI* gameAPI = HotReloadSystem::instance().gameAPI();
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
        }
        return;
    }

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
    glm::vec3 velocity = glm::length(direction) > 0.001f
        ? glm::normalize(direction)
        : glm::vec3(0.0f, 0.0f, 1.0f);
    EffectPart particle;
    particle.position = position;
    particle.velocity = velocity * (5.0f + std::max(amount, 0.0f) * 4.0f);
    particle.replayType = "blood_sphere_particle";
    particle.color = {0.35f, 0.005f, 0.01f};
    particle.scale = 0.06f;
    particle.endScale = 0.02f;
    particle.maxLifetime = 1.0f;
    particle.gravity = 25.0f;
    particle.affectedByGravity = true;
    particle.billboardText = false;
    spawn(particle);
}

void EffectPartSystem::spawnStickyBlood(glm::vec3 position, glm::vec3 normal, float force, unsigned int ownerId) {
    force = std::clamp(force, 0.35f, 1.5f);
    // bool highForce = force >= 0.7f;
    // const int bigCount = 1;
    // const int smallCount = highForce ? 10 : 7;

    const int bigCount =
        1 + (int)(force * 3.0f);

    const int smallCount =
        4 + (int)(force * 18.0f);

    glm::vec3 n = glm::length(normal) > 0.001f ? glm::normalize(normal) : glm::vec3(0,0,1);
    constexpr float MERGE_RADIUS = 0.3f;
    constexpr unsigned int GRID_BUCKETS = 512;
    std::array<int, GRID_BUCKETS> bucketHeads{};
    std::array<int, POOL_SIZE> bucketNext{};
    bucketHeads.fill(-1);
    bucketNext.fill(-1);

    unsigned int stickyCount = 0;
    for (unsigned int i = 0; i < POOL_SIZE; ++i) {
        const EffectPart& existing = mPool[i];
        if (!existing.alive || !existing.cylinderDecal)
            continue;
        ++stickyCount;
        if (!existing.mergeableBlood)
            continue;
        const unsigned int bucket = bloodGridHash(bloodGridCell(existing.position));
        bucketNext[i] = bucketHeads[bucket];
        bucketHeads[bucket] = (int)i;
    }

    for (int i = 0; i < bigCount + smallCount && stickyCount < MAX_STICKY_BLOOD; ++i) {
        const bool big = i < bigCount;
        // const glm::vec3 newPos = position + n * (0.012f + (rand() % 5) * 0.001f);
        // randomized stuff 6 7 2026 
        glm::vec3 tangent =
        glm::normalize(
            std::abs(n.z) < 0.9f
                ? glm::cross(n, glm::vec3(0,0,1))
                : glm::cross(n, glm::vec3(0,1,0)));

        glm::vec3 bitangent =
            glm::normalize(glm::cross(n, tangent));

        float randomAngle =
            ((float)(rand() % 6283) / 1000.0f);

        // float radius =
        //     big
        //         ? ((rand() % 1001) / 1000.0f) * 0.08f
        //         : ((rand() % 1001) / 1000.0f) * 0.45f;
    
        // better for higher force = more bloods 
        float spread =
            0.08f + force * 0.85f;

        float radius =
            big
                ? ((rand() % 1001) / 1000.0f) * spread * 0.2f
                : ((rand() % 1001) / 1000.0f) * spread;

        glm::vec3 offset =
            tangent * std::cos(randomAngle) * radius +
            bitangent * std::sin(randomAngle) * radius;

        const glm::vec3 newPos =
            position +
            offset +
            n * (0.012f + (rand() % 5) * 0.001f);

        bool merged = false;
        if (big) {
            const glm::ivec3 centerCell = bloodGridCell(newPos);
            for (int z = -1; z <= 1 && !merged; ++z)
            for (int y = -1; y <= 1 && !merged; ++y)
            for (int x = -1; x <= 1 && !merged; ++x) {
                const unsigned int bucket = bloodGridHash(centerCell + glm::ivec3(x, y, z));
                for (int index = bucketHeads[bucket]; index >= 0; index = bucketNext[index]) {
                    EffectPart& existing = mPool[(unsigned int)index];
                    if (!existing.alive || !existing.mergeableBlood || existing.ownerId != ownerId)
                        continue;
                    if (glm::length(existing.position - newPos) >= MERGE_RADIUS)
                        continue;
                    if (glm::dot(existing.normal, n) <= 0.9f)
                        continue;
                    existing.scale = std::min(existing.scale * 1.12f, 2.5f);
                    existing.endScale = existing.scale;
                    merged = true;
                    break;
                }
            }
        }
        if (merged)
            continue;

        EffectPart e;
        e.position = newPos;
        e.normal = n;
        e.rotation = {
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360),
            (float)(rand() % 721 - 360)
        };
        e.replayType = "blood_cylinder";
        e.maxLifetime = big ? 30.0f : 5.0f;
        e.lifetime = 0.0f;
        const float variation = 0.85f + (rand() % 301) / 1000.0f;
        const float bigScale = (0.28f + force * 0.42f) * variation;
        e.scale = big ? bigScale : bigScale * 0.125f;
        e.endScale = e.scale;
        e.billboardText = false;
        e.sticky = true;
        e.cylinderDecal = true;
        e.mergeableBlood = big;
        e.cylinderHeight = 0.01f;
        e.ownerId = ownerId;
        e.debugVisual = false;
        e.color = big
            ? glm::vec3(0.32f, 0.004f, 0.008f)
            : glm::vec3(0.68f, 0.012f, 0.02f);

        EffectPart* spawned = spawn(e);
        if (!spawned)
            break;
        ++stickyCount;
    }
}

void EffectPartSystem::spawnProjectedBlood(glm::vec3 hitPosition, glm::vec3 direction, float damage, float distance, const std::string& bodyPart, const World& world) {
    float bodyPartLethality = 1.0f;
    if (bodyPart == "head") bodyPartLethality = 2.0f;
    else if (bodyPart.find("Arm") != std::string::npos) bodyPartLethality = 0.6f;
    else if (bodyPart.find("Leg") != std::string::npos) bodyPartLethality = 0.7f;
    else if (bodyPart == "torso") bodyPartLethality = 1.2f;
    
    const float distanceMultiplier = std::clamp(1.0f - distance / 110.0f, 0.15f, 1.0f);
    const float force = std::clamp(
        damage / 100.0f * bodyPartLethality * distanceMultiplier,
        0.2f,
        1.5f);
    const glm::vec3 forward = glm::length(direction) > 0.001f
        ? glm::normalize(direction)
        : glm::vec3(0.0f, 0.0f, -1.0f);
    const glm::vec3 tangent = glm::normalize(
        std::fabs(forward.z) < 0.9f
            ? glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f))
            : glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 bitangent = glm::normalize(glm::cross(forward, tangent));

    constexpr int SUBSTEPS = 32;
    constexpr float STEP_DT = 1.0f / 30.0f;
    constexpr float BLOOD_GRAVITY = 24.0f;
    const int trajectoryCount = force >= 0.7f ? 5 : 3;
    // const float launchSpeed = 7.0f + force * 18.0f;

    // 6 7 2026 istol /
    /**
     * ow:

        pistol weak shot → nearby floor
        shotgun close range → entire wall painted
     */
    const float launchSpeed =
        4.0f + force * 42.0f;
    // const float coneSpread = 0.08f + force * 0.24f;

    // 6 7 2026 
    const float coneSpread =
        0.04f + force * 0.65f;

    mBloodDebugSegmentCount = 0;
    if (DebugConfig::DEBUG_BLOOD_FORCE) {
        printf(
            "[BLOOD FORCE] damage=%.1f part=%s partMult=%.2f distance=%.2f "
            "distanceMult=%.2f force=%.3f speed=%.2f trajectories=%d\n",
            damage,
            bodyPart.c_str(),
            bodyPartLethality,
            distance,
            distanceMultiplier,
            force,
            launchSpeed,
            trajectoryCount);
    }

    for (int trajectory = 0; trajectory < trajectoryCount; ++trajectory) {
        const float randomAngle = (float)(rand() % 6284) / 1000.0f;
        const float randomRadius = trajectory == 0
            ? 0.0f
            : ((float)(rand() % 1001) / 1000.0f) * coneSpread;
        glm::vec3 launchDirection =
            forward +
            tangent * std::cos(randomAngle) * randomRadius +
            bitangent * std::sin(randomAngle) * randomRadius;
        launchDirection = glm::normalize(launchDirection);

        glm::vec3 position = hitPosition + forward * 0.08f;
        glm::vec3 velocity = launchDirection * launchSpeed;
        float totalDistance = 0.0f;
        bool placedDecal = false;

        for (int step = 0; step < SUBSTEPS; ++step) {
            const glm::vec3 nextPosition = position + velocity * STEP_DT;
            BloodWorldHit worldHit;
            const bool hitWorld = traceBloodSegment(world, position, nextPosition, worldHit);

            if (mBloodDebugSegmentCount < MAX_BLOOD_DEBUG_SEGMENTS) {
                BloodDebugSegment& debug = mBloodDebugSegments[mBloodDebugSegmentCount++];
                debug.from = position;
                debug.to = hitWorld ? worldHit.position : nextPosition;
                debug.normal = hitWorld ? worldHit.normal : glm::vec3(0.0f);
                debug.hit = hitWorld;
            }

            totalDistance += hitWorld
                ? glm::length(worldHit.position - position)
                : glm::length(nextPosition - position);

            if (hitWorld) {
                const bool gravityFloorImpact =
                    worldHit.normal.z > 0.65f && velocity.z < 0.0f;
                spawnStickyBlood(
                    worldHit.position + worldHit.normal * 0.012f,
                    worldHit.normal,
                    force * (0.75f + (rand() % 501) / 1000.0f),
                    0);
                if (DebugConfig::DEBUG_BLOOD_HITS) {
                    printf(
                        "[BLOOD HIT] surface=%s normal=(%.3f %.3f %.3f) "
                        "travel=%.3f gravityFloor=%d\n",
                        worldHit.surfaceType,
                        worldHit.normal.x,
                        worldHit.normal.y,
                        worldHit.normal.z,
                        totalDistance,
                        (int)gravityFloorImpact);
                }
                placedDecal = true;
                break;
            }

            position = nextPosition;
            velocity.z -= BLOOD_GRAVITY * STEP_DT;
        }

        if (!placedDecal && DebugConfig::DEBUG_BLOOD_HITS) {
            printf(
                "[BLOOD HIT] surface=none travel=%.3f decal=aborted\n",
                totalDistance);
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
        // count = 18 + rand() % 11;
        // 6 7 2026 
        count =
            12 +
            (int)(force * 60.0f) +
            rand() % 20;

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
        // p.velocity = velDir * (14.0f + force * 10.0f + (rand() % 4001) / 1000.0f);

        // 6 7 2026 
        // aww its like 
        // p.anchored = true
        // p.color = color3.new(0,1,1)
        p.velocity =
            velDir *
            (
                6.0f +
                force * 35.0f +
                (rand() % 4001) / 1000.0f
            );
        p.color = {0.35f, 0.01f, 0.02f};
        // p.color = {0.95f, 0.01f, 0.02f};
        p.maxLifetime = 0.6f + (rand() % 401) / 1000.0f;
        p.lifetime = 0.0f;
        p.scale = 0.04f + force * 0.06f + (rand() % 51) / 1000.0f;
        p.endScale = p.scale * 0.5f;
        p.alpha = 1.0f;
        // p.gravity = 25.0f;
        // 6 7 2026 
        p.gravity =
            40.0f - force * 18.0f;
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
        particle.lifetime = 0.0f;
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
}

#endif
