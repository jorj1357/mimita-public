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

#include "npc/npc.h"
#include "npc/npc-combat.h"
#include "npc/npc-navigation.h"
#include "npc/npc-combat-log.h"
#include "entities/player.h"
#include "world/world.h"
#include "map/map-loader-collision.h"
#include "config/collision-lod-config.h"
#include "physics/movement/physics-collision-shared.h"
#include "combat/weapon-registry.h"
#include "combat/weapon-runtime.h"
#include "network/network-weapons.h"
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
            if (n.id == kv.first && serverGameOverrides().maxHpOverride > 0)
            {
                n.body.maxHp = serverGameOverrides().maxHpOverride;
                n.body.currentHp = n.body.maxHp;
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
            if (n.body.currentHp != kv.second.health)
                n.body.currentHp = kv.second.health;
            if (glm::length(kv.second.knockbackImpulse) > 0.05f)
                n.body.externalImpulse += kv.second.knockbackImpulse;
            // A killed ServerNpc must stop acting immediately and start its
            // respawn countdown. updateOneNpc early-returns on dead, so the
            // body freezes until respawnServerNpc resets it.
            if (kv.second.health <= 0 && !n.body.dead)
            {
                n.body.currentHp = 0;
                n.body.dead = true;
                n.body.respawnTimer = SERVER_NPC_RESPAWN_SECONDS;
            }
            break;
        }
    }
}

// Reset a killed server NPC body back to full health at its spawn point so the
// snapshot pipeline re-admits it (rebuildServerNpcMap skips dead bodies).
static void respawnServerNpc(Npc& npc)
{
    const glm::vec3 spawnPos = effectiveServerSpawn(npc.body.respawnPosition);
    npc.body.pos = spawnPos;
    npc.body.respawnPosition = spawnPos;
    npc.body.vel = glm::vec3(0.0f);
    npc.body.externalImpulse = glm::vec3(0.0f);
    // New life: bump the lifecycle counter so clients detect the respawn,
    // hard-snap the body to the spawn, and reset their death-presentation
    // state for the next death. Wrap to [1,65535] — 0 is ignored by clients.
    npc.transformEpoch = static_cast<uint16_t>((npc.transformEpoch % 65535) + 1);
    // healthall override: new spawns get the override max HP.
    if (serverGameOverrides().maxHpOverride > 0)
        npc.body.maxHp = serverGameOverrides().maxHpOverride;
    npc.body.currentHp = npc.body.maxHp;
    npc.body.dead = false;
    npc.body.respawnTimer = 0.0f;
    npc.body.killedBy.clear();
    npc.body.spawnFlashTimer = 10.0f;
    npc.attackCooldown = 0.0f;
    resetAllWeaponRuntimesForSpawn(npc.body, "server-npc-respawn");
    npc.body.syncLegacyStateToLayers();
    npc.body.updateModelWorldTransforms();
    printf("%s [SERVER NPC RESPAWN] id=%u pos=(%.2f,%.2f,%.2f)\n",
           serverTimestamp(), npc.id, spawnPos.x, spawnPos.y, spawnPos.z);
}

// Broadcast the visual/sound of an NPC's weapon firing to every client so the
// shot (muzzle flash, tracer, sound, weapon trigger) shows up remotely.
static void broadcastNpcFiring(SOCKET sock,
                               std::unordered_map<uint32_t, ServerPlayer>& players,
                               NpcSystem& npcSystem,
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

        ShotEventPacket ev{};
        ev.header.type = PACKET_SHOT_EVENT;
        ev.header.tick = tick;
        ev.header.playerId = n.id;
        ev.shotSerial = 0;
        ev.clientTimeMs = 0;
        ev.shooterPlayerId = n.id;
        ev.targetPlayerId = 0;
        ev.weapon = netWeapon;
        ev.impactType = SHOT_IMPACT_NONE;
        ev.effectFlags = SHOT_EFFECT_MUZZLE | SHOT_EFFECT_TRACER |
            SHOT_EFFECT_SHOOT_SOUND | SHOT_EFFECT_WEAPON_TRIGGER;
        ev.originX = origin.x; ev.originY = origin.y; ev.originZ = origin.z;
        ev.hitX = hit.x; ev.hitY = hit.y; ev.hitZ = hit.z;
        ev.dirX = dir.x; ev.dirY = dir.y; ev.dirZ = dir.z;
        ev.normalX = -dir.x; ev.normalY = -dir.y; ev.normalZ = -dir.z;

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
        sn.pos = n.body.pos;
        sn.vel = n.body.vel;
        sn.aim = (glm::length(n.currentFacing) > 0.001f)
            ? glm::normalize(n.currentFacing)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        sn.yaw = n.body.yaw;
        sn.health = n.body.currentHp;
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
            const float frac = float(targetTick - a.tick) / float(b.tick - a.tick);
            outPos = glm::mix(a.pos, b.pos, frac);
            return true;
        }
    }
    outPos = npc.posHistory.front().pos;
    return true;
}

// Like getNpcPositionAtTick but also returns the broadcast yaw at that tick so
// NPC body-part hitboxes are reconstructed with the facing the attacker saw.
bool getNpcPoseAtTick(const ServerNpc& npc, uint32_t targetTick,
                      glm::vec3& outPos, float& outYaw)
{
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
    for (int i = (int)npc.posHistory.size() - 1; i > 0; --i)
    {
        if (npc.posHistory[i].tick <= targetTick)
        {
            const auto& a = npc.posHistory[i];
            const auto& b = npc.posHistory[i + 1];
            const float frac = float(targetTick - a.tick) / float(b.tick - a.tick);
            outPos = glm::mix(a.pos, b.pos, frac);
            outYaw = a.yaw + (b.yaw - a.yaw) * frac;
            return true;
        }
    }
    outPos = front.pos;
    outYaw = front.yaw;
    return true;
}

void simulateSharedNpcs(SOCKET sock,
                        std::unordered_map<uint32_t, ServerPlayer>& players,
                        std::unordered_map<uint32_t, ServerNpc>& npcs,
                        NpcSystem& npcSystem,
                        World& world,
                        Player& mirrorPlayer,
                        std::unordered_set<uint32_t>& npcIdsAlive,
                        uint32_t tick,
                        uint64_t& totalPacketsOut)
{
    adoptNewServerNpcs(npcs, npcSystem, npcIdsAlive);
    syncServerNpcDamageToNpc(npcs, npcSystem, npcIdsAlive);

    // Each NPC targets its own nearest live player so range never blocks firing
    // for NPCs far from a single shared target. Damage stays server-authoritative:
    // an NPC's damage to its mirror is routed onto the real player it targeted.
    for (Npc& n : npcSystem.all())
    {
        if (n.body.dead || n.body.currentHp <= 0)
            continue;

        ServerPlayer* nearest = nullptr;
        float bestD2 = std::numeric_limits<float>::max();
        for (auto& kv : players)
        {
            ServerPlayer& p = kv.second;
            if (p.dead || p.connectionStale) continue;
            const glm::vec3 d = p.pos - n.body.pos;
            const float d2 = glm::dot(d, d);
            if (d2 < bestD2) { bestD2 = d2; nearest = &p; }
        }

        if (nearest)
        {
            mirrorPlayer.pos = nearest->pos;
            mirrorPlayer.vel = nearest->vel;
            mirrorPlayer.yaw = nearest->yaw;
            mirrorPlayer.currentHp = nearest->health;
            mirrorPlayer.dead = nearest->dead;
        }
        else
        {
            // No live player to target: still advance the NPC's physics so it
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
        // floor are untouched; the clamp only corrects downward violations.
        {
            constexpr float REST_HEIGHT = 1.8f; // capsule half-height (feet at pos.z - 1.8)
            const float floorZ = NpcNavigation::groundHeightAt(
                world, n.body.pos, 40.0f, 1.5f);
            if (floorZ > -1e5f)
            {
                const float restZ = floorZ + REST_HEIGHT;
                if (n.body.pos.z < restZ)
                {
                    n.body.pos.z = restZ;
                    if (n.body.vel.z < 0.0f)
                        n.body.vel.z = 0.0f;
                    n.body.ground.hasWorldContact = true;
                    n.body.ground.stableOnGround = true;
                    n.body.ground.onGround = true;
                    n.body.ground.realWorldContactThisFrame = true;
                }
            }
        }

        if (nearest && hpBefore > mirrorPlayer.currentHp)
        {
            const int damage = hpBefore - mirrorPlayer.currentHp;
            // Forward the exact knockback processPlayerHit applied to the mirror
            // so NPC shots push the victim like a player's shot (also fixes the
            // client HP bar, which only applies confirmed HP when knockback exists).
            const glm::vec3 knockback = mirrorPlayer.externalImpulse;
            ServerDamageResult result = applyServerDamage(
                players, *nearest, 0, damage, knockback,
                ServerDamageSource::Hitscan);
            queueServerDamageConfirmedEvent(
                sock, players, tick, totalPacketsOut, 0, *nearest, damage, result,
                mirrorPlayer.pos, glm::vec3(0.0f, 0.0f, 1.0f), knockback,
                ServerDamageSource::Hitscan, NETWORK_WEAPON_NONE);
            npcLog("npc=%u weapon=%s damage=%d healthBefore=%d healthAfter=%d "
                   "accepted=%d knockback=(%.2f %.2f %.2f)",
                   n.id, n.body.equippedWeaponId.c_str(), damage, result.healthBefore,
                   result.healthAfter, (int)result.applied,
                   knockback.x, knockback.y, knockback.z);
        }
    }

    // Respawn killed NPCs (updateOneNpc freezes dead bodies; this loop drives
    // their countdown and resets them so rebuildServerNpcMap re-admits them).
    for (Npc& n : npcSystem.all())
    {
        if (!n.body.dead) continue;
        n.body.respawnTimer = std::max(0.0f, n.body.respawnTimer - SERVER_DT);
        if (n.body.respawnTimer <= 0.0f)
            respawnServerNpc(n);
    }

    // Broadcast NPC weapon fire so clients see/hear the shot.
    broadcastNpcFiring(sock, players, npcSystem, tick, totalPacketsOut);

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
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "id=%u state=%s ammo=%d reserve=%d reloading=%d cd=%.2f "
                    "dist=%.1f los=%d hasTarget=%d; ",
                    n.id, npcStateName(n.stateMachine.currentState).c_str(),
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
    return out;
}

} // namespace MimitaNet
