// 07 21 2026, 21 30
/* purpose
* Implements miscellaneous legacy and utility server packet handlers.
* Keeps non-migrated packet handling for chat, ping, NPC, reload, spawn, and compatibility paths.
* Leaves Stage 4A weapon-specific shot, pellet, melee, and Godball packets inert.
* Does NOT own generic AttackRequest execution, projectile ticking, or transport receive loops.
* Does NOT trust client weapon damage, target, health, death, or contact claims.
* Does NOT implement client rendering, audio, or local prediction.
*/

#include "network/server.h"
#include "network/multiplayer-context.h"
#include "network/network-weapons.h"
#include "network/disagreement-visuals.h"
#include "network/simulation-constants.h"
#include "config/networking-config.h"
#include "void-death/void-death.h"
#include "combat/pellet-pattern.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-fire.h"
#include "combat/weapon-execution.h"
#include "physics/movement/physics-collision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace MimitaNet {

void handleShotRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       const HeadlessWorld& world,
                       uint32_t tick, uint64_t& totalPacketsOut,
                       DisagreementRetransmitState* retransmitState)
{
    if (bytes < (int)sizeof(ShotRequestPacket))
        return;
    const ShotRequestPacket* shot =
        reinterpret_cast<const ShotRequestPacket*>(buffer);
    (void)sock;
    (void)from;
    (void)players;
    (void)world;
    (void)tick;
    (void)totalPacketsOut;
    (void)retransmitState;
    (void)shot;
    return;
    auto shooterIt = players.find(shot->header.playerId);
    const bool ownsShooter =
        shooterIt != players.end() &&
        sameAddress(shooterIt->second.addr, from);
    if (!ownsShooter)
    {
        printf("%s [NET SHOT OWNERSHIP] claimedShooter=%u "
               "accepted=0 reason=sender-address-mismatch\n",
               serverTimestamp(), shot->header.playerId);
        return;
    }

    ServerPlayer& shooter = shooterIt->second;
    if (shooter.dead ||
        (shooter.lastShotSerial != 0 &&
         (int32_t)(shot->shotSerial - shooter.lastShotSerial) <= 0))
    {
        printf("%s [NET SHOT FILTER] shooter=%u serial=%u "
               "accepted=0 reason=%s lastSerial=%u\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shooter.dead ? "dead" : "duplicate-or-stale",
               shooter.lastShotSerial);
        return;
    }

    const bool validWeapon =
        std::strcmp(networkWeaponTypeName(shot->weapon), "unknown") != 0 &&
        shot->weapon != NETWORK_WEAPON_NONE &&
        !networkWeaponTypeIsProjectile(shot->weapon);
    const bool validImpact =
        shot->impactType <= SHOT_IMPACT_ENTITY;
    const glm::vec3 origin{
        shot->originX, shot->originY, shot->originZ};
    const glm::vec3 position{
        shot->hitX, shot->hitY, shot->hitZ};
    const glm::vec3 direction{
        shot->dirX, shot->dirY, shot->dirZ};
    const glm::vec3 normal{
        shot->normalX, shot->normalY, shot->normalZ};
    const bool finite =
        std::isfinite(origin.x) && std::isfinite(origin.y) &&
        std::isfinite(origin.z) && std::isfinite(position.x) &&
        std::isfinite(position.y) && std::isfinite(position.z) &&
        std::isfinite(direction.x) && std::isfinite(direction.y) &&
        std::isfinite(direction.z) &&
        std::isfinite(normal.x) && std::isfinite(normal.y) &&
        std::isfinite(normal.z) && std::isfinite(shot->power);
    const float shotDistance = finite
        ? glm::length(position - origin)
        : std::numeric_limits<float>::infinity();
    const float originDistance = finite
        ? glm::length(origin - shooter.pos)
        : std::numeric_limits<float>::infinity();
    const float directionLength = finite
        ? glm::length(direction)
        : 0.0f;
    const bool validGeometry =
        finite && shotDistance <= 150.0f &&
        originDistance <= 8.0f &&
        directionLength >= 0.5f && directionLength <= 1.5f;
    if (!validWeapon || !validImpact || !validGeometry ||
        shot->shotSerial == 0)
    {
        printf("%s [NET SHOT FILTER] shooter=%u serial=%u "
               "accepted=0 weapon=%u weaponName=%s impact=%u finite=%d "
               "distance=%.2f originDistance=%.2f dirLength=%.2f\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shot->weapon, networkWeaponTypeName(shot->weapon),
               shot->impactType, (int)finite,
               shotDistance, originDistance, directionLength);
        return;
    }
    shooter.lastShotSerial = shot->shotSerial;

    constexpr uint16_t ALLOWED_EFFECT_FLAGS =
        SHOT_EFFECT_MUZZLE |
        SHOT_EFFECT_TRACER |
        SHOT_EFFECT_SHOOT_SOUND |
        SHOT_EFFECT_WORLD_IMPACT |
        SHOT_EFFECT_DEBRIS |
        SHOT_EFFECT_ENTITY_IMPACT |
        SHOT_EFFECT_BLOOD |
        SHOT_EFFECT_HIT_SOUND |
        SHOT_EFFECT_WEAPON_TRIGGER;

    ShotEventPacket event{};
    event.header.type = PACKET_SHOT_EVENT;
    event.header.tick = tick;
    event.header.playerId = shooter.id;
    event.shotSerial = shot->shotSerial;
    event.clientTimeMs = shot->clientTimeMs;
    event.shooterPlayerId = shooter.id;
    event.targetPlayerId = shot->targetPlayerId;
    event.power = std::clamp(shot->power, 0.0f, 999.0f);
    event.effectFlags = shot->effectFlags & ALLOWED_EFFECT_FLAGS;
    event.weapon = shot->weapon;
    event.impactType = shot->impactType;
    if (event.impactType == SHOT_IMPACT_NONE)
    {
        event.effectFlags &= ~(
            SHOT_EFFECT_WORLD_IMPACT |
            SHOT_EFFECT_DEBRIS |
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD |
            SHOT_EFFECT_HIT_SOUND);
    }
    else if (event.impactType == SHOT_IMPACT_WORLD)
    {
        event.effectFlags &= ~(
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD);
    }
    else
    {
        event.effectFlags &= ~(
            SHOT_EFFECT_WORLD_IMPACT |
            SHOT_EFFECT_DEBRIS);
    }
    event.originX = origin.x;
    event.originY = origin.y;
    event.originZ = origin.z;
    event.hitX = position.x;
    event.hitY = position.y;
    event.hitZ = position.z;
    const glm::vec3 normalizedDirection =
        glm::normalize(direction);
    event.dirX = normalizedDirection.x;
    event.dirY = normalizedDirection.y;
    event.dirZ = normalizedDirection.z;
    const glm::vec3 normalizedNormal =
        glm::length(normal) > 0.001f
        ? glm::normalize(normal)
        : -normalizedDirection;
    event.normalX = normalizedNormal.x;
    event.normalY = normalizedNormal.y;
    event.normalZ = normalizedNormal.z;
    event.knockX = shot->knockX;
    event.knockY = shot->knockY;
    event.knockZ = shot->knockZ;
    // Stamp the true server shot tick (not the client's pose tick) so the
    // observer event-timeline holds the visual until the body renders this
    // tick — attacks and movement render in the same step.
    event.lastServerTick = tick;

    auto targetIt = players.find(shot->targetPlayerId);
    bool damageConfirmed = false;

    // ── Track detailed rejection state for disagreement broadcasts ──
    DisagreementReason rejectionReason = DISAGREEMENT_INVALID_DAMAGE;
    const char* rejectionDescription = "REJECTED: INVALID DAMAGE";
    glm::vec3 authoritativePosition = position;

    if (shot->weapon == NETWORK_WEAPON_SHOTGUN)
    {
        printf("%s [SHOTGUN SERVER REQUEST] shooter=%u serial=%u target=%u "
               "claimedDamage=%d accepted=pending reason=validation-start\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shot->targetPlayerId, shot->damage);
    }

    if (MimitaNet::gNetDamageDebug)
    {
        printf("[NET DAMAGE] shooter=%u target=%u impactType=%u "
               "damage=%d shooterDead=%d targetDead=%d "
               "shooterHealth=%d targetHealth=%d\n",
               shooter.id, shot->targetPlayerId, shot->impactType,
               shot->damage, (int)shooter.dead,
               (int)(targetIt != players.end() && targetIt->second.dead),
               shooter.health,
               targetIt != players.end() ? targetIt->second.health : -1);
    }

    // Shotgun aggregates 15 pellet damages; set adequate per-weapon cap.
    constexpr int MAX_HITSCAN_DAMAGE = 200;
    constexpr int MAX_SHOTGUN_DAMAGE = 400;
    const int damageCap = (shot->weapon == NETWORK_WEAPON_SHOTGUN)
        ? MAX_SHOTGUN_DAMAGE : MAX_HITSCAN_DAMAGE;

    // Determine exact rejection reason before validation
    if (shot->impactType == SHOT_IMPACT_ENTITY)
    {
        if (targetIt == players.end())
        {
            rejectionReason = DISAGREEMENT_TARGET_NOT_FOUND;
            rejectionDescription = "REJECTED: TARGET NOT FOUND";
        }
        else if (shooterIt == targetIt)
        {
            rejectionReason = DISAGREEMENT_SELF_TARGET;
            rejectionDescription = "REJECTED: INVALID SELF TARGET";
        }
        else if (targetIt->second.dead)
        {
            rejectionReason = DISAGREEMENT_TARGET_DEAD;
            rejectionDescription = "REJECTED: TARGET ALREADY DEAD";
        }
        else if (shot->damage <= 0 || shot->damage > damageCap)
        {
            rejectionReason = DISAGREEMENT_INVALID_DAMAGE;
            rejectionDescription = "REJECTED: INVALID DAMAGE";
        }
    }

    // Debug flag: reject all hits to force disagreement VFX
    if (isRejectAllHitsEnabled() && shot->impactType == SHOT_IMPACT_ENTITY)
    {
        rejectionReason = DISAGREEMENT_INVALID_DAMAGE;
        rejectionDescription = "REJECTED: ALL HITS REJECTED (debug)";
    }

    if (shot->impactType == SHOT_IMPACT_ENTITY &&
        targetIt != players.end() &&
        shooterIt != targetIt &&
        !targetIt->second.dead &&
        shot->damage > 0 && shot->damage <= damageCap &&
        !isRejectAllHitsEnabled())
    {
        ServerPlayer& target = targetIt->second;
        event.targetTransformEpoch = target.transformEpoch;

        // Same rewind compensation as estimateServerRewindTick: shift the
        // pellet path's rewind tick by the tunable rewind_compensation_ms so
        // hits land on the body the shooter rendered under jitter/smoothing.
        const int64_t compTicks = (int64_t)std::llround(
            NetworkingConfig::instance().data().remotePlayers
                .rewindCompensationSeconds * (double)GAMEPLAY_SIMULATION_HZ);
        const int64_t rawRewind = (int64_t)shot->lastServerTick - compTicks;
        const uint32_t rewindTick = (uint32_t)std::clamp<int64_t>(
            rawRewind, 0, (int64_t)shot->lastServerTick);

        glm::vec3 rewoundPos;
        bool hasRewound = getPositionAtTick(target, rewindTick, rewoundPos);

        glm::vec3 checkPos = hasRewound
            ? rewoundPos
            : target.pos;

        const float rewindDistance = glm::length(
            position - (checkPos + glm::vec3(0.0f, 0.0f, 0.8f)));

        if (gNetHitDebug)
        {
            printf("[NET HIT] shooter=%u target=%u "
                   "origin=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) "
                   "claimedHit=(%.2f,%.2f,%.2f) "
                   "rewindTick=%u rewindDist=%.2f "
                   "targetRewoundPos=(%.2f,%.2f,%.2f) "
                   "targetCurrentPos=(%.2f,%.2f,%.2f)\n",
                   shooter.id, shot->targetPlayerId,
                   origin.x, origin.y, origin.z,
                   direction.x, direction.y, direction.z,
                   position.x, position.y, position.z,
                   rewindTick, rewindDistance,
                   checkPos.x, checkPos.y, checkPos.z,
                   target.pos.x, target.pos.y, target.pos.z);
        }

        const float hitTolerance =
            NetworkingConfig::instance().data().remotePlayers.rewindHitTolerance;
        if (rewindDistance <= hitTolerance)
        {
            glm::vec3 shotDir = glm::normalize(direction);
            glm::vec3 worldHit, worldNormal;
            bool hitWorld = serverRaycastWorld(
                origin, shotDir, shotDistance, world, worldHit, worldNormal);

            bool occluded = hitWorld && glm::length(worldHit - origin) < shotDistance - 0.1f;

            if (gNetHitDebug)
            {
                printf("[NET HIT OCCLUSION] hitWorld=%d occluded=%d "
                       "worldHitDist=%.2f claimedDist=%.2f\n",
                       (int)hitWorld, (int)occluded,
                       hitWorld ? glm::length(worldHit - origin) : 0.0f,
                       rewindDistance);
            }

            if (!occluded)
            {
                damageConfirmed = true;
                event.damage = shot->damage;
                ServerDamageResult damage = applyServerDamage(
                    players, target, shooter.id, shot->damage,
                    glm::vec3(shot->knockX, shot->knockY, shot->knockZ),
                    ServerDamageSource::Hitscan);
                queueServerDamageConfirmedEvent(
                    sock, players, tick, totalPacketsOut, shooter.id, target,
                    shot->damage, damage, position, normalizedNormal,
                    glm::vec3(shot->knockX, shot->knockY, shot->knockZ),
                    ServerDamageSource::Hitscan, shot->weapon, shot->shotSerial);
                event.targetHealth = damage.healthAfter;
                event.damageConfirmed = 1;
                if (damage.killed)
                    event.killed = 1;

                printf("%s [NET SHOT REWIND] shooter=%u target=%u "
                       "rewoundTick=%u rewindDist=%.2f occluded=%d "
                       "hasHistory=%d\n",
                       serverTimestamp(), shooter.id, target.id,
                       shot->lastServerTick, rewindDistance,
                       (int)occluded, (int)hasRewound);
            }
            else
            {
                rejectionReason = DISAGREEMENT_OCCLUDED_SHOT;
                rejectionDescription = "REJECTED: WORLD BLOCKED SHOT";
                authoritativePosition = worldHit;
                printf("%s [NET SHOT OCCLUDED] shooter=%u target=%u "
                       "worldHit=%.2f < hitDist=%.2f\n",
                       serverTimestamp(), shooter.id, target.id,
                       glm::length(worldHit - origin), rewindDistance);
            }
        }
        else
        {
            rejectionReason = DISAGREEMENT_REWIND_MISS;
            rejectionDescription = "REJECTED: TARGET NOT AT HIT POSITION";
            authoritativePosition = checkPos + glm::vec3(0.0f, 0.0f, 0.8f);
            printf("%s [NET SHOT REWIND MISS] shooter=%u target=%u "
                   "rewoundTick=%u rewindDist=%.2f (<=2.5f required) "
                   "currentDist=%.2f hasHistory=%d\n",
                   serverTimestamp(), shooter.id, target.id,
                   shot->lastServerTick, rewindDistance,
                   glm::length(position - (target.pos + glm::vec3(0,0,0.8f))),
                   (int)hasRewound);
        }
    }

    if (shot->weapon == NETWORK_WEAPON_SHOTGUN)
    {
        printf("%s [SHOTGUN SERVER REQUEST] shooter=%u serial=%u target=%u "
               "claimedDamage=%d accepted=%d reason=%s\n",
               serverTimestamp(), shooter.id, shot->shotSerial,
               shot->targetPlayerId, shot->damage, (int)damageConfirmed,
               damageConfirmed ? "accepted" : "validation-failed");
    }

    if (!damageConfirmed && event.impactType == SHOT_IMPACT_ENTITY)
    {
        printf("%s [SERVER HIT DISAGREEMENT] eventId=%u shotSerial=%u "
               "shooter=%u target=%u reason=%s "
               "claimedHit=(%.2f,%.2f,%.2f) "
               "authoritative=(%.2f,%.2f,%.2f) "
               "correction=(%.2f,%.2f,%.2f) "
               "rewindTick=%u serverTick=%u\n",
               serverTimestamp(), 0u, shot->shotSerial,
               shooter.id, shot->targetPlayerId, rejectionDescription,
               position.x, position.y, position.z,
               authoritativePosition.x, authoritativePosition.y, authoritativePosition.z,
               authoritativePosition.x - position.x,
               authoritativePosition.y - position.y,
               authoritativePosition.z - position.z,
               shot->lastServerTick, tick);

        glm::vec3 correction = authoritativePosition - position;

        // Generate a unique event ID from serial + tick
        static uint32_t localEventCounter = 0;
        ++localEventCounter;
        uint32_t disagreementEventId = localEventCounter;

        sendDisagreementToAll(sock, players,
                              rejectionReason,
                              disagreementEventId,
                              shot->shotSerial,
                              shooter.id,
                              shot->targetPlayerId,
                              position,
                              correction,
                              rejectionDescription,
                              tick, totalPacketsOut,
                              retransmitState);

        event.targetPlayerId = 0;
        event.impactType = SHOT_IMPACT_NONE;
        event.effectFlags &= ~(
            SHOT_EFFECT_ENTITY_IMPACT |
            SHOT_EFFECT_BLOOD |
            SHOT_EFFECT_HIT_SOUND);
    }

    printf("%s [NET SHOT RELAY] shooter=%u serial=%u target=%u "
           "weapon=%u impact=%u flags=0x%03x damageConfirmed=%d\n",
           serverTimestamp(), shooter.id, event.shotSerial,
           event.targetPlayerId, event.weapon, event.impactType,
           event.effectFlags, (int)event.damageConfirmed);

    for (const auto& playerEntry : players)
    {
        if (playerEntry.second.transport)
            playerEntry.second.transport->send(&event, sizeof(event));
        else
            sendto(sock, (const char*)&event, sizeof(event), 0,
                   (sockaddr*)&playerEntry.second.addr,
                   sizeof(playerEntry.second.addr));
        ++totalPacketsOut;
    }
}

void handlePing(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                uint32_t tick, const ServerPlayer* authenticatedPlayer)
{
    if (bytes < (int)sizeof(PingPacket))
        return;
    PingPacket pong =
        *reinterpret_cast<const PingPacket*>(buffer);
    pong.header.tick = tick;
    if (authenticatedPlayer)
        serverSendToPlayer(sock, *authenticatedPlayer, &pong, sizeof(pong));
    else
        sendto(sock, (const char*)&pong, sizeof(pong), 0,
               (sockaddr*)&from, sizeof(from));
}

// ── Pellet blast request handler ─────────────────────────────────────
void handlePelletBlastRequest(SOCKET sock, const sockaddr_in& from, const char* buffer, int bytes,
                               std::unordered_map<uint32_t, ServerPlayer>& players,
                               const HeadlessWorld& world,
                               uint32_t tick, uint64_t& totalPacketsOut,
                               DisagreementRetransmitState* retransmitState)
{
    (void)retransmitState;
    if (bytes < (int)sizeof(PelletBlastRequestPacket))
        return;
    const PelletBlastRequestPacket* request =
        reinterpret_cast<const PelletBlastRequestPacket*>(buffer);
    (void)sock;
    (void)from;
    (void)players;
    (void)world;
    (void)tick;
    (void)totalPacketsOut;
    (void)retransmitState;
    (void)request;
    return;
    auto shooterIt = players.find(request->header.playerId);
    const bool ownsShooter =
        shooterIt != players.end() &&
        sameAddress(shooterIt->second.addr, from);
    if (!ownsShooter)
    {
        printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u accepted=0 reason=sender-address-mismatch\n",
               serverTimestamp(), request->header.playerId);
        return;
    }
    ServerPlayer& shooter = shooterIt->second;
    if (shooter.dead)
    {
        printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u serial=%u accepted=0 reason=dead\n",
               serverTimestamp(), shooter.id, request->shotSerial);
        return;
    }
    // Serial dedup
    if (request->shotSerial != 0 &&
        shooter.lastShotSerial != 0 &&
        (int32_t)(request->shotSerial - shooter.lastShotSerial) <= 0)
    {
        printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u serial=%u accepted=0 reason=duplicate-or-stale\n",
               serverTimestamp(), shooter.id, request->shotSerial);
        return;
    }
    shooter.lastShotSerial = request->shotSerial;

    const glm::vec3 origin(request->originX, request->originY, request->originZ);
    const glm::vec3 baseDir(request->baseDirX, request->baseDirY, request->baseDirZ);
    const float originDist = glm::length(origin - shooter.pos);
    const float dirLen = glm::length(baseDir);
    if (originDist > 8.0f || dirLen < 0.5f || dirLen > 1.5f)
    {
        printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u serial=%u accepted=0 "
               "reason=invalid-geometry originDist=%.2f dirLen=%.2f\n",
               serverTimestamp(), shooter.id, request->shotSerial, originDist, dirLen);
        return;
    }

    uint8_t equippedWeapon = networkWeaponTypeForSlot(shooter.equippedSlot);
    if (equippedWeapon != request->weapon)
    {
        printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u serial=%u accepted=0 "
               "reason=weapon-mismatch requested=%u equipped=%u\n",
               serverTimestamp(), shooter.id, request->shotSerial,
               request->weapon, equippedWeapon);
        return;
    }

    const char* weaponId = nullptr;
    if (request->weapon == NETWORK_WEAPON_SHOTGUN)
        weaponId = "shotgun";
    else if (request->weapon == NETWORK_WEAPON_AA12)
        weaponId = "aa12";
    else
    {
        printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u serial=%u accepted=0 reason=unsupported-weapon\n",
               serverTimestamp(), shooter.id, request->shotSerial);
        return;
    }

    const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
    if (!def)
    {
        printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u serial=%u accepted=0 reason=weapon-not-found\n",
               serverTimestamp(), shooter.id, request->shotSerial);
        return;
    }

    printf("%s [PELLET BLAST SERVER REQUEST] shooter=%u serial=%u weapon=%s accepted=1 "
           "originDist=%.2f dirLen=%.2f\n",
           serverTimestamp(), shooter.id, request->shotSerial, weaponId, originDist, dirLen);

    const glm::vec3 dir = glm::normalize(baseDir);
    constexpr float MAX_SHOT_DISTANCE = 100.0f;
    const int pelletCount = std::max(1, def->pelletCount);
    const float beamThickness = def->beamThickness;

    // Generate pellet directions (same pattern as client)
    PelletPatternConfig ppc;
    ppc.pelletCount = pelletCount;
    ppc.spreadDegrees = def->spread;
    ppc.spreadSeed = request->spreadSeed;
    glm::vec3 pelletDirs[MAX_PELLETS_PER_BLAST];
    int generated = generatePelletDirections(dir, ppc, pelletDirs, MAX_PELLETS_PER_BLAST);

    // Push origin forward slightly
    glm::vec3 rayOrigin = origin + dir * 0.01f;
    const bool useSphereCast = beamThickness > 0.0f;

    // Use authoritative server player dimensions
    const float kPlayerRadius = PLAYER_RADIUS;
    const float kPlayerHeight = PLAYER_HEIGHT;

    // Per-target accumulator
    struct TargetAccum {
        uint32_t id = 0;
        int pelletsHit = 0;
        int headPellets = 0;
        int torsoPellets = 0;
        int totalDamage = 0;
        glm::vec3 totalKnockback{0.0f};
        int healthAfter = 0;
        bool killed = false;
    };
    TargetAccum targets[MAX_PLAYERS];
    int targetCount = 0;

    NetworkPelletResult results[MAX_NETWORK_PELLETS] = {};
    int worldHits = 0, playerHits = 0, misses = 0;

    for (int p = 0; p < generated && p < MAX_NETWORK_PELLETS; ++p)
    {
        NetworkPelletResult& r = results[p];
        r.pelletIndex = (uint8_t)p;
        r.impactType = PELLET_IMPACT_NONE;
        const glm::vec3& pelletDir = pelletDirs[p];

        float nearest = MAX_SHOT_DISTANCE;
        glm::vec3 hitPos = rayOrigin + pelletDir * MAX_SHOT_DISTANCE;
        glm::vec3 hitNml = -pelletDir;
        uint32_t hitPlayerId = 0;

        // ── Player collision — ray vs axis-aligned bounding box ──
        for (auto& entry : players)
        {
            const ServerPlayer& target = entry.second;
            if (target.id == shooter.id || target.dead) continue;

            glm::vec3 mn(
                target.pos.x - kPlayerRadius,
                target.pos.y - kPlayerRadius,
                target.pos.z - kPlayerHeight * 0.5f);
            glm::vec3 mx(
                target.pos.x + kPlayerRadius,
                target.pos.y + kPlayerRadius,
                target.pos.z + kPlayerHeight * 0.5f);

            float d = 0.0f;
            glm::vec3 nml;
            if (WeaponFire::rayAabb(rayOrigin, pelletDir, mn, mx, d, nml) &&
                d > 0.01f && d < nearest)
            {
                nearest = d;
                hitPos = rayOrigin + pelletDir * d;
                hitPlayerId = target.id;
                hitNml = nml;
                float hitFrac = (hitPos.z - mn.z) / (mx.z - mn.z);
                r.bodyPart = hitFrac > 0.78f ? 0       // head
                           : hitFrac > 0.32f ? 1       // torso
                           : hitFrac > 0.15f ? 2       // arms
                           : (uint8_t)3;               // legs
            }
        }

        // ── World collision — iterate all world triangles ──
        for (const CollisionTriangle& tri : world.triangles)
        {
            float d = 0.0f;
            glm::vec3 n(0.0f), p(0.0f);

            if (useSphereCast)
            {
                if (::sweptSphereTriangle(rayOrigin, pelletDir, beamThickness,
                                         tri, nearest, d, n, p) && d < nearest)
                {
                    nearest = d;
                    hitPos = p;
                    hitNml = n;
                    hitPlayerId = 0;
                }
            }
            else
            {
                if (WeaponFire::rayTriangle(rayOrigin, pelletDir, tri, d) && d >= 0.01f && d < nearest)
                {
                    nearest = d;
                    hitPos = rayOrigin + pelletDir * d;
                    hitNml = tri.normal;
                    hitPlayerId = 0;
                }
            }
        }

        if (hitPlayerId != 0)
            r.bodyPart = r.bodyPart;
        else
            r.bodyPart = 0;

        r.hitX = hitPos.x; r.hitY = hitPos.y; r.hitZ = hitPos.z;
        r.normalX = hitNml.x; r.normalY = hitNml.y; r.normalZ = hitNml.z;
        r.targetPlayerId = hitPlayerId;

        printf("[PELLET COLLISION DEBUG] serial=%u pellet=%d dir=(%.4f,%.4f,%.4f) "
               "hitType=%s target=%d dist=%.2f hitPos=(%.2f,%.2f,%.2f)\n",
               request->shotSerial, p,
               pelletDir.x, pelletDir.y, pelletDir.z,
               hitPlayerId ? "player" : (nearest < MAX_SHOT_DISTANCE ? "world" : "miss"),
               hitPlayerId, nearest, hitPos.x, hitPos.y, hitPos.z);

        r.hitX = hitPos.x; r.hitY = hitPos.y; r.hitZ = hitPos.z;
        r.normalX = hitNml.x; r.normalY = hitNml.y; r.normalZ = hitNml.z;
        r.targetPlayerId = hitPlayerId;

        if (hitPlayerId != 0)
        {
            r.impactType = PELLET_IMPACT_PLAYER;

            // Per-pellet damage with body-part multiplier
            const std::string bodyPart =
                r.bodyPart == 0 ? "head" : (r.bodyPart == 2 || r.bodyPart == 3 ? "leg" : "torso");
            const float dmgF = (float)WeaponExecution::computeHitscanDamage(
                *def, bodyPart, nearest, 1.0f);
            int pelletDamage = std::max(1, (int)std::round(dmgF));

            // Accumulate per target
            bool found = false;
            for (int t = 0; t < targetCount; ++t)
            {
                if (targets[t].id == hitPlayerId)
                {
                    targets[t].pelletsHit++;
                    if (r.bodyPart == 0) targets[t].headPellets++;
                    else targets[t].torsoPellets++;
                    targets[t].totalDamage += pelletDamage;
                    targets[t].totalKnockback += pelletDir * (float)pelletDamage * 0.08f;
                    found = true;
                    break;
                }
            }
            if (!found && targetCount < MAX_PLAYERS)
            {
                targets[targetCount].id = hitPlayerId;
                targets[targetCount].pelletsHit = 1;
                if (r.bodyPart == 0) targets[targetCount].headPellets = 1;
                else targets[targetCount].torsoPellets = 1;
                targets[targetCount].totalDamage = pelletDamage;
                targets[targetCount].totalKnockback = pelletDir * (float)pelletDamage * 0.08f;
                targetCount++;
            }
            playerHits++;
        }
        else if (nearest < MAX_SHOT_DISTANCE)
        {
            r.impactType = PELLET_IMPACT_WORLD;
            worldHits++;
        }
        else
        {
            misses++;
        }
    }

    // Apply accumulated damage per target and store results for event
    for (int t = 0; t < targetCount && t < MAX_PELLET_BLAST_TARGETS; ++t)
    {
        int damage = std::min(targets[t].totalDamage, 400);
        glm::vec3 knockback = targets[t].totalKnockback;
        ServerDamageResult dmg = applyServerDamage(
            players, players[targets[t].id], shooter.id,
            damage, knockback, ServerDamageSource::Hitscan);
        queueServerDamageConfirmedEvent(
            sock, players, tick, totalPacketsOut, shooter.id, players[targets[t].id],
            damage, dmg, players[targets[t].id].pos + glm::vec3(0.0f, 0.0f, 0.8f),
            dir, knockback, ServerDamageSource::Hitscan,
            request->weapon, request->shotSerial);
        targets[t].healthAfter = dmg.healthAfter;
        targets[t].killed = dmg.killed;

        printf("[PELLET TARGET RESULT] serial=%u target=%u pelletsHit=%d "
               "head=%d torso=%d damage=%d healthBefore=%d healthAfter=%d "
               "knockback=(%.2f,%.2f,%.2f) killed=%d\n",
               request->shotSerial, targets[t].id, targets[t].pelletsHit,
               targets[t].headPellets, targets[t].torsoPellets,
               damage, dmg.healthBefore, dmg.healthAfter,
               knockback.x, knockback.y, knockback.z, (int)dmg.killed);
    }

    // Build and broadcast event
    PelletBlastEventPacket event{};
    event.header.type = PACKET_PELLET_BLAST_EVENT;
    event.header.tick = tick;
    event.shotSerial = request->shotSerial;
    event.clientTimeMs = request->clientTimeMs;
    event.shooterPlayerId = shooter.id;
    event.spreadSeed = request->spreadSeed;
    event.lastServerTick = tick;
    event.originX = origin.x; event.originY = origin.y; event.originZ = origin.z;
    event.baseDirX = dir.x; event.baseDirY = dir.y; event.baseDirZ = dir.z;
    event.weapon = request->weapon;
    event.pelletCount = (uint8_t)generated;
    event.targetCount = (uint8_t)std::min(targetCount, MAX_PELLET_BLAST_TARGETS);
    memcpy(event.pellets, results, sizeof(NetworkPelletResult) * generated);
    for (int t = 0; t < event.targetCount; ++t)
    {
        event.targets[t].targetPlayerId = targets[t].id;
        event.targets[t].totalDamage = (int16_t)std::min(targets[t].totalDamage, 32767);
        event.targets[t].healthAfter = (int16_t)std::clamp(targets[t].healthAfter, -32768, 32767);
        event.targets[t].knockX = (int16_t)std::clamp((int)targets[t].totalKnockback.x, -32768, 32767);
        event.targets[t].knockY = (int16_t)std::clamp((int)targets[t].totalKnockback.y, -32768, 32767);
        event.targets[t].knockZ = (int16_t)std::clamp((int)targets[t].totalKnockback.z, -32768, 32767);
        event.targets[t].pelletsHit = (uint8_t)targets[t].pelletsHit;
        event.targets[t].killed = targets[t].killed ? 1 : 0;
        auto victimIt = players.find(targets[t].id);
        event.targets[t].targetSpawnGeneration =
            victimIt != players.end() ? victimIt->second.spawnGeneration : 0;
    }

    for (const auto& kv : players)
    {
        if (kv.second.transport)
            kv.second.transport->send(&event, sizeof(event));
        else
            sendto(sock, (const char*)&event, sizeof(event), 0,
                   (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
        ++totalPacketsOut;
    }

    printf("[PELLET BLAST SERVER RESULT] shooter=%u serial=%u weapon=%s "
           "pelletsGenerated=%d misses=%d worldHits=%d playerHits=%d "
           "targetsHit=%d eventBytes=%zu\n",
           shooter.id, request->shotSerial, weaponId,
           generated, misses, worldHits, playerHits, targetCount,
           sizeof(event));

    int totalDamage = 0;
    for (int i = 0; i < targetCount; ++i) totalDamage += targets[i].totalDamage;
    printf("[PELLET BLAST SERVER DAMAGE] serial=%u total=%d\n",
           request->shotSerial, totalDamage);
}

void handleGodballState(SOCKET sock,
                        std::unordered_map<uint32_t, ServerPlayer>& players,
                        char* buffer, int bytes) {
    if (bytes < (int)sizeof(GodballStatePacket)) return;
    GodballStatePacket* pkt = reinterpret_cast<GodballStatePacket*>(buffer);
    auto it = players.find(pkt->ownerPlayerId);
    if (it == players.end()) return;
    ServerPlayer& p = it->second;
    p.godballX = pkt->posX;
    p.godballY = pkt->posY;
    p.godballZ = pkt->posZ;
    p.godballVx = pkt->velX;
    p.godballVy = pkt->velY;
    p.godballVz = pkt->velZ;
    p.godballActive = pkt->active != 0;
    for (auto& kv : players) {
        if (kv.first == pkt->ownerPlayerId) continue;
        if (kv.second.transport)
            kv.second.transport->send(buffer, bytes);
        else
            sendto(sock, (const char*)buffer, bytes, 0,
                   (sockaddr*)&kv.second.addr, sizeof(kv.second.addr));
    }
}

void handleGodballHitClaim(SOCKET sock,
                           std::unordered_map<uint32_t, ServerPlayer>& players,
                           std::unordered_map<uint32_t, ServerNpc>& npcs,
                           const HeadlessWorld& world,
                           char* buffer, int bytes,
                           uint32_t tick, uint64_t& totalPacketsOut) {
    (void)world;
    if (bytes < (int)sizeof(GodballHitClaimPacket)) return;
    GodballHitClaimPacket* pkt = reinterpret_cast<GodballHitClaimPacket*>(buffer);

    auto attackerIt = players.find(pkt->attackerId);
    if (attackerIt == players.end()) return;
    ServerPlayer& attacker = attackerIt->second;
    if (attacker.dead || attacker.spawnState != ServerPlayer::Active) return;

    const float clampedDamage = std::clamp(pkt->damage, 1.0f, 500.0f);
    const glm::vec3 normal = glm::length(glm::vec3(pkt->normalX, pkt->normalY, pkt->normalZ)) > 0.001f
        ? glm::normalize(glm::vec3(pkt->normalX, pkt->normalY, pkt->normalZ))
        : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 knockback = normal * std::max(1.0f, clampedDamage * 0.75f);
    const glm::vec3 hitPos(pkt->hitX, pkt->hitY, pkt->hitZ);

    // Try player target first
    auto targetIt = players.find(pkt->targetId);
    if (targetIt != players.end()) {
        ServerPlayer& target = targetIt->second;
        if (target.dead || target.spawnState != ServerPlayer::Active) return;
        if (target.spawnGeneration != pkt->spawnGeneration) return;

        ServerDamageResult result = applyServerDamage(
            players, target, pkt->attackerId, (int)std::round(clampedDamage),
            knockback, ServerDamageSource::PhysicalContact);

        if (result.applied) {
            queueServerDamageConfirmedEvent(
                sock, players, tick, totalPacketsOut,
                pkt->attackerId, target, (int)std::round(clampedDamage), result,
                hitPos, normal, knockback, ServerDamageSource::PhysicalContact,
                NETWORK_WEAPON_GODBALL, pkt->contactSerial);
        }
        return;
    }

    // Try NPC target
    auto npcIt = npcs.find(pkt->targetId);
    if (npcIt != npcs.end()) {
        ServerNpc& npc = npcIt->second;
        if (npc.health <= 0) return;

        const int intDamage = std::max(1, (int)std::round(clampedDamage));
        npc.health -= intDamage;
        npc.knockbackImpulse += knockback;
        const bool killed = npc.health <= 0;
        if (killed) npc.health = 0;

        const glm::vec3 origin = attacker.pos;
        const glm::vec3 dir = glm::length(hitPos - origin) > 0.001f
            ? glm::normalize(hitPos - origin) : normal;

        broadcastNpcDamageEvent(
            sock, players, tick, totalPacketsOut,
            pkt->attackerId, npc, intDamage, killed,
            origin, hitPos, dir, normal, NETWORK_WEAPON_GODBALL);

        if (killed) {
            auto attacker2 = players.find(pkt->attackerId);
            if (attacker2 != players.end()) {
                attacker2->second.kills += 1;
                attacker2->second.health = serverMaxHp();
            }
        }

        printf("[SERVER GODBALL NPC HIT] attacker=%u npc=%u damage=%d health=%d killed=%d\n",
               pkt->attackerId, pkt->targetId, intDamage, npc.health, (int)killed);
        return;
    }
}

void handleSpyKnifeHitClaim(SOCKET sock,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            const HeadlessWorld& world,
                            char* buffer, int bytes,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    (void)world;
    if (bytes < (int)sizeof(SpyKnifeHitClaimPacket)) return;
    SpyKnifeHitClaimPacket* pkt = reinterpret_cast<SpyKnifeHitClaimPacket*>(buffer);

    auto attackerIt = players.find(pkt->attackerId);
    if (attackerIt == players.end()) return;
    ServerPlayer& attacker = attackerIt->second;

    if (attacker.dead || attacker.spawnState != ServerPlayer::Active) return;

    bool hasSpyKnife = false;
    for (const std::string& id : attacker.ownedWeaponIds) {
        const WeaponDefinition* d = WeaponRegistry::instance().get(id);
        if (d && d->slot == attacker.equippedSlot && d->behaviorType == WeaponBehaviorType::SpyKnife) {
            hasSpyKnife = true;
            break;
        }
    }
    if (!hasSpyKnife) return;

    auto targetIt = players.find(pkt->targetId);
    if (targetIt == players.end()) return;
    ServerPlayer& target = targetIt->second;
    if (target.dead || target.spawnState != ServerPlayer::Active) return;

    float dist = glm::length(attacker.pos - target.pos);
    if (dist > 3.0f) {
        printf("[SPY KNIFE] REJECTED hit claim: too far attacker=%u target=%u dist=%.1f\n",
               pkt->attackerId, pkt->targetId, dist);
        return;
    }

    const WeaponDefinition* def = nullptr;
    for (const std::string& id : attacker.ownedWeaponIds) {
        const WeaponDefinition* d = WeaponRegistry::instance().get(id);
        if (d && d->slot == attacker.equippedSlot && d->behaviorType == WeaponBehaviorType::SpyKnife) {
            def = d;
            break;
        }
    }
    if (!def) return;

    int damage;
    float kbStrength;
    if (pkt->isBackstab) {
        damage = (int)WeaponExecution::paramOr(*def, "backstabDamagePerTick", 999.0f);
        kbStrength = WeaponExecution::paramOr(*def, "backstabKnockback", 20.0f);
    } else {
        damage = (int)WeaponExecution::paramOr(*def, "normalDamagePerTick", 1.0f);
        kbStrength = WeaponExecution::paramOr(*def, "frontstabKnockback", 100.0f);
    }

    glm::vec3 kbDir = glm::length(target.pos - attacker.pos) > 0.001f
        ? glm::normalize(target.pos - attacker.pos)
        : glm::vec3(0.0f, 0.0f, 1.0f);
    float vertFrac = pkt->isBackstab ? 0.15f
        : WeaponExecution::paramOr(*def, "frontstabVerticalKnockback", 0.3f);
    kbDir.z = std::max(kbDir.z, vertFrac);
    kbDir = glm::normalize(kbDir);
    glm::vec3 knockback = kbDir * kbStrength;

    ServerDamageResult result = applyServerDamage(
        players, target, pkt->attackerId, damage, knockback,
        ServerDamageSource::PhysicalContact);

    if (result.applied) {
        queueServerDamageConfirmedEvent(
            sock, players, tick, totalPacketsOut,
            pkt->attackerId, target, damage, result,
            glm::vec3(pkt->hitX, pkt->hitY, pkt->hitZ),
            kbDir, knockback, ServerDamageSource::PhysicalContact,
            NETWORK_WEAPON_SPYKNIFE, pkt->attackSerial);
    }

    printf("[SPY KNIFE] server applied: attacker=%u target=%u damage=%d backstab=%d dist=%.1f killed=%d\n",
           pkt->attackerId, pkt->targetId, damage, (int)pkt->isBackstab, dist, (int)result.killed);
}

} // namespace MimitaNet
