// C:\important\mimita-priv-v8\src\network\multiplayer-projectiles.cpp
/** 7 18 2026 0845
 * purpose
 * todo fill in cuz we need to migrate from multiplayer and duplicated code 
 * to just server/client, even local play, just uses localhost
 */

#include "network/multiplayer-context.h"
#include "network/confirmed-damage-presentation.h"
#include "network/network-weapons.h"
#include "debug/structured-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "audio/audio.h"
#include "camera.h"
#include "combat/projectile-render.h"
#include "combat/weapon-system.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "combat/weapon-types.h"
#include "config/weapon-hitfx-config.h"
#include "effects/effect-part.h"
#include "effects/hit-effects.h"
#include "terminal/terminal-state.h"

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
    const char* id = weapon == NETWORK_WEAPON_GRENADE_LAUNCHER
        ? "grenade_launcher"
        : "rocket_launcher";
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

uint32_t mpSendProjectileFireRequest(
    MultiplayerContext& ctx,
    uint8_t weapon,
    const glm::vec3& origin,
    const glm::vec3& direction)
{
    if (!ctx.active || !ctx.localPlayerId ||
        !networkWeaponTypeIsProjectile(weapon) ||
        !finiteVec(origin) || !finiteVec(direction) ||
        glm::length(direction) <= 0.001f)
        return 0;

    ProjectileFireRequestPacket packet{};
    packet.header.type = PACKET_PROJECTILE_FIRE_REQUEST;
    packet.header.tick = ctx.tick;
    packet.header.playerId = ctx.localPlayerId;
    packet.fireSerial = ctx.nextLocalProjectileFireSerial++;
    if (ctx.nextLocalProjectileFireSerial == 0)
        ctx.nextLocalProjectileFireSerial = 1;
    packet.lastServerTick = ctx.latestServerTick;
    packet.weapon = weapon;
    const glm::vec3 dir = glm::normalize(direction);
    packet.originX = origin.x;
    packet.originY = origin.y;
    packet.originZ = origin.z;
    packet.dirX = dir.x;
    packet.dirY = dir.y;
    packet.dirZ = dir.z;
    mpSendPacket(ctx, &packet, sizeof(packet));

    // Track pending fire request for retransmission
    MultiplayerContext::PendingFireRequest pfr;
    pfr.fireSerial = packet.fireSerial;
    pfr.weapon = weapon;
    pfr.origin = origin;
    pfr.direction = dir;
    pfr.firstSentMs = nowMs();
    pfr.lastSentMs = nowMs();
    pfr.attempts = 1;
    ctx.pendingFireRequests[packet.fireSerial] = pfr;

    logGrenadeEvent(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Important,
        "GRENADE_CLIENT_REQUEST",
        "GRENADE_P" + std::to_string(ctx.localPlayerId) + "_F" + std::to_string(packet.fireSerial) + "_J0",
        "send",
        "playerId=%u fireSerial=%u weapon=%s origin=(%.6f,%.6f,%.6f) dir=(%.6f,%.6f,%.6f) "
        "clientTick=%u pending=%zu attempt=1 transport=%d",
        ctx.localPlayerId, packet.fireSerial, networkWeaponTypeName(weapon),
        origin.x, origin.y, origin.z, dir.x, dir.y, dir.z,
        ctx.tick, ctx.pendingFireRequests.size() + 1, (int)(ctx.transport != nullptr));

    printf("[PROJECTILE FIRE REQUEST SEND] localPlayerId=%u localFireSerial=%u "
           "weapon=%s origin=(%.2f,%.2f,%.2f) direction=(%.3f,%.3f,%.3f) "
           "velocity=(server-derived) clientTick=%u\n",
           ctx.localPlayerId, packet.fireSerial,
           networkWeaponTypeName(weapon),
           origin.x, origin.y, origin.z,
           dir.x, dir.y, dir.z, ctx.tick);
    return packet.fireSerial;
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
    packet.lastServerTick = ctx.latestServerTick;
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

    // Mark pending fire request as acknowledged
    if (event->fireSerial != 0)
        ctx.pendingFireRequests.erase(event->fireSerial);

    // Rocket launcher: local simulation handles it — suppress server interpolation
    if (event->ownerPlayerId == ctx.localPlayerId &&
        event->weapon == NETWORK_WEAPON_ROCKET_LAUNCHER)
    {
        ctx.predictedProjectileIds.insert(event->projectileId);
        if (gpWeapons)
            gpWeapons->attachAuthoritativeRocket(event->fireSerial, event->projectileId);
        printf("[PROJECTILE PREDICTED] projectileId=%u fireSerial=%u weapon=%s "
               "pos=(%.2f,%.2f,%.2f) — suppressed server interpolation\n",
               event->projectileId, event->fireSerial,
               networkWeaponTypeName(event->weapon),
               event->posX, event->posY, event->posZ);
        return;
    }

    NetworkProjectile& projectile = ctx.networkProjectiles[event->projectileId];

    projectile.projectileId = event->projectileId;
    projectile.ownerPlayerId = event->ownerPlayerId;
    projectile.fireSerial = event->fireSerial;
    projectile.weaponType = event->weapon;
    projectile.position = {event->posX, event->posY, event->posZ};
    projectile.previousPosition = projectile.position;
    projectile.velocity = {event->velX, event->velY, event->velZ};
    projectile.rotation = glm::quat(event->rotW, event->rotX, event->rotY, event->rotZ);
    projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
    projectile.age = 0.0f;
    projectile.lifetime = event->lifetime;
    projectile.radius = event->radius;
    projectile.predicted = false; // grenades are never predicted — server is authoritative
    projectile.exploded = false;

    // Initialize render state directly from spawn (first state = immediate render)
    projectile.renderPosition = projectile.position;
    projectile.renderVelocity = projectile.velocity;
    projectile.renderRotation = projectile.rotation;
    projectile.renderAngularVelocity = projectile.angularVelocity;

    // Initialize interpolation state
    projectile.prevStateTick = event->spawnTick;
    projectile.prevStatePos = projectile.position;
    projectile.prevStateVel = projectile.velocity;
    projectile.prevStateRot = projectile.rotation;

    projectile.targetStateTick = event->spawnTick;
    projectile.targetStatePos = projectile.position;
    projectile.targetStateVel = projectile.velocity;
    projectile.targetStateRot = projectile.rotation;

    projectile.latestAcceptedTick = event->spawnTick;
    projectile.lastTargetReceivedMs = nowMs();
    projectile.hasTargetState = true;

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
            "predicted=%d provObjectExists=0 "
            "spawnPos=(%.6f,%.6f,%.6f) spawnVel=(%.6f,%.6f,%.6f)",
            projectile.ownerPlayerId, projectile.fireSerial, projectile.projectileId,
            provCorr.c_str(), (int)pendingMatched,
            (int)projectile.predicted,
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

    // Skip state updates for rocket-predicted projectiles
    if (ctx.predictedProjectileIds.count(event->projectileId))
        return;

    auto it = ctx.networkProjectiles.find(event->projectileId);
    if (it == ctx.networkProjectiles.end())
    {
        // Lost spawn event — recover from state update
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

    // For predicted projectiles, store as correction target without overwriting live state
    if (projectile.predicted)
    {
        projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
        projectile.age = event->age;
        logGrenadeEvent(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose,
            "GRENADE_CLIENT_STATE",
            "GRENADE_P" + std::to_string(projectile.ownerPlayerId) + "_F" + std::to_string(projectile.fireSerial) + "_J" + std::to_string(projectile.projectileId),
            "correction-target",
            "predicted=1 overwrite=0 tick=%u age=%.6f serverPos=(%.6f,%.6f,%.6f) serverVel=(%.6f,%.6f,%.6f) "
            "predictedPos=(%.6f,%.6f,%.6f) predictedVel=(%.6f,%.6f,%.6f) posErr=%.6f velErr=%.6f",
            newTick, event->age,
            event->posX, event->posY, event->posZ,
            event->velX, event->velY, event->velZ,
            projectile.position.x, projectile.position.y, projectile.position.z,
            projectile.velocity.x, projectile.velocity.y, projectile.velocity.z,
            glm::length(glm::vec3(event->posX, event->posY, event->posZ) - projectile.position),
            glm::length(glm::vec3(event->velX, event->velY, event->velZ) - projectile.velocity));
    }
    else
    {
        glm::vec3 oldPredPos = projectile.position;
        glm::vec3 oldPredVel = projectile.velocity;
        projectile.position = projectile.targetStatePos;
        projectile.velocity = projectile.targetStateVel;
        projectile.rotation = projectile.targetStateRot;
        projectile.angularVelocity = {event->angVelX, event->angVelY, event->angVelZ};
        projectile.age = event->age;
        logGrenadeEvent(::StructuredCategory::GrenadeLauncher, ::StructuredLevel::Verbose,
            "GRENADE_CLIENT_STATE",
            "GRENADE_P" + std::to_string(projectile.ownerPlayerId) + "_F" + std::to_string(projectile.fireSerial) + "_J" + std::to_string(projectile.projectileId),
            "overwrite",
            "predicted=0 overwrite=1 tick=%u age=%.6f serverPos=(%.6f,%.6f,%.6f) oldPos=(%.6f,%.6f,%.6f) "
            "serverVel=(%.6f,%.6f,%.6f) oldVel=(%.6f,%.6f,%.6f) posErr=%.6f velErr=%.6f",
            newTick, event->age,
            event->posX, event->posY, event->posZ,
            oldPredPos.x, oldPredPos.y, oldPredPos.z,
            event->velX, event->velY, event->velZ,
            oldPredVel.x, oldPredVel.y, oldPredVel.z,
            glm::length(glm::vec3(event->posX, event->posY, event->posZ) - oldPredPos),
            glm::length(glm::vec3(event->velX, event->velY, event->velZ) - oldPredVel));
    }

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

    playWorldSound(
        event->weapon == NETWORK_WEAPON_GRENADE_LAUNCHER
            ? "grenadelauncher/grenadelauncherexplode"
            : "rocketlauncher/rocketlauncherexplode",
        position, 1.0f, 1.0f, 50.0f);
    EffectPartSystem::instance().spawnMuzzleFlash(position, std::string(weaponName) + "_explosion");
    EffectPartSystem::instance().spawnWorldDebris(position, glm::vec3(0.0f, 0.0f, 1.0f), 3.0f);
    // Explosion smoke burst — config-driven burst for rockets and grenades
    {
        const std::string weaponId = weaponName;
        const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor(weaponId);
        if (expCfg.smoke.enabled)
        {
            for (int i = 0; i < expCfg.smoke.count; ++i)
            {
                EffectPart part;
                part.position = position + glm::vec3(
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.spread);
                part.velocity = glm::vec3(
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                    ((float)rand() / RAND_MAX - 0.5f) * expCfg.smoke.speed,
                    (float)rand() / RAND_MAX * expCfg.smoke.speed * 0.5f + expCfg.smoke.upwardBias);
                part.lifetime = 0.0f;
                part.maxLifetime = expCfg.smoke.lifetime + (float)rand() / RAND_MAX * expCfg.smoke.lifetime * 0.3f;
                part.scale = expCfg.smoke.size + (float)rand() / RAND_MAX * expCfg.smoke.size * 0.5f;
                part.endScale = expCfg.smoke.endSize + (float)rand() / RAND_MAX * expCfg.smoke.endSize * 0.5f;
                part.color = expCfg.smoke.color;
                part.alpha = expCfg.smoke.alpha;
                part.gravity = 1.0f;
                part.affectedByGravity = true;
                part.billboardText = false;
                part.replayType = std::string(weaponName) + "_explosion_smoke";
                EffectPartSystem::instance().spawn(part);
            }
        }
    }

    // ── Config-driven explosion sphere ─────────────────────────────────
    {
        const std::string weaponId = weaponName;
        const auto& expCfg = WeaponHitFxConfig::instance().explosionBurstFor(weaponId);
        if (expCfg.sphere.enabled)
        {
            EffectPart sphere;
            sphere.position = position;
            sphere.maxLifetime = (float)expCfg.sphere.lifetimeTicks / 60.0f;
            sphere.scale = expCfg.sphere.startRadius;
            sphere.endScale = expCfg.sphere.endRadius;
            sphere.color = expCfg.sphere.startColor * expCfg.sphere.brightnessStart;
            sphere.alpha = expCfg.sphere.alphaStart;
            sphere.billboardText = false;
            sphere.replayType = std::string(weaponName) + "_explosion_sphere";
            EffectPartSystem::instance().spawn(sphere);
        }
    }

    HitEvent hit;
    hit.position = position;
    hit.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    hit.hitWorld = true;
    hit.damage = 0;
    hit.attacker = "player_" + std::to_string(event->ownerPlayerId);
    hit.weaponSource = weaponName;
    HitEffects::onHit(hit);

    for (uint8_t i = 0; i < event->victimCount && i < MAX_PROJECTILE_DAMAGE_RESULTS; ++i)
    {
        const ProjectileDamageResultPacket& victim = event->victims[i];
        if (victim.victimPlayerId == ctx.localPlayerId)
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

    printf("[PROJECTILE CLIENT EXPLODE] projectileId=%u weapon=%s "
           "position=(%.2f,%.2f,%.2f) serverTick=%u removedVisual=%d removedLegacy=%d predicted=%d\n",
           event->projectileId, weaponName,
           position.x, position.y, position.z, event->header.tick,
           (int)removedVisual, (int)removedLegacy, (int)wasPredicted);
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

    presentConfirmedDamage(ctx, *event, sink);

    printf("[NET DAMAGE CONFIRMED] eventId=%u source=%u attacker=%u target=%u damage=%d healthBefore=%d healthAfter=%d killed=%d\n",
           event->eventId, (unsigned)event->source, event->attackerPlayerId,
           event->targetPlayerId, event->damage, event->healthBefore,
           event->healthAfter, (int)event->killed);
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

void mpUpdateNetworkProjectiles(MultiplayerContext& ctx, float dt)
{
    // ── Retransmit unacknowledged fire requests ─────────────────────
    const uint64_t now = nowMs();
    for (auto it = ctx.pendingFireRequests.begin(); it != ctx.pendingFireRequests.end(); )
    {
        MultiplayerContext::PendingFireRequest& pfr = it->second;
        if (pfr.acknowledged)
        {
            it = ctx.pendingFireRequests.erase(it);
            continue;
        }
        // Retry every 100ms, up to 10 attempts
        if (now - pfr.lastSentMs >= 100 && pfr.attempts < 10)
        {
            ProjectileFireRequestPacket packet{};
            packet.header.type = PACKET_PROJECTILE_FIRE_REQUEST;
            packet.header.tick = ctx.tick;
            packet.header.playerId = ctx.localPlayerId;
            packet.fireSerial = pfr.fireSerial;
            packet.lastServerTick = ctx.latestServerTick;
            packet.weapon = pfr.weapon;
            packet.originX = pfr.origin.x; packet.originY = pfr.origin.y; packet.originZ = pfr.origin.z;
            packet.dirX = pfr.direction.x; packet.dirY = pfr.direction.y; packet.dirZ = pfr.direction.z;
            mpSendPacket(ctx, &packet, sizeof(packet));
            pfr.lastSentMs = now;
            pfr.attempts++;
            printf("[PROJECTILE FIRE RETRY] fireSerial=%u attempt=%d\n",
                   pfr.fireSerial, pfr.attempts);
        }
        // Timeout after 3 seconds
        if (now - pfr.firstSentMs > 3000)
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
            ++it;
        }
    }

    for (auto it = ctx.networkProjectiles.begin(); it != ctx.networkProjectiles.end(); )
    {
        NetworkProjectile& projectile = it->second;
        projectile.previousPosition = projectile.position;
        projectile.age += dt;

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
            // Normal interpolation or extrapolation
            constexpr float SERVER_TICK_INTERVAL = 1.0f / 60.0f; // 60 Hz server
            constexpr float EXTRAPOLATION_LIMIT = 0.15f; // 150ms max extrapolation

            const uint64_t nowMsVal = nowMs();
            const float elapsedSinceTargetMs = (float)(nowMsVal - projectile.lastTargetReceivedMs);

            if (elapsedSinceTargetMs < 200.0f)
            {
                // Normal interpolation between previous and target
                const float timeBetweenStates = (float)(projectile.targetStateTick - projectile.prevStateTick) * SERVER_TICK_INTERVAL;
                const float timeSincePrev = elapsedSinceTargetMs / 1000.0f;
                const float alpha = timeBetweenStates > 0.001f
                    ? std::clamp(timeSincePrev / timeBetweenStates, 0.0f, 1.0f)
                    : 1.0f;

                projectile.renderPosition = glm::mix(projectile.prevStatePos, projectile.targetStatePos, alpha);
                projectile.renderVelocity = glm::mix(projectile.prevStateVel, projectile.targetStateVel, alpha);
                projectile.renderRotation = glm::normalize(glm::slerp(projectile.prevStateRot, projectile.targetStateRot, alpha));
            }
            else
            {
                // Extrapolation: continue from target state with its velocity
                const float extrapSec = std::min((elapsedSinceTargetMs - 200.0f) / 1000.0f, EXTRAPOLATION_LIMIT);
                projectile.renderPosition = projectile.targetStatePos + projectile.targetStateVel * extrapSec;
                projectile.renderVelocity = projectile.targetStateVel;
                projectile.renderRotation = projectile.targetStateRot;
            }
        }
        else
        {
            // Fallback: just use authoritative position directly
            projectile.renderPosition = projectile.position;
            projectile.renderVelocity = projectile.velocity;
        }

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
