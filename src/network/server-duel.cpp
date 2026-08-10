// 08 10 2026, 14 34
/* purpose
* Implements the authoritative PvP duel state machine on the server.
* Waits for two players, runs a single countdown, scores first-to-goal,
* instant-respawns victims at their team spawn, and auto-rematches after the
* post-match window while both players are still connected.
* Does NOT apply damage, simulate movement, or render anything.
* Does NOT touch the client queue/matchmaking or coordinator protocol.
* Does NOT alter normal (non-duel) server behavior.
*/

#include "network/server-duel.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "network/packets.h"
#include "network/server.h"
#include "npc/npc.h"
#include "debug/debug-log.h"

namespace MimitaNet {

ServerDuelState& serverDuelState()
{
    static ServerDuelState state;
    return state;
}

void serverDuelStart(const ServerDuelState& rules)
{
    ServerDuelState& d = serverDuelState();
    d.enabled = true;
    d.phase = DUEL_PHASE_WAITING;
    d.matchOver = false;
    d.scoreA = 0;
    d.scoreB = 0;
    d.goalValue = rules.goalValue;
    d.countdownSeconds = rules.countdownSeconds;
    d.rematchSeconds = rules.rematchSeconds;
    d.teamAName = rules.teamAName;
    d.teamBName = rules.teamBName;
    d.playerAId = 0;
    d.playerBId = 0;
    d.winnerPlayerId = 0;
    d.spawnsAssigned = false;
    d.stateSent = false;
    d.mapPool = rules.mapPool;
    d.rotateMaps = rules.rotateMaps;
    d.mapId = rules.mapId;
    d.hasPendingManualMap = false;
    d.pendingManualMap.clear();
    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] enabled goal=%d countdown=%.1fs rematch=%.1fs teams=%s/%s rotate=%d pool=%zu\n",
        d.goalValue, d.countdownSeconds, d.rematchSeconds,
        d.teamAName.c_str(), d.teamBName.c_str(), (int)d.rotateMaps, d.mapPool.size());
}

namespace {

uint32_t countActivePlayers(const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    uint32_t count = 0;
    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active)
            ++count;
    }
    return count;
}

void broadcastDuelState(SOCKET sock,
                        const ServerDuelState& d,
                        const std::unordered_map<uint32_t, ServerPlayer>& players,
                        uint64_t& totalPacketsOut)
{
    DuelStatePacket pkt{};
    pkt.header.type = PACKET_DUEL_STATE;
    pkt.header.tick = 0;
    pkt.phase = d.phase;
    pkt.matchOver = d.matchOver ? 1 : 0;
    pkt.scoreA = d.scoreA;
    pkt.scoreB = d.scoreB;
    pkt.goalValue = d.goalValue;
    pkt.countdownLeft = d.countdown;
    pkt.rematchLeft = d.rematchLeft;
    pkt.playerAId = d.playerAId;
    pkt.playerBId = d.playerBId;
    pkt.winnerPlayerId = d.winnerPlayerId;
    std::strncpy(pkt.teamAName, d.teamAName.c_str(), sizeof(pkt.teamAName) - 1);
    std::strncpy(pkt.teamBName, d.teamBName.c_str(), sizeof(pkt.teamBName) - 1);

    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        if (serverSendToPlayer(sock, kv.second, &pkt, sizeof(pkt)))
            ++totalPacketsOut;
    }
}

// Pick two far-apart spawn points for the two duelists.
void assignDuelSpawns(ServerDuelState& d, const HeadlessWorld& world)
{
    d.spawnsAssigned = true;
    if (world.spawnPoints.size() >= 2)
    {
        d.spawnA = world.spawnPoints[0].position;
        float bestDist = -1.0f;
        size_t bestIdx = 0;
        for (size_t i = 0; i < world.spawnPoints.size(); ++i)
        {
            const float dist = glm::length(world.spawnPoints[i].position - d.spawnA);
            if (dist > bestDist)
            {
                bestDist = dist;
                bestIdx = i;
            }
        }
        d.spawnB = world.spawnPoints[bestIdx].position;
    }
    else
    {
        d.spawnA = glm::vec3(1.0f, 5.0f, 30.0f);
        d.spawnB = glm::vec3(1.0f, 5.0f, 40.0f);
    }
    Debug::log(Debug::Category::Duel,
        "[DUEL SERVER] spawns A=(%.1f %.1f %.1f) B=(%.1f %.1f %.1f)\n",
        d.spawnA.x, d.spawnA.y, d.spawnA.z, d.spawnB.x, d.spawnB.y, d.spawnB.z);
}

void assignDuelists(ServerDuelState& d,
                    const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    d.playerAId = 0;
    d.playerBId = 0;
    for (const auto& kv : players)
    {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        if (d.playerAId == 0)
            d.playerAId = kv.first;
        else
            d.playerBId = kv.first;
    }
}

// Place both duelists at their team spawns with full HP and full ammo.
void teleportDuelistsToSpawns(ServerDuelState& d,
                              std::unordered_map<uint32_t, ServerPlayer>& players)
{
    auto place = [&](uint32_t playerId, const glm::vec3& spawn)
    {
        auto it = players.find(playerId);
        if (it == players.end()) return;
        ServerPlayer& p = it->second;
        p.duelSpawnPos = spawn;
        p.hasDuelSpawnPos = true;
        p.respawnSeconds = 0.0f;
        if (!p.dead)
        {
            beginAuthoritativeTransform(p, spawn, glm::vec3(0.0f), p.yaw, "duel-spawn");
            p.justRespawned = true;
        }
    };
    place(d.playerAId, d.spawnA);
    place(d.playerBId, d.spawnB);
}

void beginDuelCountdown(ServerDuelState& d,
                        std::unordered_map<uint32_t, ServerPlayer>& players)
{
    d.matchOver = false;
    d.scoreA = 0;
    d.scoreB = 0;
    d.winnerPlayerId = 0;
    d.phase = DUEL_PHASE_COUNTDOWN;
    d.countdown = d.countdownSeconds;
    teleportDuelistsToSpawns(d, players);
    Debug::log(Debug::Category::Duel,
        "[DUEL SERVER] countdown started players=%u/%u\n", d.playerAId, d.playerBId);
}

// loadHeadlessWorld appends, so clear everything it populates before a reload.
void clearHeadlessWorld(HeadlessWorld& world)
{
    world.triangles.clear();
    world.boundsMin = glm::vec3(0.0f);
    world.boundsMax = glm::vec3(0.0f);
    world.spawnPoints.clear();
    world.collisionChunks.clear();
    world.collisionLargeTriangles.clear();
    world.collisionSubGrids.clear();
}

void broadcastMapChange(SOCKET sock,
                        const std::string& mapId,
                        const std::unordered_map<uint32_t, ServerPlayer>& players,
                        uint64_t& totalPacketsOut)
{
    MapChangePacket pkt{};
    pkt.header.type = PACKET_MAP_CHANGE;
    pkt.header.tick = 0;
    std::strncpy(pkt.mapId, mapId.c_str(), sizeof(pkt.mapId) - 1);
    for (const auto& kv : players)
    {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        if (serverSendToPlayer(sock, kv.second, &pkt, sizeof(pkt)))
            ++totalPacketsOut;
    }
}

// Reload the headless world + NPC collision world for a new map, re-pick the
// duel spawns, teleport both players, and broadcast the map change.
void reloadDuelMap(SOCKET sock,
                   ServerDuelState& d,
                   std::unordered_map<uint32_t, ServerPlayer>& players,
                   HeadlessWorld& world,
                   World& npcWorld,
                   const std::string& mapId,
                   uint64_t& totalPacketsOut)
{
    const std::string path = "assets/maps/" + mapId + ".glb";
    clearHeadlessWorld(world);
    if (!loadHeadlessWorld(path.c_str(), world))
    {
        Debug::error(Debug::Category::Duel,
            "[DUEL SERVER] map reload failed: %s\n", path.c_str());
        return;
    }
    buildNpcWorldCollision(npcWorld, world);
    setServerMapId(mapId);
    d.mapId = mapId;
    assignDuelSpawns(d, world);
    broadcastMapChange(sock, mapId, players, totalPacketsOut);
    teleportDuelistsToSpawns(d, players);
    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] map changed live to %s (spawns=%zu)\n",
        mapId.c_str(), world.spawnPoints.size());
}

std::string nextRandomMap(const ServerDuelState& d)
{
    if (d.mapPool.empty()) return d.mapId;
    std::vector<std::string> candidates = d.mapPool;
    if (d.mapPool.size() > 1)
    {
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(),
                           [&](const std::string& m) { return m == d.mapId; }),
            candidates.end());
    }
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    return candidates[dist(rng)];
}

} // namespace

void serverDuelTick(SOCKET sock,
                    std::unordered_map<uint32_t, ServerPlayer>& players,
                    HeadlessWorld& world,
                    World& npcWorld,
                    std::unordered_map<uint32_t, ServerNpc>& npcs,
                    NpcSystem& npcSystem,
                    std::unordered_set<uint32_t>& npcIdsAlive,
                    uint32_t tick,
                    uint64_t& totalPacketsOut)
{
    ServerDuelState& d = serverDuelState();
    if (!d.enabled) return;
    (void)tick;

    // Host-only changemap command: swap the map live on the next tick.
    if (d.hasPendingManualMap)
    {
        const std::string mapId = d.pendingManualMap;
        d.hasPendingManualMap = false;
        d.pendingManualMap.clear();
        if (!mapId.empty())
        {
            reloadDuelMap(sock, d, players, world, npcWorld, mapId, totalPacketsOut);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
    }

    // Process any kill recorded by applyServerDamage last tick.
    if (d.hasPendingKill)
    {
        d.hasPendingKill = false;
        const uint32_t killerId = d.pendingKillerId;
        const uint32_t victimId = d.pendingVictimId;

        // Instant respawn at the victim's team spawn with full HP/ammo.
        auto victimIt = players.find(victimId);
        if (victimIt != players.end())
            victimIt->second.respawnSeconds = 0.0f;

        // Tell the killer where the victim respawned (tracer).
        if (killerId != victimId)
        {
            DuelEnemySpawnPacket tracer{};
            tracer.header.type = PACKET_DUEL_ENEMY_SPAWN;
            tracer.header.tick = 0;
            tracer.enemyPlayerId = victimId;
            glm::vec3 spawnPos = victimIt != players.end() && victimIt->second.hasDuelSpawnPos
                ? victimIt->second.duelSpawnPos : glm::vec3(0.0f);
            tracer.posX = spawnPos.x;
            tracer.posY = spawnPos.y;
            tracer.posZ = spawnPos.z;

            auto killerIt = players.find(killerId);
            if (killerIt != players.end() && killerIt->second.spawnState == ServerPlayer::Active)
            {
                if (serverSendToPlayer(sock, killerIt->second, &tracer, sizeof(tracer)))
                    ++totalPacketsOut;
            }
        }

        // Score only counts during the active phase, and never for a suicide.
        if (d.phase == DUEL_PHASE_ACTIVE && !d.matchOver && killerId != victimId)
        {
            if (killerId == d.playerAId)
                ++d.scoreA;
            else if (killerId == d.playerBId)
                ++d.scoreB;

            if (d.scoreA >= d.goalValue || d.scoreB >= d.goalValue)
            {
                d.matchOver = true;
                d.phase = DUEL_PHASE_MATCH_END;
                d.winnerPlayerId = killerId;
                d.rematchLeft = d.rematchSeconds;
                Debug::warn(Debug::Category::Duel,
                    "[DUEL SERVER] match over winner=%u score=%d-%d goal=%d\n",
                    d.winnerPlayerId, d.scoreA, d.scoreB, d.goalValue);
            }
        }

        broadcastDuelState(sock, d, players, totalPacketsOut);
    }

    switch (d.phase)
    {
    case DUEL_PHASE_WAITING:
        if (countActivePlayers(players) >= 2)
        {
            assignDuelists(d, players);
            assignDuelSpawns(d, world);
            // Drop the practice NPC(s) once the real duel is about to start.
            npcs.clear();
            npcSystem.destroyAll();
            npcIdsAlive.clear();
            beginDuelCountdown(d, players);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_COUNTDOWN:
        // A duelist vanished before the fight — fall back to waiting.
        if (players.find(d.playerAId) == players.end() ||
            players.find(d.playerBId) == players.end())
        {
            d.phase = DUEL_PHASE_WAITING;
            d.stateSent = false;
            break;
        }
        d.countdown -= SERVER_DT;
        if (d.countdown <= 0.0f)
        {
            d.phase = DUEL_PHASE_ACTIVE;
            d.stateSent = false;
            Debug::log(Debug::Category::Duel, "[DUEL SERVER] fight started\n");
        }
        broadcastDuelState(sock, d, players, totalPacketsOut);
        break;

    case DUEL_PHASE_ACTIVE:
        if (players.find(d.playerAId) == players.end() ||
            players.find(d.playerBId) == players.end())
        {
            d.phase = DUEL_PHASE_WAITING;
            d.stateSent = false;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        else if (tick - d.lastBroadcastTick >= 60)
        {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_MATCH_END:
        if (players.find(d.playerAId) == players.end() ||
            players.find(d.playerBId) == players.end())
        {
            // Opponent left during the end screen — no rematch.
            d.stateSent = false;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            break;
        }
        d.rematchLeft -= SERVER_DT;
        if (d.rematchLeft <= 0.0f)
        {
            // Auto-rotate to a fresh random map for the next duel.
            if (d.rotateMaps && d.mapPool.size() > 1)
                reloadDuelMap(sock, d, players, world, npcWorld,
                              nextRandomMap(d), totalPacketsOut);
            Debug::log(Debug::Category::Duel, "[DUEL SERVER] rematch\n");
            beginDuelCountdown(d, players);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        else if (tick - d.lastBroadcastTick >= 60)
        {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;
    }
}

void serverDuelRematchNow()
{
    ServerDuelState& d = serverDuelState();
    if (!d.enabled) return;
    // The MATCH_END branch starts the next duel when rematchLeft hits 0.
    d.rematchLeft = 0.0f;
}

void serverDuelRequestMapChange(const std::string& mapId)
{
    ServerDuelState& d = serverDuelState();
    if (!d.enabled || mapId.empty()) return;
    d.hasPendingManualMap = true;
    d.pendingManualMap = mapId;
}

void serverDuelOnPlayerDeath(uint32_t killerPlayerId,
                             uint32_t victimPlayerId)
{
    ServerDuelState& d = serverDuelState();
    if (!d.enabled) return;
    d.hasPendingKill = true;
    d.pendingKillerId = killerPlayerId;
    d.pendingVictimId = victimPlayerId;
}

} // namespace MimitaNet
