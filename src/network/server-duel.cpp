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
#include "network/community-server-config.h"

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
    d.mapOnly = false;
    d.communityMode = "sandbox";
    d.communityWeaponSetId = 1;
    d.communityScores.clear();
    d.communityTeams.clear();
    d.communityTeamScore[0] = d.communityTeamScore[1] = 0;
    d.communityRoundOver = false;
    d.communityRoundResetMs = 0;
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
    d.spawnOffsetRadius = rules.spawnOffsetRadius;
    d.mapPool = rules.mapPool;
    d.rotateMaps = rules.rotateMaps;
    d.mapId = rules.mapId;
    d.duelId = 0;
    d.mapVersion = 0;
    d.spawnAnchorVersion = 0;
    d.respawnSequence = 0;
    d.stateVersion = 0;
    d.spawnAnchorIndex = 0;
    d.usedMaps.clear();
    d.usedMaps.insert(rules.mapId); // avoid immediately re-picking the launch map
    d.hasPendingManualMap = false;
    d.pendingManualMap.clear();
    d.autoMapRotation = false;
    d.mapRotationMinutes = 15;
    d.nextMapRotationMs = 0;
    d.mapChangeCountdownStartMs = 0;
    d.pendingAutomaticMap.clear();
    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] enabled goal=%d countdown=%.1fs rematch=%.1fs teams=%s/%s rotate=%d pool=%zu offset=%.1f\n",
        d.goalValue, d.countdownSeconds, d.rematchSeconds,
        d.teamAName.c_str(), d.teamBName.c_str(), (int)d.rotateMaps, d.mapPool.size(),
        d.spawnOffsetRadius);
}

void serverCommunityMapStart(const std::vector<std::string>& mapPool,
                             const std::string& mapId,
                             bool autoRotation,
                             uint32_t rotationMinutes,
                             int weaponSetId)
{
    ServerDuelState rules;
    rules.mapPool = mapPool;
    rules.mapId = mapId;
    rules.rotateMaps = false;
    rules.mapOnly = true;
    rules.autoMapRotation = autoRotation;
    rules.mapRotationMinutes = std::clamp(rotationMinutes, 1u, 9999u);
    serverDuelStart(rules);
    ServerDuelState& state = serverDuelState();
    state.mapOnly = true;
    state.communityMode = "sandbox";
    state.communityWeaponSetId = std::max(1, weaponSetId);
    state.autoMapRotation = rules.autoMapRotation;
    state.mapRotationMinutes = rules.mapRotationMinutes;
    state.nextMapRotationMs = nowMs() + (uint64_t)state.mapRotationMinutes * 60000ull;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY MAP RUNTIME] map=%s auto=%d intervalMinutes=%u pool=%zu\n",
        mapId.c_str(), (int)autoRotation, state.mapRotationMinutes, mapPool.size());
}

void serverCommunitySetWeaponSet(int weaponSetId)
{
    ServerDuelState& state = serverDuelState();
    if (!state.enabled || !state.mapOnly) return;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    if (!config.weaponSetById(weaponSetId)) return;
    state.communityWeaponSetId = weaponSetId;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY WEAPON SET] selected=%d\n", state.communityWeaponSetId);
}

bool serverCommunityWeaponAllowed(const std::string& weaponId)
{
    const ServerDuelState& state = serverDuelState();
    if (!state.mapOnly) return true;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    return config.weaponAllowed(state.communityWeaponSetId, weaponId);
}

void serverCommunitySetMode(const std::string& modeId)
{
    ServerDuelState& state = serverDuelState();
    if (!state.enabled || !state.mapOnly || modeId.empty()) return;
    state.communityMode = modeId;
    state.communityScores.clear();
    state.communityTeams.clear();
    state.communityTeamScore[0] = state.communityTeamScore[1] = 0;
    state.communityRoundOver = false;
    state.communityRoundResetMs = 0;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY MODE] selected=%s\n", state.communityMode.c_str());
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
    pkt.duelId = d.duelId;
    pkt.mapVersion = d.mapVersion;
    pkt.spawnAnchorVersion = d.spawnAnchorVersion;
    pkt.respawnSequence = d.respawnSequence;
    pkt.stateVersion = d.stateVersion;
    std::strncpy(pkt.mapId, d.mapId.c_str(), sizeof(pkt.mapId) - 1);
    pkt.spawnAnchorIndex = d.spawnAnchorIndex;
    pkt.anchorX = d.spawnA.x;
    pkt.anchorY = d.spawnA.y;
    pkt.anchorZ = d.spawnA.z;
    auto a = players.find(d.playerAId);
    auto b = players.find(d.playerBId);
    if (a != players.end()) {
        pkt.playerASpawnGeneration = a->second.spawnGeneration;
        pkt.playerASpawnX = a->second.duelSpawnPos.x;
        pkt.playerASpawnY = a->second.duelSpawnPos.y;
        pkt.playerASpawnZ = a->second.duelSpawnPos.z;
    }
    if (b != players.end()) {
        pkt.playerBSpawnGeneration = b->second.spawnGeneration;
        pkt.playerBSpawnX = b->second.duelSpawnPos.x;
        pkt.playerBSpawnY = b->second.duelSpawnPos.y;
        pkt.playerBSpawnZ = b->second.duelSpawnPos.z;
    }
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
        const uint32_t eventId = nextReliableGameplayEventId();
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
        const bool sent = result == ReliableGameplayEventQueueResult::Queued;
        Debug::log(Debug::Category::Duel,
            "[ServerDuel] sent duel state duelId=%u version=%u phase=%u map=%s player=%u sent=%d score=%d-%d\n",
            d.duelId, d.stateVersion, (unsigned)d.phase, d.mapId.c_str(),
            kv.second.id, (int)sent, d.scoreA, d.scoreB);
    }
}

// Pick ONE random map spawn point as the match anchor. Both teams always
// spawn near this single point (with a fresh random XY offset each spawn), so
// respawns land right back in the fight — max action, no map editing needed.
void assignDuelSpawns(ServerDuelState& d, const HeadlessWorld& world)
{
    d.spawnsAssigned = true;
    if (!world.spawnPoints.empty())
    {
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, world.spawnPoints.size() - 1);
        const size_t anchorIndex = dist(rng);
        d.spawnAnchorIndex = (uint32_t)anchorIndex;
        ++d.spawnAnchorVersion;
        const glm::vec3 anchor = world.spawnPoints[anchorIndex].position;
        d.spawnA = anchor;
        d.spawnB = anchor;
        Debug::log(Debug::Category::Duel,
            "[DuelAnchor] map=%s anchorIndex=%zu anchor=(%.3f,%.3f,%.3f)\n",
            d.mapId.c_str(), anchorIndex, anchor.x, anchor.y, anchor.z);
    }
    else
    {
        d.spawnA = glm::vec3(1.0f, 5.0f, 30.0f);
        d.spawnB = d.spawnA;
        Debug::warn(Debug::Category::Duel,
            "[DuelFallback] map=%s reason=no_spawn_points final=(%.3f,%.3f,%.3f)\n",
            d.mapId.c_str(), d.spawnA.x, d.spawnA.y, d.spawnA.z);
    }
    Debug::log(Debug::Category::Duel,
        "[DUEL SERVER] anchor=(%.1f %.1f %.1f) spawns=%zu\n",
        d.spawnA.x, d.spawnA.y, d.spawnA.z, world.spawnPoints.size());
}

// The anchor plus a random XY offset (so nobody can predict the exact spot).
glm::vec3 duelSpawnPoint(const ServerDuelState& d)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-d.spawnOffsetRadius, d.spawnOffsetRadius);
    return d.spawnA + glm::vec3(dist(rng), dist(rng), 0.0f);
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

// Place both duelists near the match anchor with full HP and full ammo.
void teleportDuelistsToSpawns(ServerDuelState& d,
                              std::unordered_map<uint32_t, ServerPlayer>& players)
{
    auto place = [&](uint32_t playerId)
    {
        auto it = players.find(playerId);
        if (it == players.end()) return;
        ServerPlayer& p = it->second;
        const glm::vec3 spawn = duelSpawnPoint(d);
        p.duelSpawnPos = spawn;
        p.hasDuelSpawnPos = true;
        const glm::vec3 offset = spawn - d.spawnA;
        Debug::log(Debug::Category::Duel,
            "[DuelSpawn] player=%u map=%s anchor=(%.3f,%.3f,%.3f) offset=(%.3f,%.3f,%.3f) final=(%.3f,%.3f,%.3f)\n",
            playerId, d.mapId.c_str(), d.spawnA.x, d.spawnA.y, d.spawnA.z,
            offset.x, offset.y, offset.z, spawn.x, spawn.y, spawn.z);
        p.respawnSeconds = 0.0f;
        if (!p.dead)
        {
            beginAuthoritativeTransform(p, spawn, glm::vec3(0.0f), p.yaw, "duel-spawn");
            p.justRespawned = true;
        }
    };
    place(d.playerAId);
    place(d.playerBId);
}

void beginDuelCountdown(ServerDuelState& d,
                        std::unordered_map<uint32_t, ServerPlayer>& players)
{
    ++d.duelId;
    ++d.respawnSequence;
    ++d.stateVersion;
    d.matchOver = false;
    d.scoreA = 0;
    d.scoreB = 0;
    d.winnerPlayerId = 0;
    d.phase = DUEL_PHASE_COUNTDOWN;
    d.countdown = d.countdownSeconds;
    Debug::log(Debug::Category::Duel,
        "[ServerDuel] selected authoritative map=%s duelId=%u stateVersion=%u\n",
        d.mapId.c_str(), d.duelId, d.stateVersion);
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
                        const ServerDuelState& d,
                        const std::string& mapId,
                        const std::unordered_map<uint32_t, ServerPlayer>& players,
                        uint64_t& totalPacketsOut)
{
    MapChangePacket pkt{};
    pkt.header.type = PACKET_MAP_CHANGE;
    pkt.header.tick = 0;
    std::strncpy(pkt.mapId, mapId.c_str(), sizeof(pkt.mapId) - 1);
    pkt.duelId = d.duelId;
    pkt.mapVersion = d.mapVersion;
    for (const auto& kv : players)
    {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const uint32_t eventId = nextReliableGameplayEventId();
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
        const bool sent = result == ReliableGameplayEventQueueResult::Queued;
        Debug::log(Debug::Category::Duel,
            "[DuelMap] send map=%s player=%u reliable=1 sent=%d version=%u\n",
            mapId.c_str(), kv.second.id, (int)sent, d.mapVersion);
        Debug::log(Debug::Category::Duel,
            "[DuelPacketSend] type=MapChangePacket reliable=1 player=%u sent=%d map=%s\n",
            kv.second.id, (int)sent, mapId.c_str());
        if (sent)
            ++totalPacketsOut;
    }
}

void broadcastCommunityNotification(
    SOCKET sock,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    const std::string& message,
    uint16_t durationTicks,
    uint64_t& totalPacketsOut)
{
    ServerNotificationPacket packet{};
    packet.header.type = PACKET_SERVER_NOTIFICATION;
    packet.eventId = nextReliableGameplayEventId();
    packet.eventSessionId = serverReliableEventSessionId();
    packet.durationTicks = durationTicks;
    std::strncpy(packet.title, "MiMITA Server", sizeof(packet.title) - 1);
    std::strncpy(packet.message, message.c_str(), sizeof(packet.message) - 1);
    for (auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active) continue;
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, kv.second, &packet, sizeof(packet), packet.eventId,
            reliableGameplayEventSessionForPlayer(kv.second), totalPacketsOut);
        Debug::log(Debug::Category::Networking,
            "[SERVER NOTIFICATION SEND] player=%u queued=%d message=%s\n",
            kv.second.id, result == ReliableGameplayEventQueueResult::Queued,
            message.c_str());
    }
}

// Load a map into a fresh temp world; true only if it loads AND has real
// spawn points, so players always anchor at spawn points, never under the map.
bool tryLoadDuelMap(const std::string& mapId, HeadlessWorld& out)
{
    const std::string path = "assets/maps/" + mapId + ".glb";
    HeadlessWorld candidate;
    if (!loadHeadlessWorld(path.c_str(), candidate))
        return false;
    if (candidate.spawnPoints.empty())
        return false;
    out = std::move(candidate);
    return true;
}

// Move a loaded temp world into the live world + NPC collision world.
void commitDuelMap(ServerDuelState& d, HeadlessWorld& world, World& npcWorld,
                   HeadlessWorld& tmp, const std::string& mapId)
{
    clearHeadlessWorld(world);
    world = std::move(tmp);
    buildNpcWorldCollision(npcWorld, world);
    setServerMapId(mapId);
    d.mapId = mapId;
    ++d.mapVersion;
    ++d.stateVersion;
}

// Reload the world for a chosen map (changemap / rotation commit). Only ever
// touches the live world after the new map is confirmed loaded, so a failed
// swap never empties the world (players never fall under it).
bool reloadDuelMap(SOCKET sock,
                   ServerDuelState& d,
                   std::unordered_map<uint32_t, ServerPlayer>& players,
                   HeadlessWorld& world,
                   World& npcWorld,
                   const std::string& mapId,
                   uint64_t& totalPacketsOut)
{
    HeadlessWorld tmp;
    if (!tryLoadDuelMap(mapId, tmp))
    {
        Debug::error(Debug::Category::Duel,
            "[DUEL SERVER] map swap failed or has no spawn points: %s\n", mapId.c_str());
        return false;
    }
    commitDuelMap(d, world, npcWorld, tmp, mapId);
    assignDuelSpawns(d, world);
    broadcastMapChange(sock, d, mapId, players, totalPacketsOut);
    teleportDuelistsToSpawns(d, players);
    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] map changed live to %s (spawns=%zu)\n",
        mapId.c_str(), world.spawnPoints.size());
    return true;
}

// Round-robin rotation: pick a map not used this cycle (never the one just
// played), skip maps that fail to load or have no spawn points, and cycle the
// whole pool before repeating. Returns true if the map changed.
bool rotateToNextDuelMap(SOCKET sock,
                         ServerDuelState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         HeadlessWorld& world,
                         World& npcWorld,
                         uint64_t& totalPacketsOut)
{
    if (d.mapPool.size() <= 1)
        return false;

    auto unusedCandidates = [&]() {
        std::vector<std::string> v;
        for (const auto& m : d.mapPool)
            if (!d.usedMaps.count(m) && m != d.mapId)
                v.push_back(m);
        return v;
    };

    std::vector<std::string> candidates = unusedCandidates();
    if (candidates.empty())
    {
        // Whole pool used this cycle — start fresh, still avoiding the current map.
        d.usedMaps.clear();
        d.usedMaps.insert(d.mapId);
        candidates = unusedCandidates();
    }

    std::mt19937 rng(std::random_device{}());
    std::shuffle(candidates.begin(), candidates.end(), rng);

    for (const std::string& cand : candidates)
    {
        HeadlessWorld tmp;
        if (tryLoadDuelMap(cand, tmp))
        {
            d.usedMaps.insert(cand);
            commitDuelMap(d, world, npcWorld, tmp, cand);
            broadcastMapChange(sock, d, cand, players, totalPacketsOut);
            Debug::warn(Debug::Category::Duel,
                "[DUEL SERVER] rotated to map %s (spawns=%zu)\n",
                cand.c_str(), world.spawnPoints.size());
            return true;
        }
        // Failed to load or has no spawn points — skip it this cycle.
        d.usedMaps.insert(cand);
    }
    return false; // nothing valid — keep the current map
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
    if (d.mapOnly)
    {
        const uint64_t now = nowMs();
        if (d.communityRoundOver && now >= d.communityRoundResetMs) {
            d.communityRoundOver = false;
            d.communityScores.clear();
            d.communityTeamScore[0] = d.communityTeamScore[1] = 0;
            Debug::log(Debug::Category::Networking, "[COMMUNITY MATCH] new round mode=%s\n", d.communityMode.c_str());
        }
        if (d.hasPendingKill) {
            const uint32_t killer = d.pendingKillerId;
            d.hasPendingKill = false;
            if (!d.communityRoundOver && killer != 0) {
                if (d.communityMode == "team_deathmatch") {
                    std::vector<uint32_t> ids;
                    for (const auto& kv : players)
                        if (kv.second.spawnState == ServerPlayer::Active) ids.push_back(kv.first);
                    std::sort(ids.begin(), ids.end());
                    for (size_t i = 0; i < ids.size(); ++i)
                        d.communityTeams[ids[i]] = (int)(i % 2);
                    const auto teamIt = d.communityTeams.find(killer);
                    if (teamIt != d.communityTeams.end()) {
                        const int team = teamIt->second;
                        if (++d.communityTeamScore[team] >= 30) {
                            d.communityRoundOver = true;
                            const std::string message = "Team " + std::to_string(team + 1) +
                                " wins Team Deathmatch (30 kills)!";
                            broadcastCommunityNotification(sock, players, message, 300, totalPacketsOut);
                            d.communityRoundResetMs = now + 5000;
                        }
                    }
                } else if (d.communityMode == "free_for_all") {
                    const int score = ++d.communityScores[killer];
                    if (score >= 20) {
                        d.communityRoundOver = true;
                        const auto winner = players.find(killer);
                        const std::string name = winner == players.end() ? "Player" : winner->second.name;
                        const std::string message = name + " wins Free For All (20 kills)!";
                        broadcastCommunityNotification(sock, players, message, 300, totalPacketsOut);
                        d.communityRoundResetMs = now + 5000;
                    }
                }
            }
        }
        if (d.hasPendingManualMap && d.pendingAutomaticMap.empty()) {
            d.pendingAutomaticMap = d.pendingManualMap;
            d.pendingManualMap.clear();
            d.hasPendingManualMap = false;
            d.mapChangeCountdownStartMs = now;
        }
        if (d.autoMapRotation && d.pendingAutomaticMap.empty() && now >= d.nextMapRotationMs) {
            std::vector<std::string> candidates;
            for (const auto& candidate : d.mapPool)
                if (candidate != d.mapId && !d.usedMaps.count(candidate)) candidates.push_back(candidate);
            if (candidates.empty()) {
                d.usedMaps.clear();
                d.usedMaps.insert(d.mapId);
                for (const auto& candidate : d.mapPool)
                    if (candidate != d.mapId) candidates.push_back(candidate);
            }
            if (!candidates.empty()) {
                d.pendingAutomaticMap = candidates.front();
                d.mapChangeCountdownStartMs = now;
                Debug::warn(Debug::Category::Networking,
                    "[COMMUNITY MAP ROTATION] current=%s next=%s\n",
                    d.mapId.c_str(), d.pendingAutomaticMap.c_str());
            } else {
                d.nextMapRotationMs = now + (uint64_t)d.mapRotationMinutes * 60000ull;
            }
        }
        if (!d.pendingAutomaticMap.empty()) {
            const uint64_t elapsed = now - d.mapChangeCountdownStartMs;
            const uint64_t interval = 30000;
            const uint64_t remaining = elapsed >= interval ? 0 : interval - elapsed;
            static std::string lastNoticeMap;
            static uint32_t lastNotice = UINT32_MAX;
            if (lastNoticeMap != d.pendingAutomaticMap) {
                lastNoticeMap = d.pendingAutomaticMap;
                lastNotice = UINT32_MAX;
            }
            const uint32_t seconds = (uint32_t)((remaining + 999) / 1000);
            if (remaining > 0 && remaining <= interval &&
                (seconds == 30 || seconds == 5 || seconds == 3 || seconds == 2 || seconds == 1) &&
                seconds != lastNotice) {
                lastNotice = seconds;
                std::string msg = "Server changing map to " + d.pendingAutomaticMap +
                                  " in " + std::to_string(seconds) + " sec...";
                broadcastCommunityNotification(sock, players, msg, 180, totalPacketsOut);
                Debug::warn(Debug::Category::Networking, "[COMMUNITY MAP NOTICE] %s\n", msg.c_str());
            }
            if (remaining == 0) {
                const std::string next = d.pendingAutomaticMap;
                d.pendingAutomaticMap.clear();
                lastNoticeMap.clear();
                lastNotice = UINT32_MAX;
                if (reloadDuelMap(sock, d, players, world, npcWorld, next, totalPacketsOut)) {
                    d.nextMapRotationMs = now + (uint64_t)d.mapRotationMinutes * 60000ull;
                    npcSystem.destroyAll();
                    npcs.clear();
                    npcIdsAlive.clear();
                    Debug::warn(Debug::Category::Networking,
                        "[COMMUNITY MAP CHANGE] loaded=%s nextRotationMs=%llu\n",
                        next.c_str(), (unsigned long long)d.nextMapRotationMs);
                } else {
                    d.nextMapRotationMs = now + 5000;
                }
            }
        }
        return;
    }
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

        // Instant respawn near the match anchor with full HP/ammo and a fresh
        // random offset so the exact respawn spot is never predictable.
        auto victimIt = players.find(victimId);
        if (victimIt != players.end())
        {
            victimIt->second.respawnSeconds = 0.0f;
            victimIt->second.duelSpawnPos = duelSpawnPoint(d);
        }

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
            tracer.duelId = d.duelId;
            tracer.mapVersion = d.mapVersion;
            tracer.spawnAnchorVersion = d.spawnAnchorVersion;
            tracer.respawnSequence = ++d.respawnSequence;
            ++d.stateVersion;

            auto killerIt = players.find(killerId);
            if (killerIt != players.end() && killerIt->second.spawnState == ServerPlayer::Active)
            {
                const uint32_t eventId = nextReliableGameplayEventId();
                const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
                    sock, killerIt->second, &tracer, sizeof(tracer), eventId,
                    reliableGameplayEventSessionForPlayer(killerIt->second), totalPacketsOut);
                const bool sent = result == ReliableGameplayEventQueueResult::Queued;
                Debug::log(Debug::Category::Duel,
                    "[DuelPacketSend] type=DuelEnemySpawnPacket reliable=1 player=%u enemy=%u sent=%d pos=(%.3f,%.3f,%.3f)\n",
                    killerIt->second.id, victimId, (int)sent, tracer.posX, tracer.posY, tracer.posZ);
                if (sent)
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
            // If the current map has no spawn points (e.g. the host picked a
            // spawn-less map), rotate to a spawn-capable one before starting.
            if (world.spawnPoints.empty())
                rotateToNextDuelMap(sock, d, players, world, npcWorld, totalPacketsOut);
            // Drop the practice NPC(s) once the real duel is about to start.
            npcs.clear();
            npcSystem.destroyAll();
            npcIdsAlive.clear();
            assignDuelSpawns(d, world);
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
            ++d.stateVersion;
            Debug::log(Debug::Category::Duel,
                "[ServerDuel] countdown complete duelId=%u stateVersion=%u phase=ACTIVE\n",
                d.duelId, d.stateVersion);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        // During countdown, do NOT broadcast every tick. Reliable delivery of
        // every countdown snapshot creates a huge backlog that blocks the
        // ACTIVE transition from being delivered promptly. The initial
        // countdown start is broadcast by beginDuelCountdown(); the ACTIVE
        // transition is broadcast above. Clients interpolate countdown locally.
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
            // Rotate to a fresh map we weren't just on (skips bad/spawn-less maps).
            if (d.rotateMaps && d.mapPool.size() > 1)
                rotateToNextDuelMap(sock, d, players, world, npcWorld, totalPacketsOut);
            // Each new match picks a fresh random anchor (fights spread around).
            assignDuelSpawns(d, world);
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
