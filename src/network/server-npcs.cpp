// 07 31 2026, 21 30
/* purpose
* Implements server-side NPC simulation by reusing the real client NpcSystem.
* Online NPCs therefore behave exactly like local ones: full movement physics on
* the map collision, chase/orbit/strafe/retreat AI, aiming, and firing.
* The existing ServerNpc map is kept as the snapshot/broadcast representation and
* is rebuilt from the simulated NPCs every tick, so all existing damage,
* projectile, and snapshot code paths stay unchanged.
* Does NOT render NPCs, spawn hit effects for the host, or own client prediction.
* Does NOT change packet schemas or the player damage pipeline.
*/

#include "network/server.h"
#include "network/actor-lifecycle.h"
#include "hot-reload/hot-reload-system.h"
#include "network/server-gamemode.h"
#include "network/network-weapons.h"
#include "network/server-damage-policy.h"
#include "ecs/actor-entities.h"
#include "live-code/live-gameplay.h"
#include "live-code/live-behavior.h"
#include "hot-reload/generic-runtime.h"
#include "hot-reload/hot-history.h"
#include "hot-reload/hot-npc-targeting.h"
#include "hot-reload/hot-respawn.h"
#include "hot-reload/hot-npc-ground-clamp.h"
#include "ecs/entity-registry.h"
#include "network/actor-health.h"
#include "network/actor-state.h"
#include "network/dynamic-replication.h"
#include "live-code/live-journal.h"

#include "npc/npc.h"
#include "npc/npc-internal.h"
#include "npc/npc-combat.h"
#include "npc/npc-navigation.h"
#include "npc/npc-combat-log.h"
#include "debug/structured-log.h"
#include "npc/npc-avatar.h"
#include "entities/player.h"
#include "world/world.h"
#include "map/map-loader-collision.h"
#include "config/collision-lod-config.h"
#include "config/spawn-velocity-config.h"
#include "physics/movement/physics-collision-shared.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "network/network-weapons.h"
#include "network/packets.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace MimitaNet {

// How long a killed online NPC stays dead before respawning at its spawn
// point. Kept above the 180-tick (3s) fall-over death animation so clients see
// the body freeze, fall over, and disappear before the NPC respawns.
// I WANT IT TO BE CUT OFF like show the death anim AND the alive NPC respawn and moving too
// like just constant action
// constexpr float SERVER_NPC_RESPAWN_SECONDS = 3.5f;
// what if 8 10 2026 we set to be instant so we dont need anim to play before respawn 
// 8 10 2026 keep this its so fun 
constexpr float SERVER_NPC_RESPAWN_SECONDS = 0.01f;

// Resolve the active generation's respawn rule through the one generic doorway.
static const GameRespawnPolicyV1* hotRespawnPolicy()
{
    void* callable = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_RESPAWN);
    if (!callable)
        return nullptr;
    auto lookup = reinterpret_cast<GameRespawnLookupFn>(callable);
    return lookup ? lookup(nullptr) : nullptr;
}

// Initial death timer (shared hot rule with players).
static float npcInitialRespawnTimer()
{
    GameRespawnRuleV1 rule{};
    rule.structSize = sizeof(GameRespawnRuleV1);
    rule.respawnsEnabled = serverMatchRespawnsEnabled() ? 1u : 0u;
    rule.respawnSeconds = serverMatchRespawnSeconds();
    const GameRespawnPolicyV1* policy = hotRespawnPolicy();
    if (policy && policy->initialTimer)
        policy->initialTimer(nullptr, &rule);
    else
        HotRespawnImpl::initialTimer(rule);
    return rule.outRespawnSeconds;
}

// Per-tick countdown; returns true when the NPC is ready to respawn.
static bool npcRespawnTick(float& timer)
{
    GameRespawnRuleV1 rule{};
    rule.structSize = sizeof(GameRespawnRuleV1);
    rule.respawnsEnabled = serverMatchRespawnsEnabled() ? 1u : 0u;
    rule.timer = timer;
    rule.dt = SERVER_DT;
    const GameRespawnPolicyV1* policy = hotRespawnPolicy();
    if (policy && policy->tick)
        policy->tick(nullptr, &rule);
    else
        HotRespawnImpl::tick(rule);
    if (rule.stayDead)
        return false;
    timer = rule.outTimer;
    return rule.readyToRespawn != 0;
}

void broadcastNpcDamageEvent(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    uint32_t tick,
    uint64_t& totalPacketsOut,
    uint32_t shooterPlayerId,
    const ServerNpc& npc,
    int damage,
    bool killed,
    const glm::vec3& origin,
    const glm::vec3& hit,
    const glm::vec3& dir,
    const glm::vec3& normal,
    uint8_t weapon)
{
    NpcDamageEventPacket ev{};
    ev.header.type = PACKET_NPC_DAMAGE_EVENT;
    ev.header.tick = tick;
    ev.header.playerId = shooterPlayerId;
    ev.eventId = nextReliableGameplayEventId();
    ev.eventSessionId = serverReliableEventSessionId();
    ev.npcEntityId = npc.entityId;
    ev.shooterPlayerId = shooterPlayerId;
    ev.damage = damage;
    ev.npcHealth = npc.health;
    ev.killed = killed ? 1 : 0;
    ev.originX = origin.x; ev.originY = origin.y; ev.originZ = origin.z;
    ev.hitX = hit.x; ev.hitY = hit.y; ev.hitZ = hit.z;
    ev.dirX = dir.x; ev.dirY = dir.y; ev.dirZ = dir.z;
    ev.normalX = normal.x; ev.normalY = normal.y; ev.normalZ = normal.z;
    ev.weapon = weapon;
    ev.impactType = SHOT_IMPACT_ENTITY;

    // NPC damage/kill is reliable: it carries the kill-heal and killfeed, so it
    // must not be lost under packet loss. Retransmits until every client ACKs.
    queueReliableGameplayEventToAll(
        sock, players, &ev, sizeof(ev), ev.eventId, ev.eventSessionId,
        totalPacketsOut);

    printf("%s [NPC DAMAGE BROADCAST] shooter=%u npc=%u damage=%d health=%d killed=%d weapon=%u\n",
           serverTimestamp(), shooterPlayerId, npc.entityId, damage, npc.health,
           (int)killed, (unsigned)weapon);
}

void buildNpcWorldCollision(World& npcWorld, const HeadlessWorld& hw)
{
    // The client NpcSystem only reads collision data from the World, so mirror
    // the headless collision world (no GPU/GL work, safe on a headless server).
    // Decimate with the live collision-LOD cell size so NPC collision near dense
    // objects stays cheap, matching the client world.
    npcWorld.collisionMesh.triangles = hw.triangles;
    decimateCollisionTriangleList(npcWorld.collisionMesh.triangles,
                                  CollisionLodConfig::instance().cellSize());
    npcWorld.collisionChunkSize = hw.collisionChunkSize;

    // Build the same acceleration grids the client builds: chunk cells PLUS
    // the coarse large-triangle grid + sub-grids. Reusing the client builder
    // guarantees server NPC collision can never drift apart from the client's.
    // Without the coarse grid, big map pieces (floors/walls spanning > 256
    // chunks, e.g. chainofjudgement's arena floor and walls) were silently
    // dropped by appendChunkTrianglesForAABB, so server NPCs fell through the
    // floor and shot through walls.
    buildCollisionChunks(npcWorld, nullptr);

    printf("[SERVER NPC WORLD] built CPU collision: triangles=%zu chunks=%zu "
           "largeTris=%zu largeChunks=%zu alwaysLarge=%zu\n",
           npcWorld.collisionMesh.triangles.size(), npcWorld.collisionChunks.size(),
           npcWorld.collisionLargeTriangles.size(), npcWorld.collisionLargeChunks.size(),
           npcWorld.collisionAlwaysLargeTriangles.size());
}

// Adopt newly spawned ServerNpc entries (from npc_spawn requests or startup)
// into the real NpcSystem so they get full AI simulation.
static void adoptNewServerNpcs(const std::unordered_map<uint32_t, ServerNpc>& npcs,
                               NpcSystem& npcSystem,
                               std::unordered_set<uint32_t>& npcIdsAlive)
{
    for (const auto& kv : npcs)
    {
        if (npcIdsAlive.find(kv.first) != npcIdsAlive.end())
            continue;
        bool alreadySimulated = false;
        for (const Npc& n : npcSystem.all())
        {
            if (n.id == kv.first) { alreadySimulated = true; break; }
        }
        npcIdsAlive.insert(kv.first);
        if (alreadySimulated) continue;
        npcSystem.spawnNpc(kv.first, kv.second.difficulty, kv.second.pos);
        // Apply the healthall override to the newly adopted real NPC body.
        for (Npc& n : npcSystem.all())
        {
            if (n.id == kv.first)
            {
                if (serverGameOverrides().maxHpOverride > 0)
                {
                    n.body.maxHp = serverGameOverrides().maxHpOverride;
                    n.body.currentHp = n.body.maxHp;
                }
                finalizeServerNpcSpawn(n, ActorSpawnReason::NpcCreate);
            }
        }
    }
}

// Destroy real NPCs whose ServerNpc was removed this tick (killed / void death)
// and echo authoritative health + knockback from the ServerNpc map into the
// real NPC bodies (the map is the damage target for existing server code).
static void syncServerNpcDamageToNpc(const std::unordered_map<uint32_t, ServerNpc>& npcs,
                                     NpcSystem& npcSystem,
                                     std::unordered_set<uint32_t>& npcIdsAlive)
{
    for (auto it = npcIdsAlive.begin(); it != npcIdsAlive.end(); )
    {
        if (npcs.find(*it) == npcs.end())
        {
            npcSystem.destroySelected({*it});
            it = npcIdsAlive.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (const auto& kv : npcs)
    {
        for (Npc& n : npcSystem.all())
        {
            if (n.id != kv.first) continue;
            if (n.body.currentHp != kv.second.health) {
                const int drop = n.body.currentHp - kv.second.health;
                if (drop > 0) {
                    // Attribute the damage to the real NPC's mind (emotion +
                    // attacker memory). Works for player and NPC attackers.
                    npcMindOnDamaged(n, kv.second.lastAttackerId,
                                     kv.second.lastAttackerPos, drop, n.body.maxHp);
                }
                printf("[SERVER SYNC NPC DAMAGE] npcId=%u serverHealth=%d npcBodyHp=%d -> syncing to %d\n",
                       kv.first, kv.second.health, n.body.currentHp, kv.second.health);
                n.body.currentHp = kv.second.health;
            }
            if (glm::length(kv.second.knockbackImpulse) > 0.05f)
                n.body.externalImpulse += kv.second.knockbackImpulse;
            // A killed ServerNpc must stop acting immediately and start its
            // respawn countdown. updateOneNpc early-returns on dead, so the
            // body freezes until respawnServerNpc resets it.
            if (kv.second.health <= 0 && !n.body.dead)
            {
                n.body.currentHp = 0;
                n.body.dead = true;
                n.body.respawnTimer = npcInitialRespawnTimer();
            }
            break;
        }
    }
}

void finalizeServerNpcSpawn(Npc& npc, ActorSpawnReason reason)
{
    ActorSpawnEvent lifecycleEvent;
    lifecycleEvent.entityId = npc.id;
    lifecycleEvent.actorKind = ActorKind::Npc;
    lifecycleEvent.reason = reason;
    lifecycleEvent.transformEpoch = npc.transformEpoch;
    lifecycleEvent.position = npc.body.pos;
    lifecycleEvent.lookDirection = glm::vec3(std::cos(npc.body.yaw),
                                             std::sin(npc.body.yaw), 0.0f);
    lifecycleEvent = finalizeActorSpawn(lifecycleEvent, npc.avatarName.c_str());
    npc.body.yaw = std::atan2(lifecycleEvent.lookDirection.y,
                              lifecycleEvent.lookDirection.x);
    npc.body.vel = lifecycleEvent.velocity;
    npc.body.externalImpulse = glm::vec3(0.0f);
    npc.body.dead = false;
    npc.body.respawnTimer = 0.0f;
    npc.body.syncLegacyStateToLayers();
    npc.body.updateModelWorldTransforms();

    // Generic NPC entity: the ActorHealthState dynamic component is the
    // authoritative health store; the typed HealthComponent is a mirror. Mark
    // the entity for generic lifecycle replication (CREATE) so clients learn it
    // without an NPC-specific spawn packet.
    const EntityId npcEntity =
        Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, npc.id);
    Ecs::setControlSource(npcEntity, ControlSource::ServerNpc);
    Ecs::setAuthority(npcEntity, NetworkAuthority::Server);
    actorHealthInit(Ecs::raw(npcEntity), npc.body.maxHp);
    Ecs::setHealth(npcEntity, npc.body.currentHp, npc.body.maxHp, false);
    serverReplicateEntity(Ecs::raw(npcEntity));

    // Generic NPC tool ownership: every armed actor gets an equipped tool
    // entity carrying its runtime key. The hot action router decides whether a
    // behavior handles the action; if it does, the generic handled gate bypasses
    // the legacy cold fire path. Unknown tools are left to the compatibility
    // fallback. No weapon/type category is known to the kernel.
    {
        const WeaponDefinition* wdef =
            WeaponRegistry::instance().get(npc.body.equippedWeaponId);
        if (wdef) {
            const std::uint8_t netWeapon = networkWeaponTypeForDefinition(*wdef);
            if (netWeapon != NETWORK_WEAPON_NONE) {
                const EntityId toolEntity =
                    EntityRegistry::instance().createGeneric(EntityRealm::Server);
                actorStateEquipTool(Ecs::raw(npcEntity), Ecs::raw(toolEntity),
                                    netWeapon);
            }
        }
    }
}

// Reset a killed server NPC body back to full health at its spawn point so the
// snapshot pipeline re-admits it (rebuildServerNpcMap skips dead bodies).
static void respawnServerNpc(Npc& npc)
{
    glm::vec3 spawnPos = effectiveServerSpawn(npc.body.respawnPosition);
    float spawnYaw = npc.body.yaw;

    LiveEventJournal::Fields respawnEvent;
    respawnEvent.entityId = npc.id;
    respawnEvent.result = "begin";
    respawnEvent.extra = std::string("\"health\":") +
        std::to_string(npc.body.currentHp) + ",\"respawn_timer\":" +
        std::to_string(npc.body.respawnTimer);
    LiveEventJournal::instance().record("server.npc_respawn_begin", respawnEvent);

    // Hot lifecycle policy: the active mode/behavior may relocate the respawn,
    // set the yaw, and arm spawn protection on the actor entity. Generic across
    // players and NPCs; when unhandled (or no hot module) the cold spawn point
    // above is used unchanged.
    {
        ActorLifecyclePolicyV1 lp{};
        lp.playerId = npc.id;
        lp.dead = 1u;
        lp.respawnsEnabled = 1u;
        lp.pendingRespawn = 0u;
        lp.actorEntity = (std::uint64_t)Ecs::ensure(
            EntityRealm::Server, EntityDomain::Npc, npc.id);
        lp.respawnSeconds = 0.0f;
        lp.chosenPosition[0] = spawnPos.x;
        lp.chosenPosition[1] = spawnPos.y;
        lp.chosenPosition[2] = spawnPos.z;
        lp.chosenYaw = spawnYaw;
        if (LiveBehavior::dispatchGameplayEvent64(
                GAME_EVENT_ACTOR_LIFECYCLE_POLICY, &lp, sizeof(lp), 0,
                npc.id, 0) &&
            lp.handled)
        {
            spawnPos = glm::vec3(lp.position[0], lp.position[1], lp.position[2]);
            spawnYaw = lp.yaw;
        }
    }

    // Generic actor lifecycle envelope: the SAME hot lifecycle owner players
    // use. Preserves identity/life generation and arms spawn protection on the
    // NPC actor entity, so NPC and player lifecycle share one policy.
    {
        ActorLifecycleStateV1 lifecycle{};
        lifecycle.entityId = (std::uint64_t)Ecs::ensure(
            EntityRealm::Server, EntityDomain::Npc, npc.id);
        lifecycle.actorKind = 2u;  // npc
        lifecycle.lifeGeneration = npc.transformEpoch;
        lifecycle.reason = 1u;  // respawn
        lifecycle.dead = 0u;
        lifecycle.respawnRequested = 1u;
        lifecycle.position[0] = spawnPos.x;
        lifecycle.position[1] = spawnPos.y;
        lifecycle.position[2] = spawnPos.z;
        lifecycle.yaw = spawnYaw;
        lifecycle.health = npc.body.currentHp;
        lifecycle.maxHealth = npc.body.maxHp;
        LiveBehavior::dispatchActorLifecycle(lifecycle, 0);
    }

    npc.body.pos = spawnPos;
    npc.body.respawnPosition = spawnPos;
    npc.body.yaw = spawnYaw;
    // New life: bump the lifecycle counter so clients detect the respawn.
    npc.transformEpoch = static_cast<uint16_t>((npc.transformEpoch % 65535) + 1);
    assignNpcAvatar(npc);
    // Reapply the actor's role stats/loadout for the new life so role identity
    // is never lost across respawn.
    const ActorSpawnProfile profile = serverResolveActorSpawnProfile(npc.id);
    const int overrideHp = serverGameOverrides().maxHpOverride;
    const int maxHp = overrideHp > 0 ? overrideHp
        : (profile.health > 0 ? profile.health : npc.body.maxHp);
    npc.body.maxHp = maxHp;
    npc.body.currentHp = maxHp;
    npc.movementProfileId = profile.movementPreset;
    npc.navigator.reset();
    npc.traversal.reset();
    npc.prevHadTarget = false;
    npc.reactionTimer = 0.0f;
    npc.serverTargetId = 0;
    // Reapply the role behavior profile for the new life.
    npc.behaviorProfileId = profile.behaviorProfileId;
    npc.behavior = resolveNpcBehavior(profile.behaviorProfileId);
    if (npc.behavior.active && npc.behavior.aggression >= 0.0f)
        npc.tuning.aggression = npc.behavior.aggression;
    npcMindReset(npc);
    if (!profile.weapons.empty())
        npcApplyLoadout(npc, profile.weapons, profile.startingWeapon);
    npc.body.killedBy.clear();
    npc.body.spawnFlashTimer = 10.0f;
    npc.attackCooldown = 0.0f;
    resetAllWeaponRuntimesForSpawn(npc.body, "server-npc-respawn");
    finalizeServerNpcSpawn(npc, ActorSpawnReason::Respawn);
    printf("%s [SERVER NPC RESPAWN] id=%u pos=(%.2f,%.2f,%.2f)\n",
           serverTimestamp(), npc.id, spawnPos.x, spawnPos.y, spawnPos.z);
}

// Broadcast the visual/sound of an NPC's weapon firing to every client so the
// shot (muzzle flash, tracer, sound, weapon trigger) shows up remotely.
static void broadcastNpcFiring(SOCKET sock,
                               std::unordered_map<uint32_t, ServerPlayer>& players,
                               NpcSystem& npcSystem,
                               std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                               uint32_t& nextProjectileId,
                               uint32_t tick,
                               uint64_t& totalPacketsOut)
{
    for (Npc& n : npcSystem.all())
    {
        if (n.body.dead || n.body.currentHp <= 0) continue;
        // Broadcast only on the tick the NPC actually fired so clients get
        // exactly one shot/sound/tracer per bullet (no per-frame spam).
        if (!n.justFired) continue;
        n.justFired = false;
        const WeaponDefinition* wdef = WeaponRegistry::instance().get(n.body.equippedWeaponId);
        if (!wdef) continue;
        const uint8_t netWeapon = networkWeaponTypeForDefinition(*wdef);
        if (netWeapon == NETWORK_WEAPON_NONE) continue;

        glm::vec3 origin = n.body.pos + NpcCombat::npcMuzzleOffset();
        glm::vec3 dir;
        glm::vec3 hit;
        if (n.hasLastShot && glm::length(n.lastShotEnd - n.lastShotOrigin) > 0.001f)
        {
            // Broadcast the real fired shot so the remote tracer goes exactly
            // where the damage ray went (look == shoot == bullet endpoint).
            origin = n.lastShotOrigin;
            hit = n.lastShotEnd;
            dir = glm::normalize(hit - origin);
        }
        else
        {
            dir = glm::length(n.currentFacing) > 0.001f
                ? glm::normalize(n.currentFacing)
                : glm::vec3(1.0f, 0.0f, 0.0f);
            hit = origin + dir * 100.0f;
        }

        // NPCs use the same generic tool/action seam as players. The hot tool
        // behavior creates the canonical entity and projectiles.60 owns it.
        if (networkWeaponTypeIsProjectile(netWeapon))
        {
            const EntityId ownerEntity =
                Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, n.id);
            Ecs::setControlSource(ownerEntity, ControlSource::ServerNpc);
            Ecs::setAuthority(ownerEntity, NetworkAuthority::Server);
            ToolUsePolicyV1 use{};
            use.userEntity = Ecs::raw(ownerEntity);
            use.toolId = netWeapon;
            use.ownerId = n.id;
            use.toolNetworkId = netWeapon;
            use.tick = tick;
            use.baseFire = 1;
            use.outFire = 1;
            use.origin[0] = origin.x; use.origin[1] = origin.y; use.origin[2] = origin.z;
            use.direction[0] = dir.x; use.direction[1] = dir.y; use.direction[2] = dir.z;
            LiveBehavior::dispatchToolUse(use, tick);

            DBG(NpcCombat, "npc=%u weapon=%s projectileId=%u "
                "position=(%.2f,%.2f,%.2f) velocity=(%.2f,%.2f,%.2f)",
                n.id, wdef->id.c_str(), 0u, origin.x, origin.y, origin.z,
                dir.x, dir.y, dir.z);
        }

        // For multi-pellet weapons (shotgun), broadcast a PelletBlastEventPacket
        // so remote clients see all 15 pellet tracers + hear the sound once.
        // Skip the ShotEventPacket for these — the PelletBlastEventPacket handles
        // sound, muzzle flash, and per-pellet rendering.
        if (n.pelletResultCount > 1)
        {
            PelletBlastEventPacket blast{};
            blast.header.type = PACKET_PELLET_BLAST_EVENT;
            blast.header.tick = tick;
            blast.header.playerId = n.id;
            blast.shooterPlayerId = n.id;
            blast.weapon = netWeapon;
            blast.pelletCount = (uint8_t)n.pelletResultCount;
            blast.spreadSeed = n.pelletSpreadSeed;
            blast.shotSerial = n.shotSerialCounter++;
            blast.lastServerTick = tick;
            blast.originX = origin.x;
            blast.originY = origin.y;
            blast.originZ = origin.z;
            blast.baseDirX = dir.x;
            blast.baseDirY = dir.y;
            blast.baseDirZ = dir.z;
            blast.maxRange = 100.0f;
            blast.targetCount = 0;

            int pelletsToSend = std::min(n.pelletResultCount, (int)MAX_NETWORK_PELLETS);
            for (int p = 0; p < pelletsToSend; ++p)
            {
                NetworkPelletResult& pr = blast.pellets[p];
                pr.hitX = n.pelletResults[p].hitPos.x;
                pr.hitY = n.pelletResults[p].hitPos.y;
                pr.hitZ = n.pelletResults[p].hitPos.z;
                pr.normalX = n.pelletResults[p].hitNormal.x;
                pr.normalY = n.pelletResults[p].hitNormal.y;
                pr.normalZ = n.pelletResults[p].hitNormal.z;
                pr.impactType = n.pelletResults[p].hitEntity ? PELLET_IMPACT_PLAYER :
                    (n.pelletResults[p].hitWorld ? PELLET_IMPACT_WORLD : PELLET_IMPACT_NONE);
                pr.pelletIndex = (uint8_t)p;
            }

            for (const auto& pe : players)
            {
                if (pe.second.transport)
                    pe.second.transport->send(&blast, sizeof(blast));
                else
                    sendto(sock, (const char*)&blast, sizeof(blast), 0,
                           (sockaddr*)&pe.second.addr,
                           sizeof(pe.second.addr));
                ++totalPacketsOut;
            }

            n.pelletResultCount = 0; // consumed

            printf("%s [NPC PELLET BLAST] npc=%u weapon=%s pellets=%d\n",
                   serverTimestamp(), n.id, wdef->id.c_str(), pelletsToSend);
        }
        else
        {
        // Broadcast the ShotEventPacket (sound + muzzle flash on clients).
        // For projectile weapons (rocket, grenade), skip the tracer — the
        // ProjectileSpawnEventPacket renders the actual projectile instead.
        ShotEventPacket ev{};
        ev.header.type = PACKET_SHOT_EVENT;
        ev.header.tick = tick;
        ev.header.playerId = n.id;
        ev.shotSerial = 0;
        ev.clientTimeMs = 0;
        ev.shooterPlayerId = n.id;
        ev.targetPlayerId = 0;
        ev.weapon = netWeapon;
        uint16_t effectFlags = SHOT_EFFECT_MUZZLE |
            SHOT_EFFECT_SHOOT_SOUND | SHOT_EFFECT_WEAPON_TRIGGER;
        if (!networkWeaponTypeIsProjectile(netWeapon))
            effectFlags |= SHOT_EFFECT_TRACER;
        ev.impactType = SHOT_IMPACT_NONE;
        if (n.lastShotHitWorld) {
            ev.impactType = SHOT_IMPACT_WORLD;
            effectFlags |= SHOT_EFFECT_WORLD_IMPACT | SHOT_EFFECT_DEBRIS | SHOT_EFFECT_HIT_SOUND;
        }
        ev.effectFlags = effectFlags;
        ev.originX = origin.x; ev.originY = origin.y; ev.originZ = origin.z;
        ev.hitX = hit.x; ev.hitY = hit.y; ev.hitZ = hit.z;
        ev.dirX = dir.x; ev.dirY = dir.y; ev.dirZ = dir.z;
        ev.normalX = n.lastShotNormal.x;
        ev.normalY = n.lastShotNormal.y;
        ev.normalZ = n.lastShotNormal.z;

        for (const auto& pe : players)
        {
            if (pe.second.transport)
                pe.second.transport->send(&ev, sizeof(ev));
            else
                sendto(sock, (const char*)&ev, sizeof(ev), 0,
                       (sockaddr*)&pe.second.addr,
                       sizeof(pe.second.addr));
            ++totalPacketsOut;
        }

        printf("%s [NPC FIRED] npc=%u weapon=%s dir=(%.2f %.2f %.2f)\n",
               serverTimestamp(), n.id, wdef->id.c_str(), dir.x, dir.y, dir.z);
        } // end else (single-shot weapons)
    }
}

// Rebuild the snapshot/broadcast ServerNpc map from the simulated NPCs.
// History is preserved across the rebuild so hit-rewind validation keeps the
// per-tick poses the clients actually saw (the map is cleared and repopulated
// every tick, which would otherwise discard all historical samples).
static void rebuildServerNpcMap(std::unordered_map<uint32_t, ServerNpc>& npcs,
                                NpcSystem& npcSystem,
                                std::unordered_set<uint32_t>& npcIdsAlive)
{
    std::unordered_map<uint32_t, ServerNpc> next;
    next.reserve(npcs.size());
    npcIdsAlive.clear();
    for (Npc& n : npcSystem.all())
    {
        // Dead NPCs stay in the broadcast (health 0) so clients render their
        // full fall-over death animation before the NPC respawns. Alive NPCs
        // and freshly-respawned ones are broadcast as before.
        ServerNpc sn;
        sn.entityId = n.id;
        sn.transformEpoch = n.transformEpoch;
        sn.name = n.body.username.empty()
            ? "NPC " + std::to_string(n.id)
            : n.body.username;
        sn.avatarName = n.avatarName;
        sn.pos = n.body.pos;
        sn.vel = n.body.vel;
        sn.aim = (glm::length(n.currentFacing) > 0.001f)
            ? glm::normalize(n.currentFacing)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        sn.yaw = n.body.yaw;
        // Health projection: the generic ActorHealthState component is the
        // authority; ServerNpc.health is a bridge mirrored from it.
        {
            const EntityId npcEntity =
                Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, n.id);
            std::int32_t componentHealth = 0;
            if (actorHealthRead(Ecs::raw(npcEntity), &componentHealth, nullptr, nullptr))
                sn.health = componentHealth;
            else
                sn.health = n.body.currentHp;
            // Transform/velocity projection onto the generic entity so hot
            // gameplay systems can read it. Networking remains snapshot-based.
            Ecs::setTransform(npcEntity, n.body.pos, sn.aim, n.body.yaw, 0.0f);
            Ecs::setVelocity(npcEntity, n.body.vel, n.body.externalImpulse);
        }
        sn.onGround = n.body.ground.hasWorldContact;
        sn.difficulty = n.difficulty;
        sn.equippedSlot = n.body.equippedSlot;
        sn.weaponState = 0;
        {
            const auto& npcRt = n.body.weaponRuntimes.find(n.body.equippedWeaponId);
            if (npcRt != n.body.weaponRuntimes.end())
            {
                const WeaponRuntime& rt = npcRt->second;
                if (rt.shootEffectTimer > 0.0f)
                    sn.weaponState |= NET_WEAPON_STATE_FIRING;
                if (rt.isReloading)
                    sn.weaponState |= NET_WEAPON_STATE_RELOADING;
                if (rt.currentAmmo <= 0)
                    sn.weaponState |= NET_WEAPON_STATE_EMPTY;
            }
        }
        auto oldIt = npcs.find(sn.entityId);
        if (oldIt != npcs.end())
            sn.posHistory = std::move(oldIt->second.posHistory);
        next[sn.entityId] = std::move(sn);
        npcIdsAlive.insert(sn.entityId);
    }
    npcs = std::move(next);
}

// Record the position actually broadcast to clients this tick so hit-rewind
// reads exactly what attackers saw (same contract as ServerPlayer::posHistory).
void pushNpcPositionHistory(ServerNpc& npc, uint32_t tick)
{
    ServerNpcPositionSample sample;
    sample.pos = npc.pos;
    sample.vel = npc.vel;
    sample.yaw = npc.yaw;
    sample.tick = tick;
    sample.logicalGenerationId =
        HotReloadSystem::instance().status().activeGeneration;
    sample.partCount = npc.bodyPartCount;
    for (uint8_t i = 0; i < npc.bodyPartCount && i < sample.parts.size(); ++i)
        sample.parts[i] = npc.bodyParts[i];
    npc.posHistory.push_back(sample);
    while (npc.posHistory.size() > ServerNpc::MAX_POS_HISTORY)
        npc.posHistory.pop_front();
}

// Body-part AABBs the NPC rendered at a past broadcast tick, for the server's
// authoritative re-trace to validate against what the attacker actually saw.
bool getNpcBodyPartsAtTick(const ServerNpc& npc, uint32_t targetTick,
                           const ServerNpcBodyPartSample** outParts,
                           uint8_t* outPartCount)
{
    if (npc.posHistory.empty())
        return false;
    const ServerNpcPositionSample* sample = &npc.posHistory.back();
    if (targetTick <= npc.posHistory.front().tick)
        sample = &npc.posHistory.front();
    else
    {
        for (auto it = npc.posHistory.rbegin(); it != npc.posHistory.rend(); ++it)
        {
            if (it->tick <= targetTick)
            {
                sample = &(*it);
                break;
            }
        }
    }
    if (sample->partCount == 0)
        return false;
    *outParts = sample->parts.data();
    *outPartCount = sample->partCount;
    return true;
}

const std::vector<ServerPlayerBodyPartTemplate>* standardPlayerBodyTemplate()
{
    return gServerBodyTemplate.empty() ? nullptr : &gServerBodyTemplate;
}

// Interpolate the NPC's broadcast pose for a past server tick.
bool getNpcPositionAtTick(const ServerNpc& npc, uint32_t targetTick, glm::vec3& outPos)
{
    if (npc.posHistory.empty())
        return false;
    if (targetTick >= npc.posHistory.back().tick)
    {
        outPos = npc.posHistory.back().pos;
        return true;
    }
    if (targetTick <= npc.posHistory.front().tick)
    {
        outPos = npc.posHistory.front().pos;
        return true;
    }
    for (int i = (int)npc.posHistory.size() - 1; i > 0; --i)
    {
        if (npc.posHistory[i].tick == targetTick)
        {
            outPos = npc.posHistory[i].pos;
            return true;
        }
        if (npc.posHistory[i].tick < targetTick)
        {
            const auto& a = npc.posHistory[i];
            const auto& b = npc.posHistory[i + 1];
            // Generation boundary: no cross-generation interpolation.
            if (a.logicalGenerationId != b.logicalGenerationId)
            {
                outPos = b.pos;
                return true;
            }
            const float frac = float(targetTick - a.tick) / float(b.tick - a.tick);
            outPos = glm::mix(a.pos, b.pos, frac);
            return true;
        }
    }
    outPos = npc.posHistory.front().pos;
    return true;
}

// Like getNpcPositionAtTick but also returns the broadcast yaw at that tick so
// Fill the bracketing samples for an NPC history. Returns false when empty.
static bool bracketNpcHistory(const ServerNpc& npc, uint32_t targetTick,
                              MimitaNet::GameHistorySelectV1& out)
{
    out = MimitaNet::GameHistorySelectV1{};
    out.targetTick = targetTick;
    if (npc.posHistory.empty())
        return false;
    auto toSample = [](MimitaNet::GameHistorySampleV1& s,
                       const ServerNpcPositionSample& e) {
        s.tick = e.tick;
        s.logicalGenerationId = e.logicalGenerationId;
        s.position[0] = e.pos.x;
        s.position[1] = e.pos.y;
        s.position[2] = e.pos.z;
        s.velocity[0] = e.vel.x;
        s.velocity[1] = e.vel.y;
        s.velocity[2] = e.vel.z;
        s.yaw = e.yaw;
    };
    const auto& back = npc.posHistory.back();
    const auto& front = npc.posHistory.front();
    if (targetTick >= back.tick) {
        toSample(out.a, back);
        out.haveA = 1;
        return true;
    }
    if (targetTick <= front.tick) {
        toSample(out.a, front);
        out.haveA = 1;
        return true;
    }
    int lo = 0;
    int hi = (int)npc.posHistory.size() - 1;
    while (lo < hi - 1) {
        int mid = (lo + hi) / 2;
        if (npc.posHistory[mid].tick <= targetTick)
            lo = mid;
        else
            hi = mid;
    }
    toSample(out.a, npc.posHistory[lo]);
    out.haveA = 1;
    if (npc.posHistory[lo].tick == targetTick) {
        out.exactTick = 1;
        return true;
    }
    toSample(out.b, npc.posHistory[lo + 1]);
    out.haveB = 1;
    return true;
}

// NPC body-part hitboxes are reconstructed with the facing the attacker saw.
bool getNpcPoseAtTick(const ServerNpc& npc, uint32_t targetTick,
                      glm::vec3& outPos, float& outYaw)
{
    // Hot lag-comp policy first; cold fallback below.
    {
        MimitaNet::GameHistorySelectV1 select{};
        if (bracketNpcHistory(npc, targetTick, select) &&
            LiveBehavior::dispatchGameplayEvent64(
                MimitaNet::GAME_EVENT_HISTORY_SELECT, &select, sizeof(select),
                targetTick, 0, 0) &&
            select.handled)
        {
            outPos = glm::vec3(select.position[0], select.position[1],
                               select.position[2]);
            outYaw = select.yaw;
            return true;
        }
    }
    if (npc.posHistory.empty())
        return false;
    const auto& back = npc.posHistory.back();
    if (targetTick >= back.tick)
    {
        outPos = back.pos;
        outYaw = back.yaw;
        return true;
    }
    const auto& front = npc.posHistory.front();
    if (targetTick <= front.tick)
    {
        outPos = front.pos;
        outYaw = front.yaw;
        return true;
    }
    int lo = 0;
    int hi = (int)npc.posHistory.size() - 1;
    while (lo < hi - 1)
    {
        int mid = (lo + hi) / 2;
        if (npc.posHistory[mid].tick <= targetTick)
            lo = mid;
        else
            hi = mid;
    }
    const auto& a = npc.posHistory[lo];
    const auto& b = npc.posHistory[lo + 1];
    // Generation boundary: clamp to the newer authoritative sample (matches
    // player rewind) instead of interpolating across F/G.
    if (a.logicalGenerationId != b.logicalGenerationId)
    {
        outPos = b.pos;
        outYaw = b.yaw;
        return true;
    }
    const float frac = (b.tick > a.tick)
        ? float(targetTick - a.tick) / float(b.tick - a.tick)
        : 0.0f;
    outPos = glm::mix(a.pos, b.pos, frac);
    outYaw = a.yaw + (b.yaw - a.yaw) * frac;
    return true;
}

// Resolve the active generation's NPC targeting policy through the one generic
// doorway. Never cached across a generation swap. Null when no hot provider.
static const GameNpcTargetingPolicyV1* hotNpcTargetingPolicy()
{
    void* callable = MimitaRuntime::GenericRuntime::instance().capability(
        GAME_CAP_NPC_TARGETING);
    if (!callable)
        return nullptr;
    auto lookup = reinterpret_cast<GameNpcTargetingLookupFn>(callable);
    return lookup ? lookup(nullptr) : nullptr;
}

void simulateSharedNpcs(SOCKET sock,
                        std::unordered_map<uint32_t, ServerPlayer>& players,
                        std::unordered_map<uint32_t, ServerNpc>& npcs,
                        NpcSystem& npcSystem,
                        World& world,
                        Player& mirrorPlayer,
                        std::unordered_set<uint32_t>& npcIdsAlive,
                        std::unordered_map<uint32_t, ServerProjectile>& projectiles,
                        uint32_t& nextProjectileId,
                        uint32_t tick,
                        uint64_t& totalPacketsOut)
{
    adoptNewServerNpcs(npcs, npcSystem, npcIdsAlive);
    syncServerNpcDamageToNpc(npcs, npcSystem, npcIdsAlive);

    // Each NPC targets its own nearest HOSTILE actor: live players and other
    // NPCs. Unteamed modes (FFA/sandbox) make everyone hostile; TDM only enemies.
    // Damage stays server-authoritative: an NPC's damage to its mirror is routed
    // onto the real target, so NPCs behave exactly like players.
    // Hostility rule is hot (net.npc-targeting).
    auto actorsAreHostile = [](int a, int b) {
        GameNpcHostilityV1 request{};
        request.structSize = sizeof(GameNpcHostilityV1);
        request.teamA = a;
        request.teamB = b;
        const GameNpcTargetingPolicyV1* policy = hotNpcTargetingPolicy();
        if (policy && policy->hostile)
            policy->hostile(nullptr, &request);
        else
            HotNpcTargetingImpl::hostile(request);
        return request.hostile != 0;
    };
    auto npcTeamOf = [&](const Npc& npc) -> int {
        auto it = npcs.find(npc.id);
        return it != npcs.end() ? it->second.matchTeam : npc.body.matchTeam;
    };

    for (Npc& n : npcSystem.all())
    {
        if (n.body.dead || n.body.currentHp <= 0)
            continue;

        const uint32_t prevTarget = n.serverTargetId;
        const int myTeam = npcTeamOf(n);
        ServerPlayer* nearestPlayer = nullptr;
        Npc* nearestNpc = nullptr;

        if (n.behavior.active)
        {
            // Scored target selection. Weights come only from the behavior
            // profile; the legacy nearest-hostile path is used without one.
            const NpcBehaviorTuning& b = n.behavior;
            // Candidate score is hot (net.npc-targeting); the cold caller adds
            // the current-target stickiness.
            auto scoreCandidate = [&](const glm::vec3& pos, int hp, int maxHp,
                                      float threat01) {
                GameNpcTargetScoreV1 request{};
                request.structSize = sizeof(GameNpcTargetScoreV1);
                for (int i = 0; i < 3; ++i) {
                    request.selfPos[i] = n.body.pos[i];
                    request.candidatePos[i] = pos[i];
                }
                request.candidateHp = hp;
                request.candidateMaxHp = maxHp;
                request.threat01 = threat01;
                request.isCurrent = 0u;
                request.distanceTargetBias = b.distanceTargetBias;
                request.lowHealthTargetBias = b.lowHealthTargetBias;
                request.threatBias = b.threatBias;
                request.stickiness = 0.0f;
                const GameNpcTargetingPolicyV1* policy = hotNpcTargetingPolicy();
                if (policy && policy->score)
                    policy->score(nullptr, &request);
                else
                    HotNpcTargetingImpl::score(request);
                return request.score;
            };

            float bestScore = -1e30f;
            float currentScore = -1e30f;
            for (auto& kv : players)
            {
                ServerPlayer& p = kv.second;
                if (p.dead || p.connectionStale) continue;
                if (!actorsAreHostile(myTeam, p.matchTeam)) continue;
                float s = scoreCandidate(p.pos, p.health, std::max(1, p.maxHealth), 0.5f);
                const bool isCurrent = (p.id == n.serverTargetId);
                if (isCurrent) s += npcMindStickiness(n);
                if (s > bestScore) { bestScore = s; nearestPlayer = &p; nearestNpc = nullptr; }
                if (isCurrent) currentScore = s;
            }
            for (Npc& other : npcSystem.all())
            {
                if (&other == &n) continue;
                if (other.body.dead || other.body.currentHp <= 0) continue;
                if (!actorsAreHostile(myTeam, npcTeamOf(other))) continue;
                float threat01 = 0.0f;
                if (const WeaponDefinition* wd =
                        WeaponRegistry::instance().get(other.body.equippedWeaponId))
                    threat01 = glm::clamp(wd->damage / 50.0f, 0.0f, 1.0f);
                float s = scoreCandidate(other.body.pos, other.body.currentHp,
                                         other.body.maxHp, threat01);
                const bool isCurrent = (other.id == n.serverTargetId);
                if (isCurrent) s += npcMindStickiness(n);
                if (s > bestScore) { bestScore = s; nearestNpc = &other; nearestPlayer = nullptr; }
                if (isCurrent) currentScore = s;
            }

            // Switch only when a new candidate beats the current target by the
            // configured threshold (reduces target thrashing).
            if ((nearestPlayer || nearestNpc) && n.serverTargetId != 0 &&
                currentScore > -1e29f)
            {
                const uint32_t bestId = nearestPlayer ? nearestPlayer->id : nearestNpc->id;
                if (bestId != n.serverTargetId &&
                    bestScore <= currentScore + b.targetSwitchThreshold)
                {
                    bool kept = false;
                    for (auto& kv : players)
                    {
                        if (kv.second.id == n.serverTargetId && !kv.second.dead)
                        { nearestPlayer = &kv.second; nearestNpc = nullptr; kept = true; break; }
                    }
                    if (!kept)
                        for (Npc& other : npcSystem.all())
                            if (other.id == n.serverTargetId && !other.body.dead &&
                                other.body.currentHp > 0)
                            { nearestNpc = &other; nearestPlayer = nullptr; kept = true; break; }
                }
            }
        }
        else
        {
            // Legacy nearest-hostile (unchanged when no behavior profile applies).
            float bestD2 = std::numeric_limits<float>::max();
            for (auto& kv : players)
            {
                ServerPlayer& p = kv.second;
                if (p.dead || p.connectionStale) continue;
                if (!actorsAreHostile(myTeam, p.matchTeam)) continue;
                const glm::vec3 d = p.pos - n.body.pos;
                const float d2 = glm::dot(d, d);
                if (d2 < bestD2) { bestD2 = d2; nearestPlayer = &p; nearestNpc = nullptr; }
            }
            for (Npc& other : npcSystem.all())
            {
                if (&other == &n) continue;
                if (other.body.dead || other.body.currentHp <= 0) continue;
                if (!actorsAreHostile(myTeam, npcTeamOf(other))) continue;
                const glm::vec3 d = other.body.pos - n.body.pos;
                const float d2 = glm::dot(d, d);
                if (d2 < bestD2) { bestD2 = d2; nearestNpc = &other; nearestPlayer = nullptr; }
            }
        }

        // Generic target authority: the hot gameplay.60 AI writes
        // relationship.targets; when present it wins over the cold
        // nearest-enemy fallback here. The typed serverTargetId remains a
        // projection so the cold combat path follows the hot-chosen target.
        {
            const EntityId npcEntity =
                Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, n.id);
            std::uint64_t chosen = 0;
            if (actorStateGetTarget(Ecs::raw(npcEntity), &chosen) && chosen != 0) {
                const EntityId te = static_cast<EntityId>(chosen);
                if (entityDomain(te) == EntityDomain::Player) {
                    auto it = players.find(entityLegacyId(te));
                    if (it != players.end() && !it->second.dead) {
                        nearestPlayer = &it->second;
                        nearestNpc = nullptr;
                    }
                } else if (entityDomain(te) == EntityDomain::Npc) {
                    for (Npc& candidate : npcSystem.all()) {
                        if (candidate.id != entityLegacyId(te))
                            continue;
                        if (!candidate.body.dead && candidate.body.currentHp > 0) {
                            nearestNpc = &candidate;
                            nearestPlayer = nullptr;
                        }
                        break;
                    }
                }
            }
        }
        n.serverTargetId = nearestPlayer ? nearestPlayer->id
                        : (nearestNpc ? nearestNpc->id : 0);
        if (n.serverTargetId != prevTarget)
        {
            float targetDist = 0.0f;
            if (nearestPlayer) targetDist = glm::length(nearestPlayer->pos - n.body.pos);
            else if (nearestNpc) targetDist = glm::length(nearestNpc->body.pos - n.body.pos);
            npcLog("npc-target npc=%u profile=%s target=%u dist=%.1f tick=%u",
                   n.id,
                   n.behaviorProfileId.empty() ? "default" : n.behaviorProfileId.c_str(),
                   n.serverTargetId, targetDist, tick);
        }

        if (nearestPlayer)
        {
            mirrorPlayer.pos = nearestPlayer->pos;
            mirrorPlayer.vel = nearestPlayer->vel;
            mirrorPlayer.yaw = nearestPlayer->yaw;
            mirrorPlayer.currentHp = nearestPlayer->health;
            mirrorPlayer.dead = nearestPlayer->dead;
            mirrorPlayer.username = nearestPlayer->name;
            mirrorPlayer.matchTeam = nearestPlayer->matchTeam;
        }
        else if (nearestNpc)
        {
            const int targetHp = (npcs.find(nearestNpc->id) != npcs.end())
                ? npcs.find(nearestNpc->id)->second.health
                : nearestNpc->body.currentHp;
            mirrorPlayer.pos = nearestNpc->body.pos;
            mirrorPlayer.vel = nearestNpc->body.vel;
            mirrorPlayer.yaw = nearestNpc->body.yaw;
            mirrorPlayer.currentHp = targetHp;
            mirrorPlayer.dead = nearestNpc->body.dead;
            mirrorPlayer.username = nearestNpc->body.username;
            mirrorPlayer.matchTeam = npcTeamOf(*nearestNpc);
        }
        else
        {
            // No live hostile target: still advance the NPC's physics so it
            // falls from its spawn and stands on the floor instead of hovering
            // frozen in the air. A dead mirror means no targeting, no combat,
            // and no damage.
            mirrorPlayer.pos = n.body.pos;
            mirrorPlayer.vel = glm::vec3(0.0f);
            mirrorPlayer.yaw = n.body.yaw;
            mirrorPlayer.currentHp = 0;
            mirrorPlayer.dead = true;
        }
        // Clear the mirror's impulse so it only carries the knockback applied
        // THIS tick by processPlayerHit (same knockback a player's shot gives).
        mirrorPlayer.externalImpulse = glm::vec3(0.0f);
        const int hpBefore = mirrorPlayer.currentHp;

        npcSystem.updateOneWithTarget(n.id, world, mirrorPlayer, SERVER_DT);

        // Ground clamp: the decimated headless collision world can miss the
        // floor, so a server NPC that ends up below the floor gets pinned back
        // onto the nearest floor triangle and marked grounded — it never sinks
        // through the ground forever. Legitimate airborne/jumping NPCs above the
        // floor are untouched; the clamp only corrects downward violations. The
        // rest height and clamp decision are hot (net.npc-ground-clamp).
        {
            const float floorZ = NpcNavigation::groundHeightAt(
                world, n.body.pos, 100.0f, 5.0f);
            GameNpcGroundClampV1 clamp{};
            clamp.structSize = sizeof(GameNpcGroundClampV1);
            clamp.haveFloor = floorZ > -1e5f ? 1u : 0u;
            clamp.floorZ = floorZ;
            clamp.posZ = n.body.pos.z;
            clamp.velZ = n.body.vel.z;
            clamp.restHeight = 1.8f; // capsule half-height (feet at pos.z - 1.8)
            auto clampPolicy = reinterpret_cast<GameNpcGroundClampFn>(
                MimitaRuntime::GenericRuntime::instance().capability(
                    GAME_CAP_NPC_GROUND_CLAMP));
            if (clampPolicy)
                clampPolicy(nullptr, &clamp);
            else
                HotNpcGroundClampImpl::evaluate(clamp);
            if (clamp.clamp)
            {
                n.body.pos.z = clamp.outPosZ;
                n.body.vel.z = clamp.outVelZ;
                n.body.ground.hasWorldContact = true;
                n.body.ground.stableOnGround = true;
                n.body.ground.onGround = true;
                n.body.ground.realWorldContactThisFrame = true;
            }
        }

        if ((nearestPlayer || nearestNpc) && hpBefore > mirrorPlayer.currentHp)
        {
            const int damage = hpBefore - mirrorPlayer.currentHp;
            // Forward the exact knockback processPlayerHit applied to the mirror
            // so NPC shots push the victim like a player's shot (also fixes the
            // client HP bar, which only applies confirmed HP when knockback exists).
            glm::vec3 knockback = mirrorPlayer.externalImpulse;
            int resolvedDamage = damage;
            const glm::vec3 realHit = n.lastShotEnd;
            const glm::vec3 realNormal = glm::length(n.lastShotNormal) > 0.001f
                ? glm::normalize(n.lastShotNormal) : glm::vec3(0.0f, 0.0f, 1.0f);
            uint8_t hitWeapon = NETWORK_WEAPON_NONE;
            if (const WeaponDefinition* nwdef = WeaponRegistry::instance().get(n.body.equippedWeaponId))
                hitWeapon = networkWeaponTypeForDefinition(*nwdef);
            const char* wId = networkWeaponTypeName(hitWeapon);
            std::string wDisp = wId;
            if (const WeaponDefinition* wd = WeaponRegistry::instance().get(wId))
                if (!wd->displayName.empty()) wDisp = wd->displayName;

            if (nearestPlayer)
            {
                ServerDamagePolicyInput policyInput{};
                policyInput.source = GAME_DAMAGE_SOURCE_HITSCAN;
                policyInput.attackerEntity = Ecs::raw(
                    Ecs::ensure(EntityRealm::Server, EntityDomain::Npc, n.id));
                policyInput.victimEntity = Ecs::raw(
                    Ecs::ensure(EntityRealm::Server, EntityDomain::Player, nearestPlayer->id));
                policyInput.weaponNetworkId = weaponDefNetworkIdFor(n.body.equippedWeaponId);
                policyInput.victimIsNpc = 0;
                policyInput.tick = tick;
                resolvedDamage = serverResolveDamagePolicy(policyInput, damage, knockback);

                ServerDamageResult result = applyServerDamage(
                    players, *nearestPlayer, 0, resolvedDamage, knockback,
                    ServerDamageSource::Hitscan);
                // Track NPC damage for kill attribution: if this NPC's damage
                // brings the player to 0 HP on the next tick (or the player dies
                // from self-damage shortly after), attribute the kill to this NPC.
                nearestPlayer->lastNpcDamageSourceId = n.id;
                nearestPlayer->lastNpcDamageTick = tick;
                queueServerDamageConfirmedEvent(
                    sock, players, tick, totalPacketsOut, 0, *nearestPlayer, resolvedDamage, result,
                    realHit, realNormal, knockback,
                    ServerDamageSource::Hitscan, hitWeapon, 0, 0, n.id,
                    n.body.equippedWeaponId);
                npcLog("npc=%u weapon=%s damage=%d healthBefore=%d healthAfter=%d "
                       "accepted=%d knockback=(%.2f %.2f %.2f)",
                       n.id, n.body.equippedWeaponId.c_str(), damage, result.healthBefore,
                       result.healthAfter, (int)result.applied,
                       knockback.x, knockback.y, knockback.z);
                const ServerGamemodeState& gms = serverGamemodeState();
                DBG(Network,
                    "SERVER_NPC_KILLS_PLAYER proc=server npcId=%u npcName=\"%s\" "
                    "playerId=%u playerName=\"%s\" weaponId=\"%s\" weaponDisplay=\"%s\" "
                    "damage=%d healthBefore=%d healthAfter=%d killed=%d applied=%d "
                    "serverTick=%u serverCode=\"%s\" gamemode=\"%s\" matchMode=\"%s\" "
                    "phase=%d mapOnly=%d enabled=%d",
                    n.id, n.body.username.c_str(),
                    nearestPlayer->id, nearestPlayer->name.c_str(),
                    wId, wDisp.c_str(),
                    damage, result.healthBefore, result.healthAfter,
                    (int)result.killed, (int)result.applied,
                    tick, getServerCoordinatorCode().c_str(),
                    gms.communityMode.c_str(), gms.matchMode.c_str(),
                    (int)gms.phase, (int)gms.mapOnly, (int)gms.enabled);
            }
            else if (nearestNpc)
            {
                auto victimIt = npcs.find(nearestNpc->id);
                if (victimIt != npcs.end() && victimIt->second.health > 0)
                {
                    ServerNpc& victim = victimIt->second;
                    victim.health = std::max(0, victim.health - damage);
                    victim.knockbackImpulse += knockback;
                    victim.lastAttackerId = n.id;
                    victim.lastAttackerPos = n.body.pos;
                    nearestNpc->body.currentHp = victim.health;
                    nearestNpc->body.killedByWeapon = wId;
                    nearestNpc->body.lastDamagedBy = n.body.username;
                    npcMindOnDamaged(*nearestNpc, n.id, n.body.pos, damage,
                                     nearestNpc->body.maxHp);
                    const bool killed = victim.health <= 0;
                    if (killed)
                    {
                        victim.health = 0;
                        nearestNpc->body.currentHp = 0;
                        nearestNpc->body.dead = true;
                        nearestNpc->body.respawnTimer = npcInitialRespawnTimer();
                        LiveEventJournal::Fields deathEvent;
                        deathEvent.tick = tick;
                        deathEvent.entityId = nearestNpc->id;
                        deathEvent.actorId = std::to_string(n.id);
                        deathEvent.result = "killed";
                        deathEvent.extra = std::string("\"victim_entity_type\":\"npc\",\"killer_entity_type\":\"npc\",\"weapon_id\":\"") +
                            wId + "\",\"weapon_display\":\"" + wDisp +
                            "\",\"health_after\":0,\"respawn_seconds\":" +
                            std::to_string(nearestNpc->body.respawnTimer);
                        LiveEventJournal::instance().record("server.npc_died", deathEvent);
                        npcMindOnKill(n);
                        serverGamemodeRecordKill(sock, players, &npcs,
                            n.id, ENTITY_NPC, nearestNpc->id, ENTITY_NPC,
                            wId, wDisp, tick,
                            n.body.pos, nearestNpc->body.pos, tick, totalPacketsOut);
                    }
                    DBG(Network,
                        "SERVER_NPC_KILLS_NPC proc=server npcId=%u npcName=\"%s\" "
                        "victimNpcId=%u victimName=\"%s\" weaponId=\"%s\" weaponDisplay=\"%s\" "
                        "damage=%d victimHealth=%d killed=%d serverTick=%u",
                        n.id, n.body.username.c_str(),
                        nearestNpc->id, nearestNpc->body.username.c_str(),
                        wId, wDisp.c_str(), damage, victim.health,
                        (int)killed, tick);
                }
            }
        }
    }

    // Respawn killed NPCs (updateOneNpc freezes dead bodies; this loop drives
    // their countdown and resets them so rebuildServerNpcMap re-admits them).
    // One-life modes never revive: the dead body stays Spectating for the round.
    for (Npc& n : npcSystem.all())
    {
        if (!n.body.dead) continue;
        if (npcRespawnTick(n.body.respawnTimer))
            respawnServerNpc(n);
    }

    // The NPC map is the authoritative lifecycle gate for its rockets. This
    // runs before the next NPC fire broadcast, so an NPC that died this tick
    // cannot leave a projectile alive or fire again while its body respawns.
    cancelDeadNpcProjectiles(sock, players, npcs, projectiles, tick, totalPacketsOut);

    // Broadcast NPC weapon fire so clients see/hear the shot.
    // For projectile weapons (rocket, grenade), also creates ServerProjectile
    // and broadcasts ProjectileSpawnEventPacket — same path as player rockets.
    broadcastNpcFiring(sock, players, npcSystem, projectiles, nextProjectileId, tick, totalPacketsOut);

    // Once-per-second per-NPC fire-state summary: which gate would block firing
    // (state, ammo, reload, cooldown, distance, LOS). One aggregate line, never
    // per-frame spam.
    {
        static uint64_t lastFireStateLog = 0;
        const uint64_t nowFire = nowMs();
        if (nowFire - lastFireStateLog >= 1000)
        {
            lastFireStateLog = nowFire;
            std::string summary;
            for (const Npc& n : npcSystem.all())
            {
                const auto& rtIt = n.body.weaponRuntimes.find(n.body.equippedWeaponId);
                const bool hasRt = rtIt != n.body.weaponRuntimes.end();
                const float planarSpeed =
                    glm::length(glm::vec2(n.body.vel.x, n.body.vel.y));
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "id=%u state=%s spd=%.1f ammo=%d reserve=%d reloading=%d cd=%.2f "
                    "dist=%.1f los=%d hasTarget=%d; ",
                    n.id, npcStateName(n.stateMachine.currentState).c_str(),
                    planarSpeed,
                    hasRt ? rtIt->second.currentAmmo : -1,
                    hasRt ? rtIt->second.reserveAmmo : -1,
                    hasRt ? (int)rtIt->second.isReloading : -1,
                    n.attackCooldown,
                    n.sensors.targetDistance,
                    (int)n.cachedLoSBlocked, (int)n.sensors.hasTarget);
                summary += buf;
            }
            Debug::warn(Debug::Category::NpcCombat,
                "[SERVER NPC FIRE-STATE] %s\n", summary.c_str());
        }
    }

    rebuildServerNpcMap(npcs, npcSystem, npcIdsAlive);

    // Record each NPC's broadcast pose for hit-rewind validation. This runs
    // after the map rebuild so the recorded pose is exactly what the snapshot
    // built for this tick will contain (what clients see and shoot at).
    for (auto& kv : npcs)
        pushNpcPositionHistory(kv.second, tick);

    static uint64_t lastNpcPosLog = 0;
    const uint64_t now = nowMs();
    if (now - lastNpcPosLog >= 1000)
    {
        lastNpcPosLog = now;
        for (const auto& kv : npcs)
            printf("[SERVER NPC] id=%u pos=(%.2f,%.2f,%.2f) hp=%d yaw=%.0f\n",
                   kv.first, kv.second.pos.x, kv.second.pos.y, kv.second.pos.z,
                   kv.second.health, kv.second.yaw);
    }
}

SnapshotEntity makeNpcEntity(const ServerNpc& npc)
{
    SnapshotEntity out{};
    out.networkEntityId = npc.entityId;
    out.entityType = ENTITY_NPC;
    out.active = 1;
    out.ownerClientId = 0;
    out.transformEpoch = npc.transformEpoch;
    out.px = npc.pos.x; out.py = npc.pos.y; out.pz = npc.pos.z;
    out.vx = npc.vel.x; out.vy = npc.vel.y; out.vz = npc.vel.z;
    out.aimX = npc.aim.x; out.aimY = npc.aim.y; out.aimZ = npc.aim.z;
    out.yaw = npc.yaw;
    out.health = npc.health;
    if (npc.health < 100)
        printf("[SERVER SNAPSHOT NPC] entityId=%u health=%d\n", npc.entityId, npc.health);
    out.onGround = npc.onGround ? 1 : 0;
    out.equippedSlot = npc.equippedSlot;
    out.weaponState = npc.weaponState;
    out.sizeScale = 1.0f;
    out.stateFlags = 0;
    if (npc.onGround)
        out.stateFlags |= NET_STATE_ON_GROUND;
    if (glm::length(npc.vel) > 0.5f)
        out.stateFlags |= NET_STATE_WALKING;
    copyName(out.displayName, npc.name);
    std::memset(out.avatarName, 0, sizeof(out.avatarName));
    std::strncpy(out.avatarName, npc.avatarName.c_str(), sizeof(out.avatarName) - 1);
    return out;
}

} // namespace MimitaNet
