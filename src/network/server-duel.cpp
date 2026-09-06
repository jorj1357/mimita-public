// 09 01 2026, 00 00
/* purpose
* Implements the authoritative PvP duel state machine on the server.
* Waits for two players, runs a single countdown, scores first-to-goal,
* instant-respawns victims at their team spawn, and auto-rematches after the
* post-match window while both players are still connected.
* Also supports FFA and TDM match modes with multi-player scoring.
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
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"
#include "network/community-server-config.h"
#include "gamemode/gamemode.h"
#include "persistence/persistence-queue.h"
#include "persistence/persistence-events.h"

namespace MimitaNet {

ServerDuelState& serverDuelState()
{
    static ServerDuelState state;
    return state;
}

void serverStartMode(const ServerDuelState& rules)
{
    ServerDuelState& d = serverDuelState();
    d.enabled = true;
    d.mapOnly = false;
    d.mode = rules.matchMode == "tdm" ? ServerMode::TeamDeathmatch
        : rules.matchMode == "ffa" ? ServerMode::FreeForAll
        : rules.matchMode == "duel" ? ServerMode::Duel : ServerMode::Sandbox;
    d.communityMode = "sandbox";
    d.communityWeaponSetId = 1;
    d.communityScores.clear();
    d.communityTeams.clear();
    d.communityTeamScore[0] = d.communityTeamScore[1] = 0;
    d.communityRoundOver = false;
    d.communityRoundResetMs = 0;
    d.phase = DUEL_PHASE_WAITING;
    d.stateBroadcastPending = false;
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
    d.usedMaps.insert(rules.mapId);
    d.hasPendingManualMap = false;
    d.pendingManualMap.clear();
    d.autoMapRotation = false;
    d.mapRotationMinutes = 15;
    d.nextMapRotationMs = 0;
    d.mapChangeCountdownStartMs = 0;
    d.pendingAutomaticMap.clear();

    // ── FFA/TDM fields ─────────────────────────────────────────────
    d.matchMode = rules.matchMode;
    d.countdownStartTick = 0;
    d.matchStartTick = 0;
    d.matchTimeLimitTick = 0;
    d.phaseTimer = 0.0f;
    d.intermissionSeconds = rules.intermissionSeconds;
    d.resultsSeconds = rules.resultsSeconds;
    d.timeLimitSeconds = rules.timeLimitSeconds;
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    d.redTeamKills = 0;
    d.blueTeamKills = 0;
    d.matchTeams.clear();
    d.participants.clear();
    d.victoryType = 0;
    d.winnerTeam = -1;
    d.killEventCounter = 0;

    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] enabled mode=%s goal=%d countdown=%.1fs rematch=%.1fs teams=%s/%s rotate=%d pool=%zu offset=%.1f timeLimit=%d intermission=%d results=%d\n",
        d.matchMode.c_str(), d.goalValue, d.countdownSeconds, d.rematchSeconds,
        d.teamAName.c_str(), d.teamBName.c_str(), (int)d.rotateMaps, d.mapPool.size(),
        d.spawnOffsetRadius, d.timeLimitSeconds, (int)d.intermissionSeconds, (int)d.resultsSeconds);
}

void serverDuelStart(const ServerDuelState& rules)
{
    serverStartMode(rules);
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
    serverStartMode(rules);
    ServerDuelState& state = serverDuelState();
    state.mapOnly = true;
    state.mode = ServerMode::Sandbox;
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
    const bool communityMode = state.communityMode == "sandbox"
        || state.communityMode == "free_for_all"
        || state.communityMode == "team_deathmatch";
    if (!communityMode) return true;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    return config.weaponAllowed(state.communityWeaponSetId, weaponId);
}

int serverCommunityWeaponNativeSlot(int logicalSlot)
{
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    const std::string* id = config.weaponForSlot(serverDuelState().communityWeaponSetId, logicalSlot);
    if (!id) return logicalSlot;
    const WeaponDefinition* def = WeaponRegistry::instance().get(*id);
    return def ? def->slot : -1;
}

int serverCommunityWeaponLogicalSlot(const std::string& weaponId)
{
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    const int slot = config.slotForWeapon(serverDuelState().communityWeaponSetId, weaponId);
    return slot > 0 ? slot : -1;
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

void serverCommunityStartMatch(bool skipIntermission)
{
    ServerDuelState& d = serverDuelState();
    if (!d.enabled) return;

    // Set match mode from community mode
    if (d.communityMode == "free_for_all")
    {
        d.matchMode = "ffa";
        d.mode = ServerMode::FreeForAll;
    }
    else if (d.communityMode == "team_deathmatch")
    {
        d.matchMode = "tdm";
        d.mode = ServerMode::TeamDeathmatch;
    }
    else if (d.communityMode == "bomb_tag")
    {
        d.matchMode = "bombtag";
        d.mode = ServerMode::Sandbox;  // Bomb tag uses Sandbox base mode
    }
    else
        return;  // sandbox mode doesn't have a match to start

    // Load gamemode config for timing
    const Gamemode& gm = GamemodeRegistry::instance().get(d.matchMode);
    d.goalValue = gm.goalValue;
    d.timeLimitSeconds = gm.timeLimitSeconds;
    d.intermissionSeconds = (float)gm.intermissionSeconds;
    d.resultsSeconds = (float)gm.resultsSeconds;
    d.countdownSeconds = gm.countdownSeconds;
    d.goSeconds = gm.goSeconds;
    d.spawnOffsetRadius = gm.spawnOffsetRadius;

    // modestart enters the configured intermission. modestartnow enters the
    // existing pre-match handoff with a zero timer; serverDuelTick owns the
    // participant assignment and authoritative 3-2-1 countdown.
    d.mapOnly = false;
    d.lastBroadcastTick = 0;
    d.stateBroadcastPending = true;
    d.rotateMaps = d.autoMapRotation;
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    d.matchTeams.clear();
    d.participants.clear();
    d.redTeamKills = 0;
    d.blueTeamKills = 0;
    d.spawnsAssigned = false;
    d.startCountdownImmediately = skipIntermission;
    d.phase = skipIntermission ? DUEL_PHASE_PRE_MATCH : DUEL_PHASE_INTERMISSION;
    d.phaseTimer = skipIntermission ? 0.0f : d.intermissionSeconds;
    d.matchOver = false;
    d.winnerPlayerId = 0;
    d.winnerTeam = -1;
    ++d.stateVersion;
    ++d.duelId;

    Debug::warn(Debug::Category::Duel,
        "[MODESTART] mode=%s matchMode=%s phase=%s goal=%d timeLimit=%d intermission=%.0f\n",
        d.communityMode.c_str(), d.matchMode.c_str(),
        skipIntermission ? "COUNTDOWN_PENDING" : "INTERMISSION",
        d.goalValue,
        d.timeLimitSeconds, d.intermissionSeconds);
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
    pkt.phaseTimer = d.phaseTimer;
    pkt.rematchLeft = d.rematchLeft;
    pkt.playerAId = d.playerAId;
    pkt.playerBId = d.playerBId;
    pkt.winnerPlayerId = d.winnerPlayerId;
    std::strncpy(pkt.teamAName, d.teamAName.c_str(), sizeof(pkt.teamAName) - 1);
    std::strncpy(pkt.teamBName, d.teamBName.c_str(), sizeof(pkt.teamBName) - 1);

    // ── FFA/TDM extension fields ───────────────────────────────────
    std::strncpy(pkt.matchMode, d.matchMode.c_str(), sizeof(pkt.matchMode) - 1);
    pkt.matchStartTick = d.matchStartTick;
    pkt.serverTick = d.currentServerTick;
    pkt.victoryType = d.victoryType;
    pkt.redTeamKills = d.redTeamKills;
    pkt.blueTeamKills = d.blueTeamKills;
    pkt.timeLimitSeconds = d.timeLimitSeconds;
    pkt.intermissionSeconds = (int32_t)d.intermissionSeconds;
    pkt.resultsSeconds = (int32_t)d.resultsSeconds;

    // FFA top-3 leaderboard
    if (d.matchMode == "ffa") {
        // Sort players by kills descending
        std::vector<std::pair<uint32_t, int>> sorted;
        for (const auto& kv : d.ffaKills)
            sorted.push_back({kv.first, kv.second});
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });
        for (int i = 0; i < 3 && i < (int)sorted.size(); ++i) {
            pkt.ffaLeaderIds[i] = sorted[i].first;
            pkt.ffaLeaderScores[i] = sorted[i].second;
            auto nameIt = players.find(sorted[i].first);
            if (nameIt != players.end())
                std::strncpy(pkt.ffaLeaderNames[i], nameIt->second.name.c_str(), sizeof(pkt.ffaLeaderNames[i]) - 1);
        }
    }

    // Participant IDs and teams
    pkt.participantCount = (uint8_t)std::min((size_t)32, d.participants.size());
    for (uint8_t i = 0; i < pkt.participantCount; ++i) {
        pkt.participantIds[i] = d.participants[i];
        auto teamIt = d.matchTeams.find(d.participants[i]);
        pkt.participantTeams[i] = teamIt != d.matchTeams.end() ? (uint8_t)teamIt->second : 0xFF;
    }

    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const uint32_t eventId = nextReliableGameplayEventId();
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
        const bool sent = result == ReliableGameplayEventQueueResult::Queued;
        Debug::log(Debug::Category::Duel,
            "[ServerDuel] sent duel state duelId=%u version=%u phase=%u mode=%s map=%s player=%u sent=%d score=%d-%d red=%d blue=%d\n",
            d.duelId, d.stateVersion, (unsigned)d.phase, d.matchMode.c_str(), d.mapId.c_str(),
            kv.second.id, (int)sent, d.scoreA, d.scoreB, d.redTeamKills, d.blueTeamKills);
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
            assignDuelSpawns(d, world);
            broadcastMapChange(sock, d, cand, players, totalPacketsOut);
            teleportDuelistsToSpawns(d, players);
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

// ── FFA/TDM match helpers ───────────────────────────────────────────────

void assignMatchParticipants(ServerDuelState& d,
                             const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    d.participants.clear();
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    d.matchTeams.clear();
    d.redTeamKills = 0;
    d.blueTeamKills = 0;

    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active) {
            d.participants.push_back(kv.first);
            d.ffaKills[kv.first] = 0;
            d.ffaDeaths[kv.first] = 0;
        }
    }

    // Sort by ID for deterministic team assignment
    std::sort(d.participants.begin(), d.participants.end());

    if (d.matchMode == "tdm") {
        for (size_t i = 0; i < d.participants.size(); ++i) {
            d.matchTeams[d.participants[i]] = (int)(i % 2);
        }
        Debug::log(Debug::Category::Duel,
            "[FFA/TDM] Assigned %zu players to teams (red=%d blue=%d)\n",
            d.participants.size(), d.redTeamKills, d.blueTeamKills);
    }
}

void teleportAllParticipantsToSpawns(ServerDuelState& d,
                                     std::unordered_map<uint32_t, ServerPlayer>& players)
{
    for (uint32_t pid : d.participants) {
        auto it = players.find(pid);
        if (it == players.end()) continue;
        ServerPlayer& p = it->second;
        const glm::vec3 spawn = duelSpawnPoint(d);
        p.duelSpawnPos = spawn;
        p.hasDuelSpawnPos = true;
        p.respawnSeconds = 0.0f;
        if (!p.dead) {
            beginAuthoritativeTransform(p, spawn, glm::vec3(0.0f), p.yaw, "match-spawn");
            p.justRespawned = true;
        }
        Debug::log(Debug::Category::Duel,
            "[MatchSpawn] player=%u spawn=(%.3f,%.3f,%.3f)\n",
            pid, spawn.x, spawn.y, spawn.z);
    }
}

void respawnAllParticipants(ServerDuelState& d,
                            std::unordered_map<uint32_t, ServerPlayer>& players)
{
    for (uint32_t pid : d.participants) {
        auto it = players.find(pid);
        if (it == players.end()) continue;
        ServerPlayer& p = it->second;
        p.duelSpawnPos = duelSpawnPoint(d);
        p.hasDuelSpawnPos = true;
        p.respawnSeconds = 0.0f;
    }
}

void resetMatchScores(ServerDuelState& d)
{
    d.scoreA = 0;
    d.scoreB = 0;
    d.redTeamKills = 0;
    d.blueTeamKills = 0;
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    for (uint32_t pid : d.participants) {
        d.ffaKills[pid] = 0;
        d.ffaDeaths[pid] = 0;
    }
}

void beginMatchCountdown(ServerDuelState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         uint32_t currentTick)
{
    ++d.duelId;
    ++d.respawnSequence;
    ++d.stateVersion;
    d.matchOver = false;
    d.winnerPlayerId = 0;
    d.winnerTeam = -1;
    d.victoryType = 0;
    d.countdownStartTick = currentTick;
    d.matchStartTick = currentTick + (uint32_t)(d.countdownSeconds * 60.0f);
    d.countdown = d.countdownSeconds;
    d.matchTimeLimitTick = 0;
    resetMatchScores(d);
    d.phase = DUEL_PHASE_COUNTDOWN;
    teleportAllParticipantsToSpawns(d, players);
    Debug::log(Debug::Category::Duel,
        "[ServerMatch] countdown started mode=%s duelId=%u matchStartTick=%u timeLimitTick=%u participants=%zu\n",
        d.matchMode.c_str(), d.duelId, d.matchStartTick, d.matchTimeLimitTick, d.participants.size());
}

static void emitDuelMatchPersistence(ServerDuelState& d, uint32_t tick,
                                      const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    PersistenceMatchEvent event;
    event.eventId = "match_" + std::to_string(tick) + "_" + std::to_string(d.duelId);
    event.matchId = "match_" + std::to_string(d.duelId);
    event.mode = d.matchMode;
    event.victoryType = d.victoryType == 0 ? "score_limit" : "time_limit";
    event.redScore = d.redTeamKills;
    event.blueScore = d.blueTeamKills;
    event.winnerTeam = d.winnerTeam == 0 ? "red" : "blue";
    event.winnerPlayerId = (int64_t)d.winnerPlayerId;

    for (auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active) continue;
        PersistenceMatchParticipant p;
        p.userId = kv.second.accountId > 0 ? (int64_t)kv.second.accountId : 0;
        p.username = kv.second.name;
        auto teamIt = d.matchTeams.find(kv.first);
        p.team = (teamIt != d.matchTeams.end() && teamIt->second == 0) ? "red" : "blue";
        p.kills = kv.second.kills;
        p.deaths = kv.second.deaths;

        if (d.matchMode == "free_for_all") {
            auto killIt = d.ffaKills.find(kv.first);
            p.kills = killIt != d.ffaKills.end() ? killIt->second : 0;
            auto deathIt = d.ffaDeaths.find(kv.first);
            p.deaths = deathIt != d.ffaDeaths.end() ? deathIt->second : 0;
            p.won = (kv.first == d.winnerPlayerId);
        } else if (d.matchMode == "team_deathmatch") {
            auto killIt = d.ffaKills.find(kv.first);
            p.kills = killIt != d.ffaKills.end() ? killIt->second : 0;
            auto deathIt = d.ffaDeaths.find(kv.first);
            p.deaths = deathIt != d.ffaDeaths.end() ? deathIt->second : 0;
            p.won = (p.team == event.winnerTeam);
        } else {
            p.won = (kv.first == d.winnerPlayerId);
        }
        event.participants.push_back(std::move(p));
    }

    PersistenceQueue::instance().enqueueMatchResult(event);
    Debug::warn(Debug::Category::Duel,
        "[PERSISTENCE] Match result emitted: mode=%s winner=%s participants=%zu\n",
        event.mode.c_str(),
        event.winnerTeam.empty() ? std::to_string(d.winnerPlayerId).c_str() : event.winnerTeam.c_str(),
        event.participants.size());
}

void checkMatchWinConditions(ServerDuelState& d, uint32_t tick,
                             SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             uint64_t& totalPacketsOut)
{
    if (d.matchMode == "ffa") {
        for (const auto& kv : d.ffaKills) {
            if (kv.second >= d.goalValue) {
                d.matchOver = true;
                d.phase = DUEL_PHASE_RESULTS;
                d.winnerPlayerId = kv.first;
                d.victoryType = 0;  // ScoreLimit
                d.phaseTimer = d.resultsSeconds;
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                emitDuelMatchPersistence(d, tick, players);
                Debug::warn(Debug::Category::Duel,
                    "[DUEL SERVER] FFA match over winner=%u score=%d goal=%d\n",
                    kv.first, kv.second, d.goalValue);
                return;
            }
        }
    } else if (d.matchMode == "tdm") {
        if (d.redTeamKills >= d.goalValue || d.blueTeamKills >= d.goalValue) {
            d.matchOver = true;
            d.phase = DUEL_PHASE_RESULTS;
            d.winnerTeam = d.redTeamKills >= d.blueTeamKills ? 0 : 1;
            d.victoryType = 0;  // ScoreLimit
            d.phaseTimer = d.resultsSeconds;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            emitDuelMatchPersistence(d, tick, players);
            Debug::warn(Debug::Category::Duel,
                "[DUEL SERVER] TDM match over winnerTeam=%d red=%d blue=%d goal=%d\n",
                d.winnerTeam, d.redTeamKills, d.blueTeamKills, d.goalValue);
            return;
        }
    }

    // Time limit check
    if (d.matchTimeLimitTick > 0 && tick >= d.matchTimeLimitTick) {
        d.matchOver = true;
        d.phase = DUEL_PHASE_RESULTS;
        d.victoryType = 1;  // TimeLimit
        d.phaseTimer = d.resultsSeconds;
        if (d.matchMode == "ffa") {
            int best = -1;
            for (const auto& kv : d.ffaKills) {
                if (kv.second > best) {
                    best = kv.second;
                    d.winnerPlayerId = kv.first;
                }
            }
            emitDuelMatchPersistence(d, tick, players);
        } else if (d.matchMode == "tdm") {
            d.winnerTeam = d.redTeamKills >= d.blueTeamKills ? 0 : 1;
            emitDuelMatchPersistence(d, tick, players);
        }
        ++d.stateVersion;
        broadcastDuelState(sock, d, players, totalPacketsOut);
        Debug::warn(Debug::Category::Duel,
            "[DUEL SERVER] Time limit reached mode=%s red=%d blue=%d\n",
            d.matchMode.c_str(), d.redTeamKills, d.blueTeamKills);
    }
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
    d.currentServerTick = tick;
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
                std::mt19937 rng(std::random_device{}());
                std::shuffle(candidates.begin(), candidates.end(), rng);
                d.pendingAutomaticMap = candidates.front();
                d.mapChangeCountdownStartMs = now;
                Debug::warn(Debug::Category::Networking,
                    "[COMMUNITY MAP ROTATION] current=%s next=%s candidates=%zu (randomized)\n",
                    d.mapId.c_str(), d.pendingAutomaticMap.c_str(), candidates.size());
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
            // Duel 1v1 scoring (original behavior)
            if (d.matchMode == "duel") {
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
                    emitDuelMatchPersistence(d, tick, players);
                    Debug::warn(Debug::Category::Duel,
                        "[DUEL SERVER] match over winner=%u score=%d-%d goal=%d\n",
                        d.winnerPlayerId, d.scoreA, d.scoreB, d.goalValue);
                }
            }
            // FFA scoring
            else if (d.matchMode == "ffa") {
                ++d.ffaKills[killerId];
                ++d.ffaDeaths[victimId];
                // Win condition checked in checkMatchWinConditions
            }
            // TDM scoring
            else if (d.matchMode == "tdm") {
                ++d.ffaKills[killerId];
                ++d.ffaDeaths[victimId];
                auto teamIt = d.matchTeams.find(killerId);
                if (teamIt != d.matchTeams.end()) {
                    int victimTeam = -1;
                    auto vtIt = d.matchTeams.find(victimId);
                    if (vtIt != d.matchTeams.end()) victimTeam = vtIt->second;
                    // Only score if killer and victim are on different teams
                    if (victimTeam >= 0 && teamIt->second != victimTeam) {
                        if (teamIt->second == 0) ++d.redTeamKills;
                        else ++d.blueTeamKills;
                    }
                }
            }
        }

        broadcastDuelState(sock, d, players, totalPacketsOut);
    }

    // ── FFA/TDM match mode state machine ────────────────────────────
    if (d.matchMode == "ffa" || d.matchMode == "tdm")
    {
        if (d.stateBroadcastPending)
        {
            // The command changed the authoritative mode/phase between ticks.
            // Send that state now so every client can show intermission or the
            // immediate countdown without waiting for the periodic broadcast.
            broadcastDuelState(sock, d, players, totalPacketsOut);
            d.stateBroadcastPending = false;
        }
        switch (d.phase)
        {
        case DUEL_PHASE_WAITING:
            if (countActivePlayers(players) >= 2)
            {
                // If the current map has no spawn points, rotate.
                if (world.spawnPoints.empty())
                    rotateToNextDuelMap(sock, d, players, world, npcWorld, totalPacketsOut);
                // Drop practice NPCs.
                npcs.clear();
                npcSystem.destroyAll();
                npcIdsAlive.clear();
                assignDuelSpawns(d, world);
                assignMatchParticipants(d, players);
                d.phase = DUEL_PHASE_PRE_MATCH;
                d.phaseTimer = 3.0f;
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::warn(Debug::Category::Duel,
                    "[FFA/TDM] Pre-match started mode=%s participants=%zu\n",
                    d.matchMode.c_str(), d.participants.size());
            }
            break;

        case DUEL_PHASE_PRE_MATCH:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                if (d.startCountdownImmediately) {
                    d.startCountdownImmediately = false;
                    assignDuelSpawns(d, world);
                    assignMatchParticipants(d, players);
                }
                beginMatchCountdown(d, players, tick);
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            else if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_COUNTDOWN:
            if (tick >= d.matchStartTick)
            {
                d.phase = DUEL_PHASE_GO;
                d.phaseTimer = d.goSeconds;
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] GO shown mode=%s tick=%u duration=%.2f\n",
                    d.matchMode.c_str(), tick, d.goSeconds);
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_GO:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                d.phase = DUEL_PHASE_ACTIVE;
                d.matchStartTick = tick;
                if (d.timeLimitSeconds > 0)
                    d.matchTimeLimitTick = tick + (uint32_t)(d.timeLimitSeconds * 60.0f);
                respawnAllParticipants(d, players);
                ++d.stateVersion;
                d.lastBroadcastTick = tick;
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] Match ACTIVE mode=%s tick=%u\n",
                    d.matchMode.c_str(), tick, d.matchStartTick);
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_ACTIVE:
            // Check win conditions on every tick
            checkMatchWinConditions(d, tick, sock, players, totalPacketsOut);
            if (d.phase != DUEL_PHASE_ACTIVE) break;  // win condition triggered
            // Periodic broadcast
            if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_RESULTS:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                d.phase = DUEL_PHASE_INTERMISSION;
                d.phaseTimer = d.intermissionSeconds;
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] Intermission started mode=%s duration=%.0f\n",
                    d.matchMode.c_str(), d.intermissionSeconds);
            }
            else if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        case DUEL_PHASE_INTERMISSION:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                // Rotate map if configured
                if (d.rotateMaps && d.mapPool.size() > 1)
                    rotateToNextDuelMap(sock, d, players, world, npcWorld, totalPacketsOut);
                assignDuelSpawns(d, world);
                assignMatchParticipants(d, players);
                d.phase = DUEL_PHASE_PRE_MATCH;
                d.phaseTimer = 3.0f;
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] Pre-match started mode=%s participants=%zu\n",
                    d.matchMode.c_str(), d.participants.size());
            }
            else if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
                broadcastDuelState(sock, d, players, totalPacketsOut);
            }
            break;

        default:
            break;
        }
        return;
    }

    // ── Bomb Tag match mode state machine ──────────────────────────
    if (d.matchMode == "bombtag")
    {
        if (d.stateBroadcastPending)
        {
            broadcastDuelState(sock, d, players, totalPacketsOut);
            d.stateBroadcastPending = false;
        }
        serverBombTagTick(sock, players, world, npcs, npcSystem, tick, totalPacketsOut);
        return;
    }

    // ── Original duel 1v1 state machine ─────────────────────────────
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

    default:
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

// ── Bomb Tag ──────────────────────────────────────────────────────────

namespace {

// Shuffle bag for bomb holder selection. Ensures every eligible player
// is selected once before the bag is reshuffled.
struct BombShuffleBag {
    std::vector<uint32_t> order;
    int position = 0;

    void build(const std::vector<uint32_t>& eligible) {
        order = eligible;
        // Fisher-Yates shuffle
        for (int i = (int)order.size() - 1; i > 0; --i) {
            int j = (int)((uint32_t)std::rand() % (uint32_t)(i + 1));
            std::swap(order[i], order[j]);
        }
        position = 0;
    }

    uint32_t next() {
        if (order.empty()) return 0;
        if (position >= (int)order.size()) {
            // Bag exhausted — rebuild with current eligible list
            // Caller must call build() before next() if they want a fresh bag.
            // If called without build(), just wrap around.
            position = 0;
        }
        return order[position++];
    }

    void remove(uint32_t id) {
        auto it = std::find(order.begin() + position, order.end(), id);
        if (it != order.end()) {
            order.erase(it);
            if (position >= (int)order.size() && !order.empty())
                position = 0;
        }
    }
};

BombShuffleBag sBombShuffleBag;

// Simple sphere overlap test for bomb contact detection on the server.
// Uses player body-part sphere positions if available, otherwise root position.
bool bombSphereOverlap(const glm::vec3& aPos, float aRadius,
                       const glm::vec3& bPos, float bRadius)
{
    float dist = glm::length(aPos - bPos);
    return dist < (aRadius + bRadius);
}

// Get a rough position for an entity (root position for players, body.pos for NPCs).
glm::vec3 getEntityRootPos(const ServerPlayer& p) {
    return p.pos;
}

glm::vec3 getEntityRootPos(const ServerNpc& n) {
    return n.pos;
}

// Find the bomb holder's world position from server state.
glm::vec3 getBombHolderPosition(const ServerDuelState& d,
                                const std::unordered_map<uint32_t, ServerPlayer>& players,
                                const std::unordered_map<uint32_t, ServerNpc>& npcs)
{
    if (d.bombOwnerType == 1 /* player */) {
        auto it = players.find(d.bombOwnerPlayerId);
        if (it != players.end()) return getEntityRootPos(it->second);
    } else if (d.bombOwnerType == 2 /* npc */) {
        auto it = npcs.find((uint32_t)d.bombOwnerNpcIndex);
        if (it != npcs.end()) return getEntityRootPos(it->second);
    }
    return glm::vec3(0.0f);
}

// Select a new bomb holder using the shuffle bag.
void selectNewBombHolder(ServerDuelState& d,
                         const std::unordered_map<uint32_t, ServerPlayer>& players)
{
    std::vector<uint32_t> eligible;
    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active && !kv.second.dead)
            eligible.push_back(kv.first);
    }
    if (eligible.empty()) {
        d.bombOwnerType = 0;
        d.bombOwnerPlayerId = 0;
        d.bombOwnerNpcIndex = 0;
        return;
    }
    // Check if current bag is valid for current eligible set
    bool needRebuild = sBombShuffleBag.order.empty();
    if (!needRebuild) {
        // Check if all remaining bag entries are still eligible
        for (uint32_t id : sBombShuffleBag.order) {
            bool found = false;
            for (uint32_t e : eligible) {
                if (e == id) { found = true; break; }
            }
            if (!found) { needRebuild = true; break; }
        }
    }
    if (needRebuild) {
        sBombShuffleBag.build(eligible);
    }
    uint32_t chosen = sBombShuffleBag.next();
    if (chosen == 0) {
        // Fallback: random pick
        chosen = eligible[(uint32_t)std::rand() % eligible.size()];
    }
    d.bombOwnerType = 1;
    d.bombOwnerPlayerId = chosen;
    d.bombOwnerNpcIndex = 0;

    Debug::log(Debug::Category::Duel,
        "[BOMB TAG] bomb assigned to player=%u eligible=%zu\n",
        chosen, eligible.size());
}

// Broadcast bomb tag state to all active clients.
void broadcastBombTagState(SOCKET sock,
                           ServerDuelState& d,
                           const std::unordered_map<uint32_t, ServerPlayer>& players,
                           uint64_t& totalPacketsOut)
{
    BombTagStatePacket pkt{};
    pkt.header.type = PACKET_BOMB_TAG_STATE;
    pkt.header.tick = 0;
    pkt.duelId = d.duelId;
    pkt.stateVersion = d.stateVersion;
    pkt.phase = d.phase;
    pkt.bombOwnerType = d.bombOwnerType;
    pkt.bombOwnerPlayerId = d.bombOwnerPlayerId;
    pkt.bombOwnerNpcIndex = d.bombOwnerNpcIndex;
    pkt.timerTicksRemaining = d.bombTimerTicks;
    pkt.inactiveTicksRemaining = d.bombInactiveTicks;
    pkt.serverTick = d.currentServerTick;

    glm::vec3 bombPos = getBombHolderPosition(d, players, {});
    pkt.bombPosX = bombPos.x;
    pkt.bombPosY = bombPos.y;
    pkt.bombPosZ = bombPos.z;

    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const uint32_t eventId = nextReliableGameplayEventId();
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
        Debug::log(Debug::Category::Duel,
            "[BOMB TAG] sent state player=%u phase=%u owner=%u timer=%u inactive=%u\n",
            kv.second.id, d.phase, d.bombOwnerPlayerId, d.bombTimerTicks, d.bombInactiveTicks);
    }
}

// Broadcast pass visualization event to all clients.
void broadcastBombTagPass(SOCKET sock,
                          ServerDuelState& d,
                          const std::unordered_map<uint32_t, ServerPlayer>& players,
                          uint32_t oldOwnerPlayerId, uint32_t newOwnerPlayerId,
                          const glm::vec3& oldPos, const glm::vec3& newPos,
                          float passDist, float rewoundDist,
                          uint64_t& totalPacketsOut)
{
    BombTagPassEventPacket pkt{};
    pkt.header.type = PACKET_BOMB_TAG_PASS_EVENT;
    pkt.header.tick = 0;
    pkt.eventId = nextReliableGameplayEventId();
    pkt.eventSessionId = serverReliableEventSessionId();
    pkt.serverTick = d.currentServerTick;
    pkt.oldOwnerPlayerId = oldOwnerPlayerId;
    pkt.newOwnerPlayerId = newOwnerPlayerId;
    pkt.oldBombPosX = oldPos.x;
    pkt.oldBombPosY = oldPos.y;
    pkt.oldBombPosZ = oldPos.z;
    pkt.newBombPosX = newPos.x;
    pkt.newBombPosY = newPos.y;
    pkt.newBombPosZ = newPos.z;
    pkt.passDistance = passDist;
    pkt.serverRewoundDistance = rewoundDist;
    pkt.accepted = 1;

    for (const auto& kv : players) {
        if (kv.second.spawnState != ServerPlayer::Active)
            continue;
        const ReliableGameplayEventQueueResult result = queueReliableGameplayEventToPlayer(
            sock, const_cast<ServerPlayer&>(kv.second), &pkt, sizeof(pkt), pkt.eventId,
            reliableGameplayEventSessionForPlayer(const_cast<ServerPlayer&>(kv.second)), totalPacketsOut);
    }
    ++d.bombPassCounter;
    Debug::log(Debug::Category::Duel,
        "[BOMB TAG PASS] old=%u new=%u dist=%.2f rewound=%.2f passes=%u\n",
        oldOwnerPlayerId, newOwnerPlayerId, passDist, rewoundDist, d.bombPassCounter);
}

} // anonymous namespace

void serverBombTagStartMatch(bool skipIntermission)
{
    ServerDuelState& d = serverDuelState();
    if (!d.enabled) return;

    // Load bomb tag gamemode config
    const Gamemode& gm = GamemodeRegistry::instance().get("bombtag");
    d.bombTimerTicksMax = (uint32_t)(gm.bombTimerTicks > 0 ? gm.bombTimerTicks : 900);
    d.bombInactiveTicksMax = (uint32_t)(gm.inactiveTicks > 0 ? gm.inactiveTicks : 60);
    d.bombBlinkTicks = (uint32_t)(gm.blinkTicks > 0 ? gm.blinkTicks : 30);
    d.bombMaxPassSanityDist = gm.maxPassSanityDistance > 0.0f ? gm.maxPassSanityDistance : 3.0f;
    d.bombTagActive = true;
    d.bombPassCounter = 0;
    d.bombExplosionCounter = 0;
    d.bombTimerTicks = d.bombTimerTicksMax;
    d.bombInactiveTicks = 0;
    d.bombOwnerType = 0;
    d.bombOwnerPlayerId = 0;
    d.bombOwnerNpcIndex = 0;

    // Standard match lifecycle
    d.countdownSeconds = gm.countdownSeconds;
    d.goSeconds = gm.goSeconds;
    d.intermissionSeconds = (float)gm.intermissionSeconds;
    d.resultsSeconds = (float)gm.resultsSeconds;
    d.timeLimitSeconds = 0;  // infinite
    d.mapOnly = false;
    d.lastBroadcastTick = 0;
    d.stateBroadcastPending = true;
    d.phase = skipIntermission ? DUEL_PHASE_PRE_MATCH : DUEL_PHASE_INTERMISSION;
    d.phaseTimer = skipIntermission ? 0.0f : d.intermissionSeconds;
    d.matchOver = false;
    d.spawnOffsetRadius = gm.spawnOffsetRadius;
    ++d.stateVersion;
    ++d.duelId;

    Debug::warn(Debug::Category::Duel,
        "[BOMB TAG] match starting timerTicks=%u inactiveTicks=%u blinkTicks=%u sanityDist=%.1f\n",
        d.bombTimerTicksMax, d.bombInactiveTicksMax, d.bombBlinkTicks, d.bombMaxPassSanityDist);
}

void serverBombTagTick(SOCKET sock,
                       std::unordered_map<uint32_t, ServerPlayer>& players,
                       HeadlessWorld& world,
                       std::unordered_map<uint32_t, ServerNpc>& npcs,
                       NpcSystem& npcSystem,
                       uint32_t tick,
                       uint64_t& totalPacketsOut)
{
    ServerDuelState& d = serverDuelState();
    if (!d.enabled || !d.bombTagActive) return;
    d.currentServerTick = tick;

    // ── Handle pending kill from explosion ───────────────────────────
    if (d.hasPendingKill)
    {
        d.hasPendingKill = false;
        const uint32_t victimId = d.pendingVictimId;
        // Instant respawn at spawn point
        auto victimIt = players.find(victimId);
        if (victimIt != players.end()) {
            victimIt->second.respawnSeconds = 0.0f;
            victimIt->second.duelSpawnPos = duelSpawnPoint(d);
        }
    }

    // ── State machine ────────────────────────────────────────────────
    switch (d.phase) {
    case DUEL_PHASE_WAITING:
        if (countActivePlayers(players) >= 1) {
            // Start bomb tag with available players
            if (world.spawnPoints.empty())
                assignDuelSpawns(d, world);
            npcs.clear();
            npcSystem.destroyAll();
            assignMatchParticipants(d, players);
            d.phase = DUEL_PHASE_PRE_MATCH;
            d.phaseTimer = 3.0f;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
            Debug::warn(Debug::Category::Duel,
                "[BOMB TAG] pre-match started participants=%zu\n", d.participants.size());
        }
        break;

    case DUEL_PHASE_PRE_MATCH:
        d.phaseTimer -= SERVER_DT;
        if (d.phaseTimer <= 0.0f) {
            if (d.startCountdownImmediately) {
                d.startCountdownImmediately = false;
                assignDuelSpawns(d, world);
                assignMatchParticipants(d, players);
            }
            beginMatchCountdown(d, players, tick);
            // Initialize bomb timer
            d.bombTimerTicks = d.bombTimerTicksMax;
            d.bombInactiveTicks = 0;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        } else if (tick - d.lastBroadcastTick >= 60) {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_COUNTDOWN:
        if (tick >= d.matchStartTick) {
            d.phase = DUEL_PHASE_GO;
            d.phaseTimer = d.goSeconds;
            ++d.stateVersion;
            d.lastBroadcastTick = tick;
            Debug::log(Debug::Category::Duel,
                "[BOMB TAG] GO shown tick=%u\n", tick);
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_GO:
        d.phaseTimer -= SERVER_DT;
        if (d.phaseTimer <= 0.0f) {
            d.phase = DUEL_PHASE_ACTIVE;
            d.matchStartTick = tick;
            d.bombTimerTicks = d.bombTimerTicksMax;
            d.bombInactiveTicks = 0;
            // Select first bomb holder
            selectNewBombHolder(d, players);
            respawnAllParticipants(d, players);
            ++d.stateVersion;
            d.lastBroadcastTick = tick;
            Debug::warn(Debug::Category::Duel,
                "[BOMB TAG] ACTIVE tick=%u holder=%u timerTicks=%u\n",
                tick, d.bombOwnerPlayerId, d.bombTimerTicks);
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_ACTIVE:
    {
        // ── Bomb timer countdown ──────────────────────────────────
        if (d.bombTimerTicks > 0)
            --d.bombTimerTicks;

        // ── Inactive grace period ─────────────────────────────────
        if (d.bombInactiveTicks > 0)
            --d.bombInactiveTicks;

        // ── Bomb explosion ────────────────────────────────────────
        if (d.bombTimerTicks == 0 && d.bombOwnerType != 0) {
            // Kill the bomb holder
            uint32_t victimId = d.bombOwnerPlayerId;
            auto victimIt = players.find(victimId);
            if (victimIt != players.end() && !victimIt->second.dead) {
                // Apply lethal damage via the normal server damage path
                victimIt->second.health = 0;
                victimIt->second.dead = true;
                victimIt->second.respawnSeconds = 0.01f;
                ++victimIt->second.deaths;

                // Credit kill to a random other player (explosion is environment)
                // For bomb tag, we credit no one — bomb explosion is environmental
                d.hasPendingKill = true;
                d.pendingKillerId = 0;
                d.pendingVictimId = victimId;

                Debug::warn(Debug::Category::Duel,
                    "[BOMB TAG] explosion! victim=%u deaths=%u explosions=%u\n",
                    victimId, victimIt->second.deaths, d.bombExplosionCounter + 1);
            }
            ++d.bombExplosionCounter;

            // Select new bomb holder and reset timer
            d.bombTimerTicks = d.bombTimerTicksMax;
            d.bombInactiveTicks = 0;
            selectNewBombHolder(d, players);

            // Respawn the killed player instantly
            if (victimIt != players.end()) {
                victimIt->second.dead = false;
                victimIt->second.health = 100;
                victimIt->second.duelSpawnPos = duelSpawnPoint(d);
                victimIt->second.respawnSeconds = 0.0f;
                beginAuthoritativeTransform(victimIt->second,
                    victimIt->second.duelSpawnPos, glm::vec3(0.0f),
                    victimIt->second.yaw, "bomb-respawn");
            }

            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
            break;
        }

        // ── Physical contact detection (bomb pass) ────────────────
        if (d.bombInactiveTicks == 0 && d.bombOwnerType == 1 && !d.matchOver) {
            // Player holds bomb — check contact with other players
            auto holderIt = players.find(d.bombOwnerPlayerId);
            if (holderIt != players.end() && !holderIt->second.dead) {
                glm::vec3 holderPos = getEntityRootPos(holderIt->second);
                float bombRadius = 0.5f;
                float targetRadius = 1.5f;

                for (const auto& kv : players) {
                    if (kv.first == d.bombOwnerPlayerId) continue;
                    if (kv.second.spawnState != ServerPlayer::Active) continue;
                    if (kv.second.dead) continue;

                    glm::vec3 targetPos = getEntityRootPos(kv.second);
                    float dist = glm::length(holderPos - targetPos);

                    // Sanity distance check
                    if (dist > d.bombMaxPassSanityDist) continue;

                    // Physical overlap check
                    if (bombSphereOverlap(holderPos, bombRadius, targetPos, targetRadius)) {
                        // Transfer bomb
                        glm::vec3 oldPos = holderPos;
                        d.bombOwnerType = 1;
                        d.bombOwnerPlayerId = kv.first;
                        d.bombOwnerNpcIndex = 0;
                        d.bombInactiveTicks = d.bombInactiveTicksMax;

                        glm::vec3 newPos = getEntityRootPos(kv.second);
                        broadcastBombTagPass(sock, d, players,
                            d.bombOwnerPlayerId, kv.first,
                            oldPos, newPos, dist, dist, totalPacketsOut);
                        ++d.stateVersion;
                        broadcastBombTagState(sock, d, players, totalPacketsOut);
                        Debug::log(Debug::Category::Duel,
                            "[BOMB TAG] PASS %u -> %u dist=%.2f\n",
                            d.bombOwnerPlayerId, kv.first, dist);
                        break;
                    }
                }
            }
        }

        // ── Periodic state broadcast ──────────────────────────────
        if (tick - d.lastBroadcastTick >= 10) {
            d.lastBroadcastTick = tick;
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        }
        break;
    }

    case DUEL_PHASE_RESULTS:
        d.phaseTimer -= SERVER_DT;
        if (d.phaseTimer <= 0.0f) {
            d.phase = DUEL_PHASE_INTERMISSION;
            d.phaseTimer = d.intermissionSeconds;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        } else if (tick - d.lastBroadcastTick >= 60) {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    case DUEL_PHASE_INTERMISSION:
        d.phaseTimer -= SERVER_DT;
        if (d.phaseTimer <= 0.0f) {
            assignDuelSpawns(d, world);
            assignMatchParticipants(d, players);
            d.phase = DUEL_PHASE_PRE_MATCH;
            d.phaseTimer = 3.0f;
            d.bombTimerTicks = d.bombTimerTicksMax;
            d.bombInactiveTicks = 0;
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
        } else if (tick - d.lastBroadcastTick >= 60) {
            d.lastBroadcastTick = tick;
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        break;

    default:
        break;
    }
}

} // namespace MimitaNet
