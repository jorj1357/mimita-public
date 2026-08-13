// 07 21 2026, 23 20
/* purpose
* Owns client-side network projectile prediction, adoption, correction, and rendering.
* Steps rocket and grenade projectiles through the shared projectile physics kernel.
* Processes authoritative projectile spawn, state, explosion, despawn, and attack results.
* Does NOT validate projectile hits, damage, ammo, cooldowns, death, or server authority.
* Does NOT send legacy direct projectile fire packets for active gameplay.
* Does NOT own local single-player rocket or persistent grenade simulation.
*/

#include "network/multiplayer-context.h"
#include "network/simulation-constants.h"
#include "config/networking-config.h"
#include "network/confirmed-damage-presentation.h"
#include "network/network-weapons.h"
#include "network/weapon-runtime-reconciliation.h"
#include "network/disagreement-visuals.h"
#include "npc/npc-combat-log.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/client-collision-world-view.h"
#include "combat/explosion-fx.h"
#include "combat/projectile-render.h"
#include "combat/projectile-simulation.h"
#include "combat/weapon-system.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "effects/effect-part.h"
#include "terminal/terminal-state.h"
#include "world/world.h"
#include "physics/movement/physics-collision.h"

namespace MimitaNet {
namespace {

// ── Grenade diagnostic logging helper ────────────────────────────────
// Writes to both StructuredLogger (category file + summary) and the
// dedicated grenade-*.txt file.  Provides consistent formatting.
static uint64_t gGrenadeSeq = 0;

static void logGrenadeEvent(
    ::StructuredCategory cat,
    ::StructuredLevel level,
    const char* eventId,
    const std::string& correlationId,
    const char* reason,
    const char* fmt, ...)
{
    ::StructuredLogger& logger = ::StructuredLogger::instance();
    if (!logger.shouldLog(cat, level))
        return;

    char msg[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    ::StructuredLogger::Entry e;
    e.category = cat;
    e.level = level;
    e.eventId = eventId;
    e.correlationId = correlationId;
    e.reason = reason ? reason : "";
    e.sourceFile = __FILE__;
    e.sourceLine = __LINE__;
    e.functionName = __FUNCTION__;
    e.message = msg;
    logger.write(e);
    ++gGrenadeSeq;
}

bool finiteVec(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

const WeaponDefinition* projectileDefinition(uint8_t weapon)
{
    const char* id = nullptr;
    if (weapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
        id = "grenade_launcher";
    else if (weapon == NETWORK_WEAPON_ROCKET_LAUNCHER)
        id = "rocket_launcher";
    if (!id)
        return nullptr;
    return WeaponRegistry::instance().get(id);
}

float cp(const WeaponDefinition* def, const char* key, float fallback)
{
    if (!def)
        return fallback;
    auto it = def->customParams.find(key);
    return it != def->customParams.end() ? it->second : fallback;
}

ProjectileVisualConfig projectileVisualConfig(uint8_t weapon)
{
    const bool isRocket = weapon == NETWORK_WEAPON_ROCKET_LAUNCHER;
    const WeaponDefinition* def = projectileDefinition(weapon);
    ProjectileVisualConfig cfg;
    cfg.texturePath = isRocket ? "assets/textureshq/colorful2.png" : "assets/textureshq/meat1.png";
    cfg.length = cp(def, "projectileVisualLength", isRocket ? 1.5f : 1.8f);
    cfg.radius = cp(def, "projectileVisualRadius", isRocket ? 0.18f : 0.28f);
    cfg.scale = glm::vec3(
        cp(def, "projectileVisualScaleX", 1.0f),
        cp(def, "projectileVisualScaleY", 1.0f),
        cp(def, "projectileVisualScaleZ", 1.0f));
    cfg.rotationOffsetDegrees = glm::vec3(
        cp(def, "projectileVisualRotationOffsetX", 0.0f),
        cp(def, "projectileVisualRotationOffsetY", 0.0f),
        cp(def, "projectileVisualRotationOffsetZ", 0.0f));
    cfg.textureTiling = glm::vec2(
        cp(def, "projectileVisualTextureTilingU", 1.0f),
        cp(def, "projectileVisualTextureTilingV", 1.0f));
    cfg.fillAlpha = cp(def, "projectileFillAlpha", 1.0f);
    cfg.outlineEnabled = cp(def, "projectileOutlineEnabled", 1.0f) > 0.0f;
    cfg.outlineColor = glm::vec3(
        cp(def, "projectileOutlineColorR", 1.0f),
        cp(def, "projectileOutlineColorG", 0.8f),
        cp(def, "projectileOutlineColorB", 0.2f));
    cfg.outlineAlpha = cp(def, "projectileOutlineAlpha", 0.4f);
    cfg.outlineScale = cp(def, "projectileOutlineScale", 1.15f);
    cfg.glowEnabled = cp(def, "projectileGlowEnabled", 1.0f) > 0.0f;
    cfg.glowColor = glm::vec3(
        cp(def, "projectileGlowColorR", 1.0f),
        cp(def, "projectileGlowColorG", 0.6f),
        cp(def, "projectileGlowColorB", 0.0f));
    cfg.glowAlpha = cp(def, "projectileGlowAlpha", 0.15f);
    cfg.glowRadiusMultiplier = cp(def, "projectileGlowRadiusMultiplier", 3.0f);
    return cfg;
}

uint32_t provisionalProjectileId(uint32_t requestId)
{
    if (requestId == 0)
        return 0;
    return 0x80000000u | (requestId & 0x7fffffffu);
}

void configureNetworkProjectile(NetworkProjectile& projectile,
                                const WeaponDefinition* def)
{
    if (!def)
        return;
    projectile.lifetime = def->projectileLifetime > 0.0f
        ? def->projectileLifetime
        : projectile.lifetime;
    projectile.radius = def->projectileRadius > 0.0f
        ? def->projectileRadius
        : projectile.radius;
    projectile.gravity = cp(def, "gravity", 20.0f);
    projectile.drag = cp(def, "drag", 0.15f);
    projectile.restitution = cp(def, "bounceRestitution", 0.35f);
    projectile.friction = cp(def, "bounceFriction", 0.5f);
    projectile.armingDistance = cp(def, "armingDistance", 2.0f);
    projectile.armingTime = cp(def, "armingTime", 0.0f);
    projectile.minBounceSpeed = cp(def, "minBounceSpeed", 0.1f);
    projectile.angularDrag = cp(def, "angularDrag", 0.3f);
    projectile.maxBounceCount = (int)cp(def, "maxBounceCount", 10.0f);
    projectile.explodeOnPlayerImpact = cp(def, "explodeOnPlayerImpact", 1.0f) > 0.0f;
    projectile.explodeOnWorldImpact = cp(def, "explodeOnWorldImpact", 0.0f) > 0.0f;
    projectile.explodeOnLifetime = cp(def, "explodeOnLifetime", 1.0f) > 0.0f;
}

ProjectilePhysicsState makePhysicsState(const NetworkProjectile& projectile)
{
    ProjectilePhysicsState state;
    state.position = projectile.position;
    state.velocity = projectile.velocity;
    state.rotation = projectile.rotation;
    state.angularVelocity = projectile.angularVelocity;
    state.age = projectile.age;
    state.bounceCount = projectile.bounceCount;
    state.exploded = projectile.exploded;
    state.sleeping = projectile.worldTouched &&
        glm::length(projectile.velocity) <= 0.0001f &&
        glm::length(projectile.angularVelocity) <= 0.0001f;
    return state;
}

ProjectilePhysicsConfig makePhysicsConfig(const NetworkProjectile& projectile)
{
    ProjectilePhysicsConfig config;
    config.speed = glm::length(projectile.velocity);
    config.radius = projectile.radius;
    config.lifetime = projectile.lifetime;
    config.armingDistance = projectile.armingDistance;
    config.gravity = projectile.gravity;
    config.drag = projectile.drag;
    config.angularDrag = projectile.angularDrag;
    config.restitution = projectile.restitution;
    config.friction = projectile.friction;
    config.maxBounceCount = projectile.maxBounceCount;
    config.minBounceSpeed = projectile.minBounceSpeed;
    config.bounceEnabled = projectile.maxBounceCount > 0;
    return config;
}

void applyPhysicsState(NetworkProjectile& projectile,
                       const ProjectilePhysicsState& state)
{
    projectile.position = state.position;
    projectile.velocity = state.velocity;
    projectile.rotation = state.rotation;
    projectile.angularVelocity = state.angularVelocity;
    projectile.age = state.age;
    projectile.bounceCount = state.bounceCount;
}

std::vector<ClientCollisionWorldView::PlayerReplica> projectileReplicas(
    const MultiplayerContext& ctx)
{
    std::vector<ClientCollisionWorldView::PlayerReplica> replicas;
    replicas.reserve(ctx.remotePlayers.size());
    for (const auto& entry : ctx.remotePlayers)
    {
        const Player& player = entry.second;
        ClientCollisionWorldView::PlayerReplica replica;
        replica.playerId = entry.first;
        replica.spawnGeneration = player.spawnGeneration;
        replica.pos = player.pos;
        replica.dead = player.dead;
        replica.sizeScale = player.sizeScale;
        Capsule capsule = player.getCapsule();
        replica.capsuleRadius = capsule.r;
        replica.capsuleHeight = std::max(0.001f, capsule.b.z - capsule.a.z);
        replica.capsuleCenter = (capsule.a + capsule.b) * 0.5f;
        replicas.push_back(replica);
    }
    return replicas;
}

void removePredictedProjectileForRequest(MultiplayerContext& ctx,
                                         uint32_t requestId)
{
    ctx.predictedExplosions.erase(requestId);
    ctx.predictedSelfKnockbacks.erase(requestId);
    const uint32_t provisionalId = provisionalProjectileId(requestId);
    if (provisionalId != 0)
    {
        ctx.predictedProjectileIds.erase(provisionalId);
        ctx.networkProjectiles.erase(provisionalId);
    }
}

// ── Predicted projectile adoption ─────────────────────────────────────
// Renames the client-predicted projectile (provisional ID) to the authoritative
// server projectile ID, keeping its predicted=true flag and current sim pose so
// the visual never rubber-bands. Returns true when a prediction was adopted.
// Safe to call from any signal (AttackResult, spawn, or state recovery) and at
// any ordering: if the authoritative ID already exists, the predicted duplicate
// is simply dropped. This is what makes the local owner's rocket/grenade a
// single entity even when the unreliable spawn broadcast is lost (badconn).
bool adoptPredictedProjectile(MultiplayerContext& ctx,
                              uint32_t requestId,
                              uint32_t authoritativeId)
{
    const uint32_t provisionalId = provisionalProjectileId(requestId);
    if (provisionalId == 0 || authoritativeId == 0 || provisionalId == authoritativeId)
        return false;
    auto it = ctx.networkProjectiles.find(provisionalId);
    if (it == ctx.networkProjectiles.end())
        return false;
    if (ctx.networkProjectiles.count(authoritativeId) != 0)
    {
        // Recovery/spawn already created the authoritative projectile — drop the
        // predicted duplicate rather than ever showing two.
        ctx.networkProjectiles.erase(it);
        ctx.predictedProjectileIds.erase(provisionalId);
        return false;
    }
    NetworkProjectile projectile = it->second; // keeps predicted=true + sim pose
    ctx.networkProjectiles.erase(it);
    ctx.predictedProjectileIds.erase(provisionalId);
    projectile.projectileId = authoritativeId;
    ctx.networkProjectiles[authoritativeId] = projectile;
    ctx.predictedProjectileIds.insert(authoritativeId);
    printf("[PROJECTILE ADOPT] requestId=%u provisionalId=%u authoritativeId=%u weapon=%s\n",
           requestId, provisionalId, authoritativeId,
           networkWeaponTypeName(projectile.weaponType));
    return true;
}

// ── Client splash line-of-sight (mirrors the server's splashHasLineOfSight) ──
// Same wall-only blocking rule via the shared kernel helper, so predicted blast
// feedback and self-knockback match the server's authoritative splash verdict.
bool clientSplashHasLineOfSight(const World& world,
                                const glm::vec3& explosionPos,
                                const glm::vec3& victimPoint)
{
    const glm::vec3 delta = victimPoint - explosionPos;
    const float maxDist = glm::length(delta);
    if (maxDist < 0.75f)
        return true;
    const glm::vec3 dir = delta / maxDist;

    AABB rayBounds;
    rayBounds.min = glm::min(explosionPos, victimPoint);
    rayBounds.max = glm::max(explosionPos, victimPoint);
    std::vector<int> candidates;
    appendChunkTrianglesForAABB(world, rayBounds, 0.05f, candidates, "splashLineOfSight");

    return !splashRayBlockedByWall(explosionPos, dir, maxDist,
                                   candidates, world.collisionMesh.triangles);
}

// Nearest world-space body-part point (head/arms/legs/torso) on a rendered
// Player to the blast — same representation the beam/aim uses, never a capsule.
// Falls back to the torso center when the body parts aren't loaded.
bool playerSplashBodyPoint(const Player& p, const glm::vec3& blast, glm::vec3& outPoint)
{
    SplashBodyPartBox boxes[16];
    int count = 0;
    if (!p.physicalBody.parts.empty())
    {
        const_cast<Player&>(p).updateModelWorldTransforms();
        for (const PhysicalBodyPart& part : p.physicalBody.parts)
        {
            if (count >= 16)
                break;
            const glm::vec3 localCenter =
                (part.collider.localMin + part.collider.localMax) * 0.5f;
            boxes[count].center =
                glm::vec3(part.worldTransform * glm::vec4(localCenter, 1.0f));
            boxes[count].half = glm::max(
                (part.collider.localMax - part.collider.localMin) * 0.5f,
                glm::vec3(0.12f));
            ++count;
        }
    }
    if (count == 0)
    {
        const Capsule cap = p.getCapsule();
        outPoint = p.pos + glm::vec3(0.0f, 0.0f, (cap.a.z + cap.b.z) * 0.5f);
        return true;
    }
    return splashNearestBodyPartPoint(blast, boxes, count, outPoint);
}

void spawnProjectileTrail(NetworkProjectile& projectile, float dt)
{
    const bool rocket = projectile.weaponType == NETWORK_WEAPON_ROCKET_LAUNCHER;
    const float rate = rocket ? 30.0f : 18.0f;
    projectile.smokeAccumulator += rate * dt;
    while (projectile.smokeAccumulator >= 1.0f)
    {
        projectile.smokeAccumulator -= 1.0f;
        EffectPart part;
        // Trails use render position for smoothness
        part.position = projectile.renderPosition;
        part.velocity = rocket
            ? projectile.renderVelocity * -0.08f
            : projectile.renderVelocity * 0.10f;
        part.lifetime = 0.0f;
        part.maxLifetime = rocket ? 0.8f : 0.25f;
        part.scale = rocket ? 0.18f : 0.04f;
        part.endScale = rocket ? 0.6f : 0.01f;
        part.color = rocket
            ? glm::vec3(0.55f, 0.55f, 0.55f)
            : glm::vec3(1.0f, 0.65f, 0.15f);
        part.alpha = rocket ? 0.55f : 0.9f;
        part.gravity = rocket ? 0.0f : 8.0f;
        part.affectedByGravity = !rocket;
        part.replayType = rocket ? "net_rocket_trail" : "net_grenade_spark";
        EffectPartSystem::instance().spawn(part);
    }
}

uint64_t reliableEventKey(uint32_t eventSessionId, uint32_t eventId)
{
    return ((uint64_t)eventSessionId << 32) | (uint64_t)eventId;
}

void ackReliableEvent(MultiplayerContext& ctx, uint32_t eventId, uint32_t eventSessionId)
{
    if (eventId == 0 || eventSessionId == 0)
        return;
    ReliableEventAckPacket ack{};
    ack.header.type = PACKET_RELIABLE_EVENT_ACK;
    ack.header.tick = ctx.tick;
    ack.header.playerId = ctx.localPlayerId;
    ack.eventId = eventId;
    ack.eventSessionId = eventSessionId;
    mpSendPacket(ctx, &ack, sizeof(ack));
}

bool acceptReliableEventOnce(MultiplayerContext& ctx, uint32_t eventId, uint32_t eventSessionId)
{
    if (eventId == 0 || eventSessionId == 0)
        return true;
    if (ctx.reliableEventSessionId != 0 && eventSessionId != ctx.reliableEventSessionId)
        return false;
    ackReliableEvent(ctx, eventId, eventSessionId);
    const uint64_t key = reliableEventKey(eventSessionId, eventId);
    if (ctx.processedReliableEventIds.count(key))
        return false;
    ctx.processedReliableEventIds.insert(key);
    ctx.processedReliableEventOrder.push_back(key);
    while (ctx.processedReliableEventOrder.size() > 512)
    {
        ctx.processedReliableEventIds.erase(ctx.processedReliableEventOrder.front());
        ctx.processedReliableEventOrder.pop_front();
    }
    return true;
}

} // namespace

bool mpAcceptReliableEventOnce(MultiplayerContext& ctx, uint32_t eventId, uint32_t eventSessionId)
{
    return acceptReliableEventOnce(ctx, eventId, eventSessionId);
}

uint32_t mpSendProjectileFireRequest(
    MultiplayerContext& ctx,
    uint8_t weapon,
    const glm::vec3& origin,
    const glm::vec3& direction)
{
    (void)ctx;
    (void)weapon;
    (void)origin;
    (void)direction;
    printf("[PROJECTILE FIRE REQUEST SEND] accepted=0 reason=legacy-direct-packet-disabled\n");
    return 0;
}

uint32_t mpPredictProjectileAttack(
    MultiplayerContext& ctx,
    uint32_t requestId,
    uint16_t weaponDefNetworkId,
    const glm::vec3& origin,
    const glm::vec3& direction)
{
    if (!ctx.active || !ctx.localPlayerId || requestId == 0 ||
        !finiteVec(origin) || !finiteVec(direction) ||
        glm::length(direction) <= 0.001f)
        return 0;

    const std::string* weaponId = weaponIdForDefNetworkId(weaponDefNetworkId);
    if (!weaponId)
        return 0;
    const WeaponDefinition* def = WeaponRegistry::instance().get(*weaponId);
    if (!def || def->executionType != WeaponExecutionType::Projectile)
        return 0;
    const uint8_t networkWeapon = networkWeaponTypeForDefinition(*def);
    if (!networkWeaponTypeIsProjectile(networkWeapon))
        return 0;

    const uint32_t provisionalId = provisionalProjectileId(requestId);
    if (provisionalId == 0)
        return 0;

    const glm::vec3 dir = glm::normalize(direction);
    const float speed = def->projectileSpeed > 0.0f ? def->projectileSpeed : 40.0f;
    const float upBias = cp(def, "upBias", 4.0f);
    NetworkProjectile projectile;
    projectile.projectileId = provisionalId;
    projectile.ownerPlayerId = ctx.localPlayerId;
    projectile.fireSerial = requestId;
    projectile.weaponType = networkWeapon;
    projectile.position = origin;
    projectile.previousPosition = origin;
    projectile.velocity = dir * speed + glm::vec3(0.0f, 0.0f, upBias);
    projectile.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    projectile.angularVelocity = glm::vec3(0.0f);
    projectile.lifetime = def->projectileLifetime > 0.0f ? def->projectileLifetime : 5.0f;
    projectile.radius = def->projectileRadius > 0.0f ? def->projectileRadius : 0.3f;
    projectile.predicted = true;
    projectile.exploded = false;
    configureNetworkProjectile(projectile, def);
    if (networkWeapon == NETWORK_WEAPON_GRENADE_LAUNCHER)
    {
        glm::vec3 forward = glm::length(dir) > 0.0001f ? dir : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 refUp = std::fabs(forward.z) < 0.99f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(forward, refUp));
        projectile.angularVelocity = right * cp(def, "angSpeed", 6.0f);
    }

    projectile.renderPosition = projectile.position;
    projectile.renderVelocity = projectile.velocity;
    projectile.renderRotation = projectile.rotation;
    projectile.renderAngularVelocity = projectile.angularVelocity;
    projectile.prevStatePos = projectile.position;
    projectile.prevStateVel = projectile.velocity;
    projectile.prevStateRot = projectile.rotation;
    projectile.targetStatePos = projectile.position;
    projectile.targetStateVel = projectile.velocity;
    projectile.targetStateRot = projectile.rotation;

    ctx.networkProjectiles[provisionalId] = projectile;
    ctx.predictedProjectileIds.insert(provisionalId);

    printf("[PROJECTILE CLIENT PREDICT] requestId=%u provisionalId=%u weapon=%s "
           "pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f)\n",
           requestId, provisionalId, networkWeaponTypeName(networkWeapon),
           projectile.position.x, projectile.position.y, projectile.position.z,
           projectile.velocity.x, projectile.velocity.y, projectile.velocity.z);
    return provisionalId;
}

void mpCancelPredictedProjectileAttack(MultiplayerContext& ctx,
                                       uint32_t requestId)
{
    removePredictedProjectileForRequest(ctx, requestId);
}

uint32_t mpSendMeleeHitRequest(
    MultiplayerContext& ctx,
    uint32_t targetPlayerId,
    int damage,
    uint8_t weapon,
    uint8_t attackType,
    const glm::vec3& hit,
    const glm::vec3& normal,
    const glm::vec3& knockback,
    float weaponSpeed)
{
    if (!ctx.active || !ctx.localPlayerId || targetPlayerId == 0 ||
        weapon != NETWORK_WEAPON_SWORDSWORD)
        return 0;

    MeleeHitRequestPacket packet{};
    packet.header.type = PACKET_MELEE_HIT_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.attackSerial = ctx.nextLocalMeleeAttackSerial++;
    if (ctx.nextLocalMeleeAttackSerial == 0)
        ctx.nextLocalMeleeAttackSerial = 1;
    packet.lastServerTick = mpFireRenderTick(ctx, ctx.latestServerTick);
    packet.targetPlayerId = targetPlayerId;
    packet.damage = std::clamp(damage, 1, 200);
    packet.weapon = weapon;
    packet.attackType = attackType;
    packet.hitX = hit.x;
    packet.hitY = hit.y;
    packet.hitZ = hit.z;
    packet.normalX = normal.x;
    packet.normalY = normal.y;
    packet.normalZ = normal.z;
    packet.knockX = knockback.x;
    packet.knockY = knockback.y;
    packet.knockZ = knockback.z;
    packet.weaponSpeed = weaponSpeed;
    mpSendPacket(ctx, &packet, sizeof(packet));
    printf("[SWORD HIT REQUEST SEND] attackerId=%u targetId=%u attackSerial=%u "
           "damage=%d hit=(%.2f,%.2f,%.2f) knockback=(%.2f,%.2f,%.2f)\n",
           ctx.localPlayerId, targetPlayerId, packet.attackSerial,
           packet.damage, hit.x, hit.y, hit.z,
           knockback.x, knockback.y, knockback.z);
    return packet.attackSerial;
}

void mpProcessProjectileSpawnEventPacket(MultiplayerContext& ctx, const ProjectileSpawnEventPacket* event)
{
    if (ctx.projectileTerminals.has(event->projectileId))
    {
        printf("[PROJECTILE CLIENT SPAWN] projectileId=%u weapon=%s accepted=0 reason=already-terminated\n",
               event->projectileId, networkWeaponTypeName(event->weapon));
        return;
    }

    if (event->fireSerial != 0)
    {
        ctx.pendingFireRequests.erase(event->fireSerial);
        auto pendingAttack = ctx.pendingAttackRequests.find(event->fireSerial);
        if (pendingAttack != ctx.pendingAttackRequests.end())
            ctx.pendingAttackRequests.erase(pendingAttack);
    }

    const bool localOwner = event->ownerPlayerId == ctx.localPlayerId;
    bool adoptedPrediction = adoptPredictedProjectile(ctx, event->fireSerial, event->projectileId);
    // If the authoritative projectile already exists for the local owner with a
    // matching fireSerial (adopted earlier via the reliable AttackResult), don't
    // reset its pose — the spawn broadcast just arrived late.
    bool ownAlreadyExists = localOwner && !adoptedPrediction;
    if (ownAlreadyExists)
    {
        auto ex = ctx.networkProjectiles.find(event->projectileId);
        ownAlreadyExists = ex != ctx.networkProjectiles.end() &&
            ex->second.ownerPlayerId == ctx.localPlayerId &&
            ex->second.fireSerial == event->fireSerial;
    }
    const bool preserveSim = adoptedPrediction || ownAlreadyExists;
    NetworkProjectile& projectile = ctx.networkProjectiles[event->projectileId];

    projectile.projectileId = event->projectileId;
    projectile.ownerPlayerId = event->ownerPlayerId;
    projectile.fireSerial = event->fireSerial;
    projectile.weaponType = event->weapon;
    const glm::vec3 serverPosition(event->posX, event->posY, event->posZ);
    const glm::vec3 serverVelocity(event->velX, event->velY, event->velZ);
    if (!preserveSim)
        projectile.position = serverPosition;
    projectile.previousPosition = projectile.position;
    if (!preserveSim)
    {
        projectile.velocity = serverVelocity;
        projectile.rotation = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);
        projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
    }
    projectile.age = preserveSim ? projectile.age : 0.0f;
    projectile.lifetime = event->lifetime;
    projectile.radius = event->radius;
    projectile.predicted = localOwner && preserveSim;
    projectile.exploded = false;
    configureNetworkProjectile(projectile, projectileDefinition(event->weapon));
    if (projectile.predicted)
        ctx.predictedProjectileIds.insert(event->projectileId);

    // Initialize render state directly from spawn (first state = immediate render)
    projectile.renderPosition = projectile.position;
    projectile.renderVelocity = projectile.velocity;
    projectile.renderRotation = projectile.rotation;
    projectile.renderAngularVelocity = projectile.angularVelocity;

    // Interpolation state. When adopting a client-predicted projectile, NEVER
    // yank the visual back to the server's (RTT-old) spawn state — continue
    // from the predicted current position so the grenade/rocket never
    // rubber-bands backward. The server's state events reconcile forward.
    if (preserveSim)
    {
        projectile.prevStateTick = projectile.targetStateTick = ctx.latestServerTick;
        projectile.prevStatePos = projectile.targetStatePos = projectile.position;
        projectile.prevStateVel = projectile.targetStateVel = projectile.velocity;
        projectile.prevStateRot = projectile.targetStateRot = projectile.rotation;
    }
    else
    {
        projectile.prevStateTick = event->spawnTick;
        projectile.prevStatePos = projectile.position;
        projectile.prevStateVel = projectile.velocity;
        projectile.prevStateRot = projectile.rotation;
        projectile.targetStateTick = event->spawnTick;
        projectile.targetStatePos = serverPosition;
        projectile.targetStateVel = serverVelocity;
        projectile.targetStateRot = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);
    }
    projectile.hasTargetState = true;

    // Muzzle flash on fire for remote shooters (owner already has the
    // instant client-side muzzle flash from local prediction).
    if (!localOwner)
        EffectPartSystem::instance().spawnMuzzleFlash(serverPosition, networkWeaponTypeName(event->weapon));

    // ── Correlation promotion: provisional fireSerial now has authoritative projectileId ──
    {
        bool pendingMatched = (event->fireSerial != 0) &&
            (ctx.pendingFireRequests.find(event->fireSerial) == ctx.pendingFireRequests.end());
        std::string provCorr = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
            + "_F" + std::to_string(projectile.fireSerial) + "_J0";
        std::string authCorr = "GRENADE_P" + std::to_string(projectile.ownerPlayerId)
            + "_F" + std::to_string(projectile.fireSerial) + "_J" + std::to_string(projectile.projectileId);

        logGrenadeEvent(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important,
            "GRENADE_CORRELATION_PROMOTE",
            authCorr,
            "promote",
            "ownerId=%u fireSerial=%u authoritativeId=%u provisionalCorr=%s pendingMatched=%d "
            "predicted=%d provObjectExists=%d "
            "spawnPos=(%.6f,%.6f,%.6f) spawnVel=(%.6f,%.6f,%.6f)",
            projectile.ownerPlayerId, projectile.fireSerial, projectile.projectileId,
            provCorr.c_str(), (int)pendingMatched,
            (int)projectile.predicted, (int)adoptedPrediction,
            event->posX, event->posY, event->posZ,
            event->velX, event->velY, event->velZ);
    }

    logGrenadeEvent(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important,
        "GRENADE_CLIENT_SPAWN",
        "GRENADE_P" + std::to_string(projectile.ownerPlayerId) + "_F" + std::to_string(projectile.fireSerial) + "_J" + std::to_string(projectile.projectileId),
        "authoritative",
        "projectileId=%u ownerId=%u fireSerial=%u weapon=%s predicted=%d isLocal=%d "
        "pos=(%.6f,%.6f,%.6f) vel=(%.6f,%.6f,%.6f) angVel=(%.6f,%.6f,%.6f) "
        "radius=%.6f lifetime=%.6f spawnTick=%u",
        projectile.projectileId, projectile.ownerPlayerId, projectile.fireSerial,
        networkWeaponTypeName(projectile.weaponType),
        (int)projectile.predicted,
        (int)(projectile.ownerPlayerId == ctx.localPlayerId),
        projectile.position.x, projectile.position.y, projectile.position.z,
        projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
        projectile.angularVelocity.x, projectile.angularVelocity.y, projectile.angularVelocity.z,
        projectile.radius, projectile.lifetime, event->spawnTick);

    printf("[PROJECTILE CLIENT SPAWN] projectileId=%u ownerPlayerId=%u "
           "weapon=%s position=(%.2f,%.2f,%.2f) velocity=(%.2f,%.2f,%.2f) "
           "spawnTick=%u\n",
           projectile.projectileId, projectile.ownerPlayerId,
           networkWeaponTypeName(projectile.weaponType),
           projectile.position.x, projectile.position.y, projectile.position.z,
           projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
           event->spawnTick);
}

void mpProcessProjectileStateEventPacket(MultiplayerContext& ctx, const ProjectileStateEventPacket* event)
{
    if (ctx.projectileTerminals.has(event->projectileId))
    {
        printf("[PROJECTILE STATE RX] projectileId=%u serverTick=%u accepted=0 reason=already-terminated\n",
               event->projectileId, event->header.tick);
        return;
    }

    auto it = ctx.networkProjectiles.find(event->projectileId);
    if (it == ctx.networkProjectiles.end())
    {
        // Lost spawn event — recover from state update. For the local owner's
        // own projectile this state event can beat both the AttackResult and the
        // spawn broadcast: adopt the predicted projectile (rename provisional →
        // authoritative) so we NEVER render a second one. Remote projectiles (or
        // an owner with no prediction) fall through to a fresh create.
        if (event->ownerPlayerId == ctx.localPlayerId &&
            adoptPredictedProjectile(ctx, event->fireSerial, event->projectileId))
        {
            it = ctx.networkProjectiles.find(event->projectileId);
            if (it != ctx.networkProjectiles.end())
            {
                NetworkProjectile& adopted = it->second;
                adopted.prevStateTick = adopted.targetStateTick = event->header.tick;
                adopted.prevStatePos = adopted.targetStatePos = adopted.position;
                adopted.prevStateVel = adopted.targetStateVel = adopted.velocity;
                adopted.prevStateRot = adopted.targetStateRot = adopted.rotation;
                adopted.latestAcceptedTick = 0; // accept this first server tick below
                adopted.lastTargetReceivedMs = nowMs();
            }
        }
    }
    if (it == ctx.networkProjectiles.end())
    {
        NetworkProjectile& projectile = ctx.networkProjectiles[event->projectileId];
        projectile.projectileId = event->projectileId;
        projectile.ownerPlayerId = 0;
        projectile.weaponType = event->weapon;
        projectile.position = {event->posX, event->posY, event->posZ};
        projectile.previousPosition = projectile.position;
        projectile.velocity = {event->velX, event->velY, event->velZ};
        projectile.rotation = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);
        projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
        projectile.age = event->age;
        projectile.lifetime = 0.0f;
        projectile.radius = 0.0f;
        projectile.predicted = false;
        projectile.exploded = false;
        configureNetworkProjectile(projectile, projectileDefinition(event->weapon));
        // Initialize render state for recovery
        projectile.renderPosition = projectile.position;
        projectile.renderVelocity = projectile.velocity;
        projectile.renderRotation = projectile.rotation;
        projectile.prevStateTick = event->header.tick;
        projectile.prevStatePos = projectile.position;
        projectile.prevStateVel = projectile.velocity;
        projectile.prevStateRot = projectile.rotation;
        projectile.targetStateTick = event->header.tick;
        projectile.targetStatePos = projectile.position;
        projectile.targetStateVel = projectile.velocity;
        projectile.targetStateRot = projectile.rotation;
        projectile.latestAcceptedTick = event->header.tick;
        projectile.lastTargetReceivedMs = nowMs();
        projectile.hasTargetState = true;
        printf("[PROJECTILE STATE RECOVER] projectileId=%u weapon=%s "
               "position=(%.2f,%.2f,%.2f) age=%.2f\n",
               event->projectileId, networkWeaponTypeName(event->weapon),
               event->posX, event->posY, event->posZ, event->age);
        return;
    }
    NetworkProjectile& projectile = it->second;

    // ── Stale state rejection ──────────────────────────────────────
    // Reject states older than our latest accepted state.
    const uint32_t newTick = event->header.tick;
    if (newTick <= projectile.latestAcceptedTick)
    {
        printf("[PROJECTILE STATE RX] projectileId=%u serverTick=%u "
               "latestAcceptedTick=%u accepted=0 reason=stale-or-duplicate\n",
               event->projectileId, newTick, projectile.latestAcceptedTick);
        return;
    }

    // ── Move current target to previous, set new target ────────────
    projectile.prevStateTick = projectile.targetStateTick;
    projectile.prevStatePos = projectile.targetStatePos;
    projectile.prevStateVel = projectile.targetStateVel;
    projectile.prevStateRot = projectile.targetStateRot;

    projectile.targetStateTick = newTick;
    projectile.targetStatePos = {event->posX, event->posY, event->posZ};
    projectile.targetStateVel = {event->velX, event->velY, event->velZ};
    projectile.targetStateRot = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);

    projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
    projectile.age = event->age;
    logGrenadeEvent(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose,
        "GRENADE_CLIENT_STATE",
        "GRENADE_P" + std::to_string(projectile.ownerPlayerId) + "_F" + std::to_string(projectile.fireSerial) + "_J" + std::to_string(projectile.projectileId),
        "correction-target",
        "predicted=%d overwrite=0 tick=%u age=%.6f serverPos=(%.6f,%.6f,%.6f) serverVel=(%.6f,%.6f,%.6f) "
        "localPos=(%.6f,%.6f,%.6f) localVel=(%.6f,%.6f,%.6f) posErr=%.6f velErr=%.6f",
        (int)projectile.predicted, newTick, event->age,
        event->posX, event->posY, event->posZ,
        event->velX, event->velY, event->velZ,
        projectile.position.x, projectile.position.y, projectile.position.z,
        projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
        glm::length(glm::vec3(event->posX, event->posY, event->posZ) - projectile.position),
        glm::length(glm::vec3(event->velX, event->velY, event->velZ) - projectile.velocity));

    projectile.latestAcceptedTick = newTick;
    projectile.lastTargetReceivedMs = nowMs();
    projectile.hasTargetState = true;

    printf("[PROJECTILE STATE RX] projectileId=%u serverTick=%u "
           "latestAcceptedTick=%u accepted=1 pos=(%.2f,%.2f,%.2f) vel=(%.2f,%.2f,%.2f) age=%.2f\n",
           event->projectileId, newTick, projectile.latestAcceptedTick,
           event->posX, event->posY, event->posZ,
           event->velX, event->velY, event->velZ,
           event->age);
}

void mpProcessProjectileExplodeEventPacket(MultiplayerContext& ctx, const ProjectileExplodeEventPacket* event)
{
    if (!acceptReliableEventOnce(ctx, event->eventId, event->eventSessionId))
    {
        printf("[PROJECTILE CLIENT TERMINAL] eventId=%u projectileId=%u weapon=%s terminal=explode accepted=0 reason=duplicate-event\n",
               event->eventId, event->projectileId, networkWeaponTypeName(event->weapon));
        return;
    }

    if (!ctx.projectileTerminals.record(event->projectileId))
    {
        printf("[PROJECTILE CLIENT TERMINAL] projectileId=%u weapon=%s terminal=explode accepted=0 reason=duplicate\n",
               event->projectileId, networkWeaponTypeName(event->weapon));
        return;
    }

    const glm::vec3 position(event->posX, event->posY, event->posZ);
    const char* weaponName = networkWeaponTypeName(event->weapon);
    const bool wasPredicted = ctx.predictedProjectileIds.erase(event->projectileId) > 0;
    bool removedVisual = ctx.networkProjectiles.erase(event->projectileId) > 0;
    bool removedLegacy = false;
    if (gpWeapons)
    {
        removedLegacy = gpWeapons->removeAuthoritativeRocket(event->projectileId);
        if (!removedLegacy)
            removedLegacy = gpWeapons->removeLocalRocketByFireSerial(event->fireSerial);
    }

    // ── Explosion visuals: reconcile against the client-predicted explosion ──
    // The local owner predicts the explosion instantly. The server confirm either
    // agrees (let the predicted effect play out — no redraw) or disagrees (show a
    // disagreement marker + the corrected explosion). Other clients always render
    // the server-confirmed explosion.
    constexpr float kExplosionAgreeDistance = 3.0f;
    std::string attacker = "player_" + std::to_string(event->ownerPlayerId);
    {
        auto pi = ctx.playerRegistry.find(event->ownerPlayerId);
        if (pi != ctx.playerRegistry.end())
            attacker = pi->second.name;
    }
    const auto predIt = ctx.predictedExplosions.find(event->fireSerial);
    if (predIt != ctx.predictedExplosions.end())
    {
        const glm::vec3 predictedPos = predIt->second;
        ctx.predictedExplosions.erase(predIt);
        if (glm::length(predictedPos - position) <= kExplosionAgreeDistance)
        {
            printf("[EXPLOSION RECONCILE] fireSerial=%u agree predicted=(%.2f,%.2f,%.2f) "
                   "server=(%.2f,%.2f,%.2f)\n",
                   event->fireSerial, predictedPos.x, predictedPos.y, predictedPos.z,
                   position.x, position.y, position.z);
        }
        else
        {
            printf("[EXPLOSION RECONCILE] fireSerial=%u disagree predicted=(%.2f,%.2f,%.2f) "
                   "server=(%.2f,%.2f,%.2f) dist=%.2f\n",
                   event->fireSerial, predictedPos.x, predictedPos.y, predictedPos.z,
                   position.x, position.y, position.z,
                   glm::length(predictedPos - position));
            DisagreementEvent de;
            de.timeMs = nowMs();
            de.reason = DISAGREEMENT_POSITION_CORRECTION;
            de.sourcePlayerId = event->ownerPlayerId;
            de.position = predictedPos;
            de.correction = position - predictedPos;
            de.description = "explosion position mismatch";
            spawnDisagreementEffect(de);
            spawnExplosionFx(position, weaponName, attacker);
        }
    }
    else
    {
        spawnExplosionFx(position, weaponName, attacker);
    }

    for (uint8_t i = 0; i < event->victimCount && i < MAX_PROJECTILE_DAMAGE_RESULTS; ++i)
    {
        const ProjectileDamageResultPacket& victim = event->victims[i];
        if (victim.victimPlayerId == ctx.localPlayerId)
        {
            // The local owner already applied this self-knockback via client
            // explosion prediction — the authoritative event supersedes it, so
            // skip the pendingKnockback add rather than pushing the shooter twice.
            const auto selfKb = ctx.predictedSelfKnockbacks.find(event->fireSerial);
            if (selfKb != ctx.predictedSelfKnockbacks.end())
            {
                ctx.predictedSelfKnockbacks.erase(selfKb);
                printf("[NET KNOCKBACK APPLY] projectileId=%u victim=local "
                       "impulse=(%.2f,%.2f,%.2f) source=%s accepted=predicted-supersede\n",
                       event->projectileId,
                       victim.knockX, victim.knockY, victim.knockZ,
                       weaponName);
            }
            else
            {
                ctx.pendingKnockback += glm::vec3(
                    victim.knockX, victim.knockY, victim.knockZ);
                ctx.pendingKnockbackSource = weaponName;
                printf("[NET KNOCKBACK APPLY] projectileId=%u victim=local "
                       "impulse=(%.2f,%.2f,%.2f) source=%s\n",
                       event->projectileId,
                       victim.knockX, victim.knockY, victim.knockZ,
                       weaponName);
            }
        }
        else
        {
            auto remote = ctx.remotePlayers.find(victim.victimPlayerId);
            if (remote != ctx.remotePlayers.end())
            {
                remote->second.externalImpulse += glm::vec3(
                    victim.knockX, victim.knockY, victim.knockZ);
            }
        }
        printf("[NET PROJECTILE DAMAGE RECV] projectileId=%u victimPlayerId=%u "
               "damage=%d healthAfter=%d killed=%d\n",
               event->projectileId, victim.victimPlayerId,
               victim.damage, victim.healthAfter, (int)victim.killed);
    }

    // Server did not knock the local player back for this fireSerial — drop any
    // phantom predicted self-knockback so it never lingers.
    ctx.predictedSelfKnockbacks.erase(event->fireSerial);

    printf("[PROJECTILE CLIENT EXPLODE] projectileId=%u weapon=%s "
           "position=(%.2f,%.2f,%.2f) serverTick=%u removedVisual=%d removedLegacy=%d predicted=%d\n",
           event->projectileId, weaponName,
           position.x, position.y, position.z, event->header.tick,
           (int)removedVisual, (int)removedLegacy, (int)wasPredicted);
}

// ── Client-local "server disagree: hit rejected" detection ────────────
// Spawns a world effect at the position where the client's local trace claimed
// a hit, when the server's authoritative trace disagreed (shot rejected, a
// different target, or a miss that never produced a confirmation).
static void spawnHitClaimDisagreement(MultiplayerContext& ctx,
                                      const MultiplayerContext::PendingHitClaim& claim,
                                      const char* description)
{
    ++ctx.rejectedHits;
    const bool npcClaim =
        claim.claimedTargetId != 0 &&
        ctx.remoteNpcs.find(claim.claimedTargetId) != ctx.remoteNpcs.end();
    mpRollbackPredictedDamage(ctx, claim.claimedTargetId, npcClaim, description);
    if (!NetworkingConfig::instance().data().disagreement.enabled)
        return;
    DisagreementEvent event;
    event.timeMs = nowMs();
    event.reason = DISAGREEMENT_INVALID_STATE;
    event.position = claim.claimedHit;
    event.correction = glm::vec3(0.0f);
    event.sourcePlayerId = ctx.localPlayerId;
    event.targetPlayerId = claim.claimedTargetId;
    event.description = description;
    spawnDisagreementEffect(event);
    logDisagreement(event);
}

void mpSweepHitClaims(MultiplayerContext& ctx)
{
    constexpr uint64_t HIT_CLAIM_TIMEOUT_MS = 800;
    const uint64_t now = nowMs();
    for (auto it = ctx.pendingHitClaims.begin(); it != ctx.pendingHitClaims.end(); )
    {
        if (it->second.resolved || it->second.confirmed)
        {
            it = ctx.pendingHitClaims.erase(it);
            continue;
        }
        if (now - it->second.sentMs >= HIT_CLAIM_TIMEOUT_MS)
        {
            spawnHitClaimDisagreement(ctx, it->second, "HIT REJECTED");
            it = ctx.pendingHitClaims.erase(it);
            continue;
        }
        ++it;
    }
}

void mpProcessDamageConfirmedEventPacket(MultiplayerContext& ctx,
                                         const DamageConfirmedEventPacket* event,
                                         const ConfirmedDamagePresentationSink* sink)
{
    if (!acceptReliableEventOnce(ctx, event->eventId, event->eventSessionId))
    {
        printf("[NET DAMAGE CONFIRMED] eventId=%u target=%u accepted=0 reason=duplicate-or-stale\n",
               event->eventId, event->targetPlayerId);
        return;
    }

    // Hit-claim correlation: causeSerial is our attack requestId. If the server
    // confirmed damage on the claimed target, the hit agreed. If it confirmed
    // damage on a DIFFERENT target, the server's trace disagreed with ours.
    if (event->attackerPlayerId == ctx.localPlayerId)
    {
        auto claimIt = ctx.pendingHitClaims.find(event->causeSerial);
        if (claimIt != ctx.pendingHitClaims.end())
        {
            const bool targetMatched =
                claimIt->second.claimedTargetId == event->targetPlayerId;
            claimIt->second.confirmed = true;
            claimIt->second.resolved = true;
            if (!targetMatched)
                spawnHitClaimDisagreement(ctx, claimIt->second, "HIT REJECTED");
            ctx.pendingHitClaims.erase(claimIt);
        }
        if (event->targetPlayerId != ctx.localPlayerId)
        {
            // A confirmed-damage event can arrive after the victim already
            // respawned (instant respawn or delayed/lossy delivery). Applying
            // the predicted health/death to the new life would render the
            // respawned body as dead (hidden) for the hold window. Skip it
            // when the victim has advanced past the life this event describes.
            bool victimLifeFresh = true;
            if (event->targetSpawnGeneration != 0)
            {
                auto victimIt = ctx.remotePlayers.find(event->targetPlayerId);
                if (victimIt != ctx.remotePlayers.end())
                    victimLifeFresh = victimIt->second.spawnGeneration ==
                        event->targetSpawnGeneration;
            }
            if (victimLifeFresh)
            {
                mpConfirmPredictedDamage(
                    ctx, event->targetPlayerId, event->healthAfter,
                    event->killed != 0, false);
            }
            else
            {
                uint32_t currentGen = 0;
                auto victimIt = ctx.remotePlayers.find(event->targetPlayerId);
                if (victimIt != ctx.remotePlayers.end())
                    currentGen = victimIt->second.spawnGeneration;
                printf("[NET PREDICTED DAMAGE SKIP] target=%u eventGen=%u "
                       "currentGen=%u reason=victim-already-respawned\n",
                       event->targetPlayerId, (unsigned)event->targetSpawnGeneration,
                       currentGen);
            }
        }
    }

    presentConfirmedDamage(ctx, *event, sink);

    const glm::vec3 knockback(event->knockX, event->knockY, event->knockZ);
    const bool isLocalVictim = (event->targetPlayerId == ctx.localPlayerId);
    npcLog("npc-damage-confirmed attacker=%u target=%u damage=%d "
           "healthBefore=%d healthAfter=%d killed=%d localVictim=%d knockback=(%.2f %.2f %.2f)",
           event->attackerPlayerId, event->targetPlayerId,
           event->damage, event->healthBefore, event->healthAfter, (int)event->killed,
           (int)isLocalVictim, knockback.x, knockback.y, knockback.z);

    if (isLocalVictim)
    {
        // Health synced to the shot visual: queue a pending change applied in
        // the SAME frame the attacker's body renders past this shot's server
        // tick (the same gate the bullet tracer uses). Always queue the HP drop
        // even when knockback is zero — otherwise confirmed damage with no
        // knockback (e.g. NPC hits) would never update the victim's health bar.
        const double maxHoldMs = NetworkingConfig::instance().data()
            .eventTimeline.remoteEffectMaximumHoldMs;
        MultiplayerContext::PendingVictimHealth ph;
        ph.healthAfter = event->healthAfter;
        ph.killed = event->killed != 0;
        ph.knockback = knockback;
        ph.applyAtMs = nowMs() + (uint64_t)std::max(16.0, maxHoldMs);
        ph.shooterId = event->attackerPlayerId;
        ph.eventServerTick = event->header.tick;
        ph.receivedMs = nowMs();
        ctx.pendingVictimHealth.push_back(ph);
    }
    else if (glm::length(knockback) > 0.001f)
    {
        auto remote = ctx.remotePlayers.find(event->targetPlayerId);
        if (remote != ctx.remotePlayers.end())
            remote->second.externalImpulse += knockback;
    }

    printf("[NET DAMAGE CONFIRMED] eventId=%u source=%u attacker=%u target=%u damage=%d healthBefore=%d healthAfter=%d killed=%d\n",
           event->eventId, (unsigned)event->source, event->attackerPlayerId,
           event->targetPlayerId, event->damage, event->healthBefore,
           event->healthAfter, (int)event->killed);
    {
        char msg[512];
        std::snprintf(msg, sizeof(msg),
                      "attacker=%u target=%u damage=%d health=%d->%d killed=%d source=%u",
                      event->attackerPlayerId, event->targetPlayerId, event->damage,
                      event->healthBefore, event->healthAfter, (int)event->killed,
                      (unsigned)event->source);
        ::logStructured(::StructuredCategory::Network, ::StructuredLevel::Important,
                        "DAMAGE_CONFIRMED",
                        "ATTACK_" + std::to_string(event->causeSerial),
                        "server-confirmed damage event", msg);
    }
}

void mpProcessProjectileFireResultPacket(MultiplayerContext& ctx, const ProjectileFireResultPacket* event)
{
    const ProjectileFireResultApplyOutcome outcome =
        mpApplyProjectileFireResultToPending(ctx, *event);

    if (outcome.accepted)
    {
        {
            auto& _lg = ::StructuredLogger::instance();
            if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
                ::StructuredLogger::Entry e;
                e.category = ::StructuredCategory::GrenadeLauncher;
                e.level = ::StructuredLevel::Important;
                e.eventId = "GRENADE_FIRE_RESULT";
                e.correlationId = "GRENADE_P" + std::to_string(ctx.localPlayerId)
                    + "_F" + std::to_string(event->fireSerial)
                    + "_J" + std::to_string(event->projectileId);
                e.reason = "accepted";
                char b[256]; std::snprintf(b, sizeof(b),
                    "fireSerial=%u projectileId=%u weapon=%s accepted=1",
                    event->fireSerial, event->projectileId,
                    networkWeaponTypeName(event->weapon));
                e.message = b;
                _lg.write(e);
            }
        }
    }
    else
    {
        if (outcome.clearedProjectilePending)
        {
            {
                auto& _lg = ::StructuredLogger::instance();
                if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important)) {
                    ::StructuredLogger::Entry e;
                    e.category = ::StructuredCategory::GrenadeLauncher;
                    e.level = ::StructuredLevel::Important;
                    e.eventId = "GRENADE_FIRE_RESULT";
                    e.correlationId = "GRENADE_P" + std::to_string(ctx.localPlayerId)
                        + "_F" + std::to_string(event->fireSerial) + "_J0";
                    e.reason = "rejected";
                    char b[256]; std::snprintf(b, sizeof(b),
                        "fireSerial=%u reason=%d cooldownRemaining=%.2f refundQueued=%d",
                        event->fireSerial, (int)event->reason,
                        event->cooldownRemaining,
                        (int)(ctx.processedRefundSerials.count(event->fireSerial) > 0));
                    e.message = b;
                    _lg.write(e);
                }
            }
        }
        else
        {
        {
            auto& _lg = ::StructuredLogger::instance();
            if (_lg.shouldLog(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose)) {
                ::StructuredLogger::Entry e;
                e.category = ::StructuredCategory::GrenadeLauncher;
                e.level = ::StructuredLevel::Verbose;
                e.eventId = "GRENADE_FIRE_RESULT";
                e.correlationId = "GRENADE_P" + std::to_string(ctx.localPlayerId)
                    + "_F" + std::to_string(event->fireSerial) + "_J0";
                e.reason = "no-matching-pending";
                char b[128]; std::snprintf(b, sizeof(b), "fireSerial=%u reason=%d",
                    event->fireSerial, (int)event->reason);
                e.message = b;
                _lg.write(e);
            }
        }
        }
    }
}

void mpProcessAttackResultPacket(MultiplayerContext& ctx, const AttackResultPacket* event)
{
    bool projectilePending = false;
    auto pendingIt = ctx.pendingAttackRequests.find(event->requestId);
    if (pendingIt != ctx.pendingAttackRequests.end())
    {
        const std::string* weaponId =
            weaponIdForDefNetworkId(pendingIt->second.weaponDefNetworkId);
        const WeaponDefinition* def = weaponId
            ? WeaponRegistry::instance().get(*weaponId)
            : nullptr;
        projectilePending = def &&
            def->executionType == WeaponExecutionType::Projectile &&
            networkWeaponTypeIsProjectile(networkWeaponTypeForDefinition(*def));
        ctx.pendingAttackRequests.erase(pendingIt);
    }

    // Hit-claim resolution: the server's AttackResult now carries a definitive
    // hit verdict, so resolve the client's local hit prediction the moment the
    // round trip completes instead of waiting (or timing out) on a damage event.
    // This is what makes predicted damage correct ~instantly after a shot:
    // agreement keeps the prediction, disagreement rolls it back once.
    auto claimIt = ctx.pendingHitClaims.find(event->requestId);
    if (claimIt != ctx.pendingHitClaims.end())
    {
        if (!event->accepted)
        {
            // Shot rejected outright (spawn/cooldown/etc) — the hit never happened.
            spawnHitClaimDisagreement(ctx, claimIt->second, "HIT REJECTED");
            ctx.pendingHitClaims.erase(claimIt);
        }
        else if (event->hitVerdict == HIT_VERDICT_HIT_CLAIMED_TARGET)
        {
            // Server confirmed damage on the claimed target: keep the prediction;
            // the DamageConfirmed/NpcDamage event applies the exact health.
            claimIt->second.confirmed = true;
            claimIt->second.resolved = true;
            ctx.pendingHitClaims.erase(claimIt);
        }
        else if (event->hitVerdict == HIT_VERDICT_HIT_OTHER_TARGET ||
                 event->hitVerdict == HIT_VERDICT_MISS)
        {
            // The server's rewind trace hit a different target (or nothing):
            // roll back the claimed target's predicted damage/death immediately.
            spawnHitClaimDisagreement(ctx, claimIt->second,
                event->hitVerdict == HIT_VERDICT_HIT_OTHER_TARGET
                    ? "HIT OTHER TARGET" : "HIT MISS");
            ctx.pendingHitClaims.erase(claimIt);
        }
    }

    if (!event->accepted && projectilePending)
        removePredictedProjectileForRequest(ctx, event->requestId);
    else if (event->accepted && event->projectileId != 0 && projectilePending)
        // Reliable adoption: the AttackResult carries the authoritative projectile
        // ID and is retried until it arrives, so the local owner's predicted
        // rocket/grenade never depends on the lossy spawn broadcast to become one
        // entity. If the spawn arrives later, it is treated as already-adopted.
        adoptPredictedProjectile(ctx, event->requestId, event->projectileId);

    if (event->weaponDefNetworkId != 0 &&
        event->magazineAmmo >= 0 && event->reserveAmmo >= 0)
    {
        ctx.pendingAttackResults.push_back(*event);
        if (ctx.pendingAttackResults.size() > 64)
            ctx.pendingAttackResults.erase(ctx.pendingAttackResults.begin());
    }

    printf("[ATTACK RESULT RX] playerId=%u requestId=%u accepted=%d "
           "reason=%d ammo=%d/%d projId=%u projectilePending=%d\n",
           ctx.localPlayerId, event->requestId, (int)event->accepted,
           (int)event->reason, event->magazineAmmo, event->reserveAmmo,
           event->projectileId, (int)projectilePending);
}

void mpProcessProjectileDespawnEventPacket(MultiplayerContext& ctx, const ProjectileDespawnEventPacket* event)
{
    if (!acceptReliableEventOnce(ctx, event->eventId, event->eventSessionId))
    {
        printf("[PROJECTILE CLIENT TERMINAL] eventId=%u projectileId=%u weapon=%s terminal=despawn accepted=0 reason=duplicate-event\n",
               event->eventId, event->projectileId, networkWeaponTypeName(event->weapon));
        return;
    }

    if (!ctx.projectileTerminals.record(event->projectileId))
    {
        printf("[PROJECTILE CLIENT TERMINAL] projectileId=%u weapon=%s terminal=despawn accepted=0 reason=duplicate\n",
               event->projectileId, networkWeaponTypeName(event->weapon));
        return;
    }
    const bool removedVisual = ctx.networkProjectiles.erase(event->projectileId) > 0;
    const bool wasPredicted = ctx.predictedProjectileIds.erase(event->projectileId) > 0;
    bool removedLegacy = false;
    if (gpWeapons)
    {
        removedLegacy = gpWeapons->removeAuthoritativeRocket(event->projectileId);
    }
    printf("[PROJECTILE CLIENT DESPAWN] projectileId=%u weapon=%s reason=%u removedVisual=%d removedLegacy=%d predicted=%d\n",
           event->projectileId, networkWeaponTypeName(event->weapon),
           (unsigned)event->reason, (int)removedVisual,
           (int)removedLegacy, (int)wasPredicted);
}

void mpProcessMeleeHitEventPacket(MultiplayerContext& ctx, const MeleeHitEventPacket* event)
{
    // Animation-only event (targetPlayerId=0): create sword state but skip VFX
    if (event->targetPlayerId != 0)
    {
        NetworkShotEvent out;
        out.shotSerial = event->attackSerial;
        out.shooterPlayerId = event->attackerPlayerId;
        out.targetPlayerId = event->targetPlayerId;
        out.damage = event->damage;
        out.targetHealth = event->targetHealth;
        out.effectFlags =
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD |
            SHOT_EFFECT_HIT_SOUND |
            SHOT_EFFECT_WEAPON_TRIGGER;
        out.weapon = event->weapon;
        out.impactType = SHOT_IMPACT_ENTITY;
        out.killed = event->killed != 0;
        out.damageConfirmed = event->damageConfirmed != 0;
        out.targetTransformEpoch = event->targetTransformEpoch;
        out.hit = {event->hitX, event->hitY, event->hitZ};
        out.normal = {event->normalX, event->normalY, event->normalZ};
        out.knockback = {event->knockX, event->knockY, event->knockZ};
        out.direction = glm::length(out.knockback) > 0.001f
            ? glm::normalize(out.knockback)
            : -out.normal;
        out.origin = out.hit - out.direction;
        ctx.shotEvents.push_back(out);
    }

    // Initialize remote sword state for animation reconstruction
    if (event->attackerPlayerId == 0 || event->attackerPlayerId == ctx.localPlayerId)
        return;
    auto playerIt = ctx.remotePlayers.find(event->attackerPlayerId);
    if (playerIt == ctx.remotePlayers.end())
        return;

    Player& attacker = playerIt->second;
    auto rtIt = attacker.weaponRuntimes.find("swordsword");
    if (rtIt == attacker.weaponRuntimes.end())
    {
        // Ensure runtime exists
        const WeaponDefinition* def = WeaponRegistry::instance().get("swordsword");
        if (!def) return;
        WeaponRuntime& rt = attacker.weaponRuntimes["swordsword"];
        rt = WeaponRuntime{};
        WeaponRuntimeHelper::initRuntime(rt, *def);
    }

    // Set pose state for immediate visual feedback
    WeaponRuntime& rt = attacker.weaponRuntimes["swordsword"];
    rt.shootEffectTimer = 0.18f;
    if (event->attackType == 1)
        rt.customFloats["swordPoseState"] = 1.0f;
    else if (event->attackType == 2)
        rt.customFloats["swordPoseState"] = 2.0f;

    // Create/replace sword state for lifecycle tracking
    SwordswordState& ss = ctx.remoteSwordStates[event->attackerPlayerId];
    ss = SwordswordState{};
    if (event->attackType == 1)
        ss.state = SwordswordState::AttackState::SlashWindup;
    else
        ss.state = SwordswordState::AttackState::LungeWindup;
    ss.stateTimer = 0.0f;
    ss.animTimer = 0.0f;
}

void mpUpdateRemoteSwordStates(MultiplayerContext& ctx, float dt)
{
    const WeaponDefinition* def = WeaponRegistry::instance().get("swordsword");
    if (!def) return;

    float slashWindup  = 0.08f; float slashActive  = 0.15f; float slashRecover = 0.10f;
    float lungeWindup  = 0.10f; float lungeActive  = 0.20f; float lungeRecover = 0.12f;

    auto it = ctx.remoteSwordStates.begin();
    while (it != ctx.remoteSwordStates.end())
    {
        uint32_t attackerId = it->first;
        SwordswordState& ss = it->second;
        Player* attacker = nullptr;
        {
            auto pi = ctx.remotePlayers.find(attackerId);
            if (pi != ctx.remotePlayers.end())
                attacker = &pi->second;
        }

        if (ss.state == SwordswordState::AttackState::Idle)
        {
            it = ctx.remoteSwordStates.erase(it);
            continue;
        }

        // Advance state
        if (ss.state == SwordswordState::AttackState::SlashWindup) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashWindup;
            if (ss.stateTimer >= slashWindup) {
                ss.state = SwordswordState::AttackState::SlashActive;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::SlashActive) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashActive;
            if (ss.stateTimer >= slashActive) {
                ss.state = SwordswordState::AttackState::SlashRecover;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::SlashRecover) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / slashRecover;
            if (ss.stateTimer >= slashRecover) {
                ss.state = SwordswordState::AttackState::Idle;
                if (attacker) {
                    auto rtIt = attacker->weaponRuntimes.find("swordsword");
                    if (rtIt != attacker->weaponRuntimes.end())
                        rtIt->second.customFloats["swordPoseState"] = 0.0f;
                }
                it = ctx.remoteSwordStates.erase(it);
                continue;
            }
        } else if (ss.state == SwordswordState::AttackState::LungeWindup) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeWindup;
            if (ss.stateTimer >= lungeWindup) {
                ss.state = SwordswordState::AttackState::LungeActive;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::LungeActive) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeActive;
            if (ss.stateTimer >= lungeActive) {
                ss.state = SwordswordState::AttackState::LungeRecover;
                ss.stateTimer = 0.0f;
            }
        } else if (ss.state == SwordswordState::AttackState::LungeRecover) {
            ss.stateTimer += dt;
            ss.animTimer = ss.stateTimer / lungeRecover;
            if (ss.stateTimer >= lungeRecover) {
                ss.state = SwordswordState::AttackState::Idle;
                if (attacker) {
                    auto rtIt = attacker->weaponRuntimes.find("swordsword");
                    if (rtIt != attacker->weaponRuntimes.end())
                        rtIt->second.customFloats["swordPoseState"] = 0.0f;
                }
                it = ctx.remoteSwordStates.erase(it);
                continue;
            }
        }

        // Update swordPoseState on remote player's runtime
        if (attacker)
        {
            auto rtIt = attacker->weaponRuntimes.find("swordsword");
            if (rtIt != attacker->weaponRuntimes.end())
            {
                rtIt->second.shootEffectTimer = 0.18f;
                if (ss.state == SwordswordState::AttackState::SlashWindup ||
                    ss.state == SwordswordState::AttackState::LungeWindup)
                    rtIt->second.customFloats["swordPoseState"] = 0.0f;
                else if (ss.state == SwordswordState::AttackState::SlashActive)
                    rtIt->second.customFloats["swordPoseState"] = 1.0f;
                else if (ss.state == SwordswordState::AttackState::LungeActive)
                    rtIt->second.customFloats["swordPoseState"] = 2.0f;
                else if (ss.state == SwordswordState::AttackState::SlashRecover ||
                         ss.state == SwordswordState::AttackState::LungeRecover)
                    rtIt->second.customFloats["swordPoseState"] = 0.0f;
            }
        }

        ++it;
    }
}

void mpUpdateNetworkProjectiles(MultiplayerContext& ctx, float dt, const World& world)
{
    // ── Retransmit unacknowledged fire requests ─────────────────────
    const uint64_t now = nowMs();
    for (auto it = ctx.pendingFireRequests.begin(); it != ctx.pendingFireRequests.end(); )
    {
        MultiplayerContext::PendingFireRequest& pfr = it->second;
        if (pfr.acknowledged || now - pfr.firstSentMs > 3000)
        {
            printf("[FIRE_TIMEOUT] fireSerial=%u weapon=%s — refund queued\n",
                   pfr.fireSerial, networkWeaponTypeName(pfr.weapon));
            if (ctx.processedRefundSerials.count(pfr.fireSerial) == 0)
            {
                MultiplayerContext::FireRejection fr;
                fr.fireSerial = pfr.fireSerial;
                fr.weapon = pfr.weapon;
                fr.reason = 255; // timeout indicator
                fr.cooldownRemaining = 0.0f;
                ctx.fireRejections.push_back(fr);
                ctx.processedRefundSerials.insert(pfr.fireSerial);
            }
            it = ctx.pendingFireRequests.erase(it);
        }
        else
        {
            printf("[PROJECTILE LEGACY PENDING DROP] fireSerial=%u weapon=%s "
                   "reason=legacy-direct-packet-disabled\n",
                   pfr.fireSerial, networkWeaponTypeName(pfr.weapon));
            it = ctx.pendingFireRequests.erase(it);
        }
    }

    std::vector<ClientCollisionWorldView::PlayerReplica> replicas =
        projectileReplicas(ctx);

    for (auto it = ctx.networkProjectiles.begin(); it != ctx.networkProjectiles.end(); )
    {
        NetworkProjectile& projectile = it->second;
        projectile.previousPosition = projectile.position;

        if (networkWeaponTypeIsProjectile(projectile.weaponType) &&
            projectile.radius > 0.0f && dt > 0.0f)
        {
            ClientCollisionWorldView physicsWorld(
                world.collisionMesh,
                world.collisionChunkSize,
                &world.collisionChunks,
                &world.collisionLargeTriangles,
                projectile.ownerPlayerId,
                replicas);
            ProjectilePhysicsState state = makePhysicsState(projectile);
            ProjectilePhysicsConfig config = makePhysicsConfig(projectile);
            ProjectileStepResult step =
                simulateProjectileTick(state, config, physicsWorld, dt);
            applyPhysicsState(projectile, state);
            projectile.distanceTraveled += step.travelDistance;
            if (state.sleeping || step.type == ProjectileCollisionType::WorldBounce ||
                step.type == ProjectileCollisionType::WorldImpact)
                projectile.worldTouched = true;

            // ── Client-side explosion prediction (instant feedback) ──
            // Mirrors the server's explode policy (server-projectiles.cpp) using the
            // same deterministic physics kernel. Only the local owner's predicted
            // projectile predicts the explosion; the server confirm reconciles it.
            if (projectile.predicted &&
                projectile.ownerPlayerId == ctx.localPlayerId &&
                !projectile.exploded)
            {
                bool shouldExplode = false;
                glm::vec3 explodePos = projectile.position;
                if (step.type == ProjectileCollisionType::LifetimeExpired && projectile.explodeOnLifetime)
                {
                    shouldExplode = true;
                    explodePos = projectile.position;
                }
                else if (step.type == ProjectileCollisionType::PlayerImpact && projectile.explodeOnPlayerImpact)
                {
                    shouldExplode = true;
                    explodePos = step.hitPosition;
                }
                else if (step.type == ProjectileCollisionType::WorldImpact && projectile.explodeOnWorldImpact)
                {
                    shouldExplode = true;
                    explodePos = step.hitPosition;
                }
                if (shouldExplode)
                {
                    projectile.exploded = true;
                    const char* weaponId = networkWeaponTypeName(projectile.weaponType);
                    std::string attacker = "player_" + std::to_string(ctx.localPlayerId);
                    auto pi = ctx.playerRegistry.find(ctx.localPlayerId);
                    if (pi != ctx.playerRegistry.end())
                        attacker = pi->second.name;
                    spawnExplosionFx(explodePos, weaponId, attacker);
                    ctx.predictedExplosions[projectile.fireSerial] = explodePos;

                    // ── Predicted blast hit feedback (instant) ──────────────
                    // Replicates the server's splash damage formula so the local
                    // shooter sees hitmarker + hit sound + damage number + HP bar
                    // on every remote target in the blast immediately. The
                    // server's reliable DamageConfirmedEvent reconciles it. The
                    // splash line-of-sight check mirrors the server so predicted
                    // feedback aligns with the authoritative verdict (a target
                    // behind a wall is skipped, not rolled back later).
                    const WeaponDefinition* def = projectileDefinition(projectile.weaponType);
                    if (def)
                    {
                        const float radius = cp(def, "splashRadius", 8.0f);
                        const float splashDamage = cp(def, "rocketDirectDamage", 150.0f);
                        const float exponent = cp(def, "splashExponent", 2.0f);
                        const bool losOn = cp(def, "splashLineOfSight", 1.0f) > 0.0f;
                        const auto applyBlast = [&](Player& t, uint32_t id, bool isNpc) {
                            if (t.dead || t.currentHp <= 0) return;
                            const float d = glm::length(t.pos - explodePos);
                            if (d >= radius) return;
                            if (losOn)
                            {
                                glm::vec3 target;
                                playerSplashBodyPoint(t, explodePos, target);
                                if (!clientSplashHasLineOfSight(world, explodePos, target))
                                    return; // wall/cover → server will also skip
                            }
                            float dv = splashDamage *
                                std::exp(-std::pow(d / radius, 2.0f) * exponent);
                            const int dmg = std::max(1, (int)std::round(dv));
                            WeaponFire::processRemoteBlastHitFeedback(
                                *def, t.pos, t.pos - explodePos, attacker,
                                id, isNpc, t, dmg);
                        };
                        for (auto& kv : ctx.remoteNpcs)
                            applyBlast(kv.second, kv.first, true);
                        for (auto& kv : ctx.remotePlayers)
                            applyBlast(kv.second, kv.first, false);

                        // ── Predicted self-knockback (instant, TF2-style) ──
                        // The shooter feels their own blast immediately using the
                        // server's exact formula; the authoritative explode event
                        // supersedes it (no double push).
                        const float kbStrength = cp(def, "knockbackStrength", 160.0f);
                        const float selfMul = cp(def, "selfKnockbackMultiplier", 1.0f);
                        if (gpPlayer && !gpPlayer->dead)
                        {
                            const Capsule cap = gpPlayer->getCapsule();
                            const float selfH = cap.b.z - cap.a.z + 2.0f * cap.r;
                            const glm::vec3 center =
                                gpPlayer->pos + glm::vec3(0.0f, 0.0f, selfH * 0.25f);
                            const glm::vec3 toSelf = center - explodePos;
                            const float selfDist = glm::length(toSelf);
                            if (selfDist < radius)
                            {
                                glm::vec3 selfTarget;
                                playerSplashBodyPoint(*gpPlayer, explodePos, selfTarget);
                                if (!losOn || clientSplashHasLineOfSight(world, explodePos, selfTarget))
                                {
                                    const glm::vec3 dir = selfDist > 0.001f
                                        ? toSelf / selfDist : glm::vec3(0.0f, 1.0f, 0.0f);
                                    const float t = selfDist / radius;
                                    const float knockScale = (1.0f - t * t) * 0.85f + 0.15f;
                                    const glm::vec3 kb =
                                        dir * kbStrength * knockScale * selfMul;
                                    gpPlayer->externalImpulse += kb;
                                    ctx.predictedSelfKnockbacks[projectile.fireSerial] = kb;
                                    printf("[PROJECTILE CLIENT PREDICTED SELF-KNOCKBACK] fireSerial=%u impulse=(%.2f,%.2f,%.2f)\n",
                                           projectile.fireSerial, kb.x, kb.y, kb.z);
                                }
                            }
                        }
                    }
                    printf("[PROJECTILE CLIENT PREDICTED EXPLOSION] fireSerial=%u weapon=%s "
                           "pos=(%.2f,%.2f,%.2f)\n",
                           projectile.fireSerial, weaponId,
                           explodePos.x, explodePos.y, explodePos.z);
                }
            }
        }
        else
        {
            projectile.age += dt;
            projectile.position += projectile.velocity * dt;
        }

        if (projectile.lifetime > 0.0f && projectile.age > projectile.lifetime + 1.0f)
        {
            logGrenadeEvent(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose,
                "GRENADE_CLIENT_REMOVE",
                "GRENADE_P" + std::to_string(projectile.ownerPlayerId) + "_F" + std::to_string(projectile.fireSerial) + "_J" + std::to_string(projectile.projectileId),
                "lifetime-expired",
                "age=%.6f lifetime=%.6f pos=(%.6f,%.6f,%.6f) vel=(%.6f,%.6f,%.6f) speed=%.6f predicted=%d",
                projectile.age, projectile.lifetime,
                projectile.position.x, projectile.position.y, projectile.position.z,
                projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
                glm::length(projectile.velocity), (int)projectile.predicted);
            it = ctx.networkProjectiles.erase(it);
            continue;
        }

        if (projectile.hasTargetState)
        {
            const glm::vec3 posError = projectile.targetStatePos - projectile.position;
            const glm::vec3 velError = projectile.targetStateVel - projectile.velocity;
            const float err = glm::length(posError);
            if (err > 8.0f)
            {
                projectile.position = projectile.targetStatePos;
                projectile.velocity = projectile.targetStateVel;
                projectile.rotation = projectile.targetStateRot;
            }
            else if (err > 0.01f)
            {
                const float posBlend = projectile.predicted ? 0.12f : 0.35f;
                projectile.position += posError * posBlend;
                projectile.velocity += velError * 0.25f;
                projectile.rotation = glm::normalize(
                    glm::slerp(projectile.rotation, projectile.targetStateRot, 0.20f));
            }
        }

        projectile.renderPosition = projectile.position;
        projectile.renderVelocity = projectile.velocity;
        projectile.renderRotation = projectile.rotation;

        // Rotation: grenades keep integrated quaternion (tumbling); rockets face velocity
        if (projectile.weaponType != NETWORK_WEAPON_GRENADE_LAUNCHER)
        {
            const float renderSpeed = glm::length(projectile.renderVelocity);
            if (renderSpeed > 0.001f)
            {
                projectile.renderRotation = glm::rotation(
                    glm::vec3(0.0f, 0.0f, 1.0f),
                    glm::normalize(projectile.renderVelocity));
            }
        }

        spawnProjectileTrail(projectile, dt);
        ++it;
    }
}

void mpRenderNetworkProjectiles(const MultiplayerContext& ctx, const Camera& camera)
{
    for (const auto& entry : ctx.networkProjectiles)
    {
        const NetworkProjectile& projectile = entry.second;
        if (projectile.exploded)
            continue;
        ProjectileVisualConfig cfg = projectileVisualConfig(projectile.weaponType);
        renderProjectile(camera, projectile.renderPosition, projectile.renderRotation, cfg);
    }
}

} // namespace MimitaNet
