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
#include "entities/player.h"
#include "world/world.h"
#include "physics/movement/physics-collision-shared.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace MimitaNet {

void buildNpcWorldCollision(World& npcWorld, const HeadlessWorld& hw)
{
    // The client NpcSystem only reads collision data from the World, so mirror
    // the headless collision world (no GPU/GL work, safe on a headless server).
    npcWorld.collisionMesh.triangles = hw.triangles;
    npcWorld.collisionChunkSize = hw.collisionChunkSize;
    npcWorld.collisionLargeTriangles.clear();
    npcWorld.collisionChunks.clear();

    constexpr int MAX_CHUNKS_PER_TRIANGLE = 256;
    for (int i = 0; i < (int)hw.triangles.size(); ++i)
    {
        const AABB tb = makeTriangleAABB(hw.triangles[i]);
        const glm::ivec3 c0 = collisionChunkCoord(tb.min, hw.collisionChunkSize);
        const glm::ivec3 c1 = collisionChunkCoord(tb.max, hw.collisionChunkSize);
        const int chunkCount =
            (c1.x - c0.x + 1) * (c1.y - c0.y + 1) * (c1.z - c0.z + 1);
        if (chunkCount > MAX_CHUNKS_PER_TRIANGLE)
        {
            npcWorld.collisionLargeTriangles.push_back(i);
            continue;
        }
        for (int x = c0.x; x <= c1.x; ++x)
        for (int y = c0.y; y <= c1.y; ++y)
        for (int z = c0.z; z <= c1.z; ++z)
            npcWorld.collisionChunks[glm::ivec3(x, y, z)].push_back(i);
    }
    printf("[SERVER NPC WORLD] built CPU collision: triangles=%zu chunks=%zu largeTris=%zu\n",
           npcWorld.collisionMesh.triangles.size(), npcWorld.collisionChunks.size(),
           npcWorld.collisionLargeTriangles.size());
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
            break;
        }
    }
}

// Rebuild the snapshot/broadcast ServerNpc map from the simulated NPCs.
static void rebuildServerNpcMap(std::unordered_map<uint32_t, ServerNpc>& npcs,
                                const NpcSystem& npcSystem,
                                std::unordered_set<uint32_t>& npcIdsAlive)
{
    npcs.clear();
    npcIdsAlive.clear();
    for (const Npc& n : npcSystem.all())
    {
        if (n.body.dead || n.body.currentHp <= 0) continue;
        ServerNpc sn;
        sn.entityId = n.id;
        sn.name = n.body.username.empty()
            ? "NPC " + std::to_string(n.id)
            : n.body.username;
        sn.pos = n.body.pos;
        sn.vel = n.body.vel;
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
        npcs[sn.entityId] = sn;
        npcIdsAlive.insert(sn.entityId);
    }
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

    // Pick the nearest live player (to the first NPC) as the shared AI target.
    ServerPlayer* target = nullptr;
    float bestD2 = std::numeric_limits<float>::max();
    glm::vec3 anchor(0.0f, 0.0f, 0.0f);
    if (!npcSystem.all().empty())
        anchor = npcSystem.all().front().body.pos;
    for (auto& kv : players)
    {
        ServerPlayer& p = kv.second;
        if (p.dead) continue;
        const glm::vec3 d = p.pos - anchor;
        const float d2 = glm::dot(d, d);
        if (d2 < bestD2) { bestD2 = d2; target = &p; }
    }

    mirrorPlayer.pos = target ? target->pos : anchor;
    mirrorPlayer.vel = target ? target->vel : glm::vec3(0.0f);
    mirrorPlayer.currentHp = target ? target->health : 100;
    mirrorPlayer.dead = target ? target->dead : false;
    const int hpBefore = mirrorPlayer.currentHp;

    npcSystem.update(world, mirrorPlayer, SERVER_DT);

    // Route hitscan damage the NPCs dealt to the mirror back onto the real
    // nearest player. The snapshot then carries the new HP to the victim.
    if (target && hpBefore > mirrorPlayer.currentHp)
    {
        const int damage = hpBefore - mirrorPlayer.currentHp;
        ServerDamageResult result = applyServerDamage(
            players, *target, 0, damage, glm::vec3(0.0f),
            ServerDamageSource::Hitscan);
        queueServerDamageConfirmedEvent(
            sock, players, tick, totalPacketsOut, 0, *target, damage, result,
            mirrorPlayer.pos, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f),
            ServerDamageSource::Hitscan, NETWORK_WEAPON_NONE);
    }

    rebuildServerNpcMap(npcs, npcSystem, npcIdsAlive);

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
    out.px = npc.pos.x; out.py = npc.pos.y; out.pz = npc.pos.z;
    out.vx = npc.vel.x; out.vy = npc.vel.y; out.vz = npc.vel.z;
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
