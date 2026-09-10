// 09 01 2026, 00 00
/* purpose
* Implements the authoritative JSON-defined gamemode state machine on the server.
* Owns shared waiting, intermission, countdown, active, results, and switching phases.
* Routes duel, FFA, TDM, Bomb Tag, sandbox, and future rules through shared actor services.
* Does NOT apply damage, simulate movement, or render anything.
* Does NOT touch the client queue/matchmaking or coordinator protocol.
* Does NOT own mode presentation or mode-specific GUI layout.
*/

#include "network/server-gamemode.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "network/packets.h"
#include "network/server.h"
#include "npc/npc.h"
#include "combat/weapon-registry.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"
#include "network/community-server-config.h"
#include "gamemode/gamemode.h"
#include "persistence/persistence-queue.h"
#include "persistence/persistence-events.h"
#include "config/spawn-velocity-config.h"
#include "network/actor-lifecycle.h"

namespace MimitaNet {

static void finalizeServerNpcMirrorSpawn(ServerNpc& npc,
                                         ActorSpawnReason reason,
                                         uint32_t serverTick)
{
    ActorSpawnEvent event;
    event.entityId = npc.entityId;
    event.actorKind = ActorKind::Npc;
    event.reason = reason;
    event.transformEpoch = npc.transformEpoch;
    event.serverTick = serverTick;
    event.position = npc.pos;
    event.lookDirection = npc.aim;
    event = finalizeActorSpawn(event, npc.name.c_str());
    npc.aim = event.lookDirection;
    npc.yaw = std::atan2(event.lookDirection.y, event.lookDirection.x);
    npc.vel = event.velocity;
    npc.knockbackImpulse = glm::vec3(0.0f);
}

ServerGamemodeState& serverGamemodeState()
{
    static ServerGamemodeState state;
    return state;
}

void serverStartMode(const ServerGamemodeState& rules)
{
    ServerGamemodeState& d = serverGamemodeState();
    d.enabled = true;
    d.mapOnly = false;
    d.mode = rules.matchMode == "tdm" ? ServerMode::TeamDeathmatch
        : rules.matchMode == "ffa" ? ServerMode::FreeForAll
        : rules.matchMode == "duel" ? ServerMode::Duel : ServerMode::Sandbox;
    d.communityMode = "sandbox";
    d.communityWeaponSetId = 1;
    d.appliedCommunityWeaponSetId = 0;
    d.communityWeaponSetExplicit = false;
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
    d.participantNames.clear();
    d.victoryType = 0;
    d.winnerTeam = -1;
    d.killEventCounter = 0;
    d.pendingKillEvents.clear();

    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] enabled mode=%s goal=%d countdown=%.1fs rematch=%.1fs teams=%s/%s rotate=%d pool=%zu offset=%.1f timeLimit=%d intermission=%d results=%d\n",
        d.matchMode.c_str(), d.goalValue, d.countdownSeconds, d.rematchSeconds,
        d.teamAName.c_str(), d.teamBName.c_str(), (int)d.rotateMaps, d.mapPool.size(),
        d.spawnOffsetRadius, d.timeLimitSeconds, (int)d.intermissionSeconds, (int)d.resultsSeconds);
}

void serverGamemodeStart(const ServerGamemodeState& rules)
{
    serverStartMode(rules);
}

void serverCommunityMapStart(const std::vector<std::string>& mapPool,
                             const std::string& mapId,
                             bool autoRotation,
                             uint32_t rotationMinutes,
                             int weaponSetId)
{
    ServerGamemodeState rules;
    rules.mapPool = mapPool;
    rules.mapId = mapId;
    rules.rotateMaps = false;
    rules.mapOnly = true;
    rules.autoMapRotation = autoRotation;
    rules.mapRotationMinutes = std::clamp(rotationMinutes, 1u, 9999u);
    serverStartMode(rules);
    ServerGamemodeState& state = serverGamemodeState();
    state.mapOnly = true;
    state.mode = ServerMode::Sandbox;
    state.communityMode = "sandbox";
    state.communityWeaponSetId = std::max(1, weaponSetId);
    state.communityWeaponSetExplicit = true;
    state.autoMapRotation = rules.autoMapRotation;
    state.mapRotationMinutes = rules.mapRotationMinutes;
    state.nextMapRotationMs = nowMs() + (uint64_t)state.mapRotationMinutes * 60000ull;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY MAP RUNTIME] map=%s auto=%d intervalMinutes=%u pool=%zu\n",
        mapId.c_str(), (int)autoRotation, state.mapRotationMinutes, mapPool.size());
}

void serverCommunitySetWeaponSet(int weaponSetId)
{
    ServerGamemodeState& state = serverGamemodeState();
    if (!state.enabled || !state.mapOnly) return;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    if (!config.weaponSetById(weaponSetId)) return;
    state.communityWeaponSetId = weaponSetId;
    state.communityWeaponSetExplicit = true;
    state.appliedCommunityWeaponSetId = 0;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY WEAPON SET] selected=%d\n", state.communityWeaponSetId);
}

bool serverCommunityWeaponAllowed(const std::string& weaponId)
{
    const ServerGamemodeState& state = serverGamemodeState();
    const bool communityMode = state.communityMode == "sandbox"
        || state.communityMode == "free_for_all"
        || state.communityMode == "team_deathmatch"
        || state.hasBombFeature;
    if (!communityMode) return true;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    return config.weaponAllowed(state.communityWeaponSetId, weaponId);
}

int serverCommunityWeaponNativeSlot(int logicalSlot)
{
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    const std::string* id = config.weaponForSlot(serverGamemodeState().communityWeaponSetId, logicalSlot);
    if (!id) return logicalSlot;
    const WeaponDefinition* def = WeaponRegistry::instance().get(*id);
    return def ? def->slot : -1;
}

int serverCommunityWeaponLogicalSlot(const std::string& weaponId)
{
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.weaponSets().empty()) config.load();
    const int slot = config.slotForWeapon(serverGamemodeState().communityWeaponSetId, weaponId);
    return slot > 0 ? slot : -1;
}

void serverCommunitySetMode(const std::string& modeId)
{
    ServerGamemodeState& state = serverGamemodeState();
    if (!state.enabled || modeId.empty()) return;
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.modes().empty()) config.load();
    state.communityMode = modeId;
    state.communityScores.clear();
    state.communityTeams.clear();
    state.communityTeamScore[0] = state.communityTeamScore[1] = 0;
    state.communityRoundOver = false;
    state.communityRoundResetMs = 0;
    Debug::warn(Debug::Category::Networking,
        "[COMMUNITY MODE] selected=%s\n", state.communityMode.c_str());
}

void serverCommunityStartMatch(bool skipIntermission, const std::string& requestedMode)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;

    if (!requestedMode.empty())
        d.communityMode = requestedMode;

    // ── Look up the community mode and resolve its gamemode_id ───────
    const CommunityServerConfig& communityConfig = CommunityServerConfig::instance();
    const CommunityMode* cm = communityConfig.modeById(d.communityMode);
    if (!cm) return;  // unknown mode — cannot start

    // Use gamemode_id to look up the actual gamemode config.
    // This bridges onlinemodes.json (community menu) to gamemodes/*.json (gameplay rules).
    const std::string& resolvedGamemodeId = cm->gamemodeId;

    // Defer a live mode switch until the current mode has shown its results.
    if (!d.mapOnly && !d.matchMode.empty() && d.matchMode != resolvedGamemodeId &&
        d.phase != DUEL_PHASE_WAITING && d.phase != DUEL_PHASE_RESULTS)
    {
        d.pendingModeSwitch = true;
        d.pendingModeSwitchCountdown = skipIntermission;
        d.pendingGamemodeId = resolvedGamemodeId;
        d.phase = DUEL_PHASE_RESULTS;
        d.phaseTimer = 5.0f;
        d.matchOver = true;
        if (d.matchMode == "ffa") {
            int best = -1;
            for (const auto& kv : d.ffaKills)
                if (kv.second > best) { best = kv.second; d.winnerPlayerId = kv.first; }
        } else if (d.matchMode == "tdm") {
            d.winnerTeam = d.redTeamKills >= d.blueTeamKills ? 0 : 1;
        }
        d.stateBroadcastPending = true;
        ++d.stateVersion;
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE SWITCH] old=%s new=%s resultsSeconds=5 countdownAfter=%d\n",
            d.matchMode.c_str(), resolvedGamemodeId.c_str(), (int)skipIntermission);
        return;
    }

    // Set match mode from community mode id (used for routing and state machine)
    d.matchMode = resolvedGamemodeId;
    // DEPRECATED: the old ServerMode enum and short-form matchMode strings
    // are kept for backward compatibility with duel/FFA/TDM code paths.
    // New modes should use matchMode directly (the community mode id).
    d.mode = ServerMode::Sandbox;

    // Load gamemode config using the resolved gamemode_id
    const Gamemode& gm = GamemodeRegistry::instance().get(resolvedGamemodeId);
    d.goalValue = gm.goalValue;
    // An explicit GUI/runtime selection wins over the gamemode default.
    if (!d.communityWeaponSetExplicit && gm.weaponSetId > 0)
        d.communityWeaponSetId = gm.weaponSetId;
    d.timeLimitSeconds = gm.timeLimitSeconds;
    d.intermissionSeconds = (float)gm.intermissionSeconds;
    d.resultsSeconds = (float)gm.resultsSeconds;
    d.countdownSeconds = gm.countdownSeconds;
    d.goSeconds = gm.goSeconds;
    d.spawnOffsetRadius = gm.spawnOffsetRadius;
    d.hasBombFeature = gm.features.bombHolderText;
    d.bombTagActive = false;

    // ── Visual/settings overrides from gamemode ────────────────────
    d.cameraFov = gm.cameraFov;
    d.ragdollExplicit = gm.ragdollExplicit;
    d.ragdollEnabled = gm.ragdollEnabled;
    d.bloodExplicit = gm.bloodExplicit;
    d.bloodEnabled = gm.bloodEnabled;

    // modestart enters the configured intermission. modestartnow enters the
    // countdown directly; serverGamemodeTick owns the authoritative 3-2-1.
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
    d.pendingKillEvents.clear();
    d.hasPendingKill = false;
    d.spawnsAssigned = false;
    d.startCountdownImmediately = skipIntermission;
    d.phase = DUEL_PHASE_INTERMISSION;
    d.phaseTimer = skipIntermission ? 0.0f : d.intermissionSeconds;
    d.matchOver = false;
    d.winnerPlayerId = 0;
    d.winnerTeam = -1;
    ++d.stateVersion;
    ++d.duelId;

    // ── Mode-specific activation ─────────────────────────────────────
    // Each mode that needs extra initialization gets its entry point called here.
    // This replaces the old hardcoded if/else if chain.
    if (d.hasBombFeature) {
        serverBombTagStartMatch(skipIntermission);
    }

    Debug::warn(Debug::Category::Duel,
        "[MODESTART] community=%s gamemode=%s phase=%s goal=%d timeLimit=%d intermission=%.0f\n",
        d.communityMode.c_str(), resolvedGamemodeId.c_str(),
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
                        const ServerGamemodeState& d,
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

    // ── Gamemode visual overrides ──────────────────────────────────
    pkt.cameraFov = d.cameraFov;
    pkt.ragdollEnabled = d.ragdollExplicit ? (d.ragdollEnabled ? 2 : 1) : 0;
    pkt.bloodEnabled = d.bloodExplicit ? (d.bloodEnabled ? 2 : 1) : 0;

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
            auto nameIt = d.participantNames.find(sorted[i].first);
            if (nameIt != d.participantNames.end())
                std::strncpy(pkt.ffaLeaderNames[i], nameIt->second.c_str(), sizeof(pkt.ffaLeaderNames[i]) - 1);
            else
                std::snprintf(pkt.ffaLeaderNames[i], sizeof(pkt.ffaLeaderNames[i]),
                              "NPC-%u", sorted[i].first);
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
            "[ServerGamemode] sent state duelId=%u version=%u phase=%u mode=%s map=%s player=%u sent=%d score=%d-%d red=%d blue=%d\n",
            d.duelId, d.stateVersion, (unsigned)d.phase, d.matchMode.c_str(), d.mapId.c_str(),
            kv.second.id, (int)sent, d.scoreA, d.scoreB, d.redTeamKills, d.blueTeamKills);
    }
}

// Pick ONE random map spawn point as the match anchor. Both teams always
// spawn near this single point (with a fresh random XY offset each spawn), so
// respawns land right back in the fight — max action, no map editing needed.
void assignGamemodeSpawns(ServerGamemodeState& d, const HeadlessWorld& world)
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
glm::vec3 gamemodeSpawnPoint(const ServerGamemodeState& d)
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-d.spawnOffsetRadius, d.spawnOffsetRadius);
    return d.spawnA + glm::vec3(dist(rng), dist(rng), 0.0f);
}

void assignGamemodeParticipants(ServerGamemodeState& d,
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
void teleportGamemodeParticipantsToSpawns(ServerGamemodeState& d,
                              std::unordered_map<uint32_t, ServerPlayer>& players)
{
    auto place = [&](uint32_t playerId)
    {
        auto it = players.find(playerId);
        if (it == players.end()) return;
        ServerPlayer& p = it->second;
        const glm::vec3 spawn = gamemodeSpawnPoint(d);
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
            beginAuthoritativeTransform(p, spawn,
                SpawnVelocityConfig::instance().enabled()
                    ? SpawnVelocityConfig::instance().computeSpawnImpulse(p.yaw)
                    : glm::vec3(0.0f), p.yaw, "duel-spawn");
            p.justRespawned = true;
        }
    };
    place(d.playerAId);
    place(d.playerBId);
}

void beginGamemodeCountdown(ServerGamemodeState& d,
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
        "[ServerGamemode] selected authoritative map=%s duelId=%u stateVersion=%u\n",
        d.mapId.c_str(), d.duelId, d.stateVersion);
    teleportGamemodeParticipantsToSpawns(d, players);
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
                        const ServerGamemodeState& d,
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
void commitGamemodeMap(ServerGamemodeState& d, HeadlessWorld& world, World& npcWorld,
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
bool reloadGamemodeMap(SOCKET sock,
                   ServerGamemodeState& d,
                   std::unordered_map<uint32_t, ServerPlayer>& players,
                   HeadlessWorld& world,
                   World& npcWorld,
                   std::unordered_map<uint32_t, ServerNpc>& npcs,
                   NpcSystem& npcSystem,
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
    commitGamemodeMap(d, world, npcWorld, tmp, mapId);
    assignGamemodeSpawns(d, world);
    broadcastMapChange(sock, d, mapId, players, totalPacketsOut);
    teleportGamemodeParticipantsToSpawns(d, players);
    for (Npc& npc : npcSystem.all())
    {
        const glm::vec3 spawn = gamemodeSpawnPoint(d);
        npc.body.pos = spawn;
        npc.body.respawnPosition = spawn;
        npc.body.vel = glm::vec3(0.0f);
        npc.body.externalImpulse = glm::vec3(0.0f);
        npc.body.currentHp = npc.body.maxHp;
        npc.body.dead = false;
        npc.body.respawnTimer = 0.0f;
        finalizeServerNpcSpawn(npc, ActorSpawnReason::MapChange);
        npc.body.syncLegacyStateToLayers();
        npc.body.updateModelWorldTransforms();
        auto mirror = npcs.find(npc.id);
        if (mirror != npcs.end())
        {
            mirror->second.pos = spawn;
            mirror->second.vel = glm::vec3(0.0f);
            mirror->second.health = npc.body.currentHp;
            ++mirror->second.transformEpoch;
        }
    }
    // Map changes are actor lifecycle boundaries, not duel-only teleports.
    // Every active player receives a fresh authoritative spawn on the new map.
    for (auto& kv : players) {
        ServerPlayer& p = kv.second;
        if (p.spawnState != ServerPlayer::Active) continue;
        p.duelSpawnPos = gamemodeSpawnPoint(d);
        p.hasDuelSpawnPos = true;
        beginAuthoritativeTransform(p, p.duelSpawnPos,
                                    SpawnVelocityConfig::instance().enabled()
                                        ? SpawnVelocityConfig::instance().computeSpawnImpulse(p.yaw)
                                        : glm::vec3(0.0f), p.yaw,
                                    "map-change-respawn");
        completeAuthoritativeSpawn(sock, p, false);
    }
    Debug::warn(Debug::Category::Duel,
        "[DUEL SERVER] map changed live to %s (spawns=%zu)\n",
        mapId.c_str(), world.spawnPoints.size());
    return true;
}

// Round-robin rotation: pick a map not used this cycle (never the one just
// played), skip maps that fail to load or have no spawn points, and cycle the
// whole pool before repeating. Returns true if the map changed.
bool rotateToNextGamemodeMap(SOCKET sock,
                         ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         HeadlessWorld& world,
                         World& npcWorld,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         NpcSystem& npcSystem,
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
            commitGamemodeMap(d, world, npcWorld, tmp, cand);
            assignGamemodeSpawns(d, world);
            broadcastMapChange(sock, d, cand, players, totalPacketsOut);
            teleportGamemodeParticipantsToSpawns(d, players);
            for (Npc& npc : npcSystem.all())
            {
                const glm::vec3 spawn = gamemodeSpawnPoint(d);
                npc.body.pos = spawn;
                npc.body.respawnPosition = spawn;
                npc.body.vel = glm::vec3(0.0f);
                npc.body.externalImpulse = glm::vec3(0.0f);
                npc.body.currentHp = npc.body.maxHp;
                npc.body.dead = false;
                npc.body.respawnTimer = 0.0f;
                finalizeServerNpcSpawn(npc, ActorSpawnReason::MapChange);
                npc.body.syncLegacyStateToLayers();
                npc.body.updateModelWorldTransforms();
                auto mirror = npcs.find(npc.id);
                if (mirror != npcs.end())
                {
                    mirror->second.pos = spawn;
                    mirror->second.vel = glm::vec3(0.0f);
                    mirror->second.health = npc.body.currentHp;
                    ++mirror->second.transformEpoch;
                }
            }
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

void assignMatchParticipants(ServerGamemodeState& d,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             std::unordered_map<uint32_t, ServerNpc>* npcs = nullptr)
{
    d.participants.clear();
    d.participantNames.clear();
    d.ffaKills.clear();
    d.ffaDeaths.clear();
    d.matchTeams.clear();
    d.redTeamKills = 0;
    d.blueTeamKills = 0;

    for (const auto& kv : players) {
        if (kv.second.spawnState == ServerPlayer::Active) {
            d.participants.push_back(kv.first);
            d.participantNames[kv.first] = kv.second.name;
            d.ffaKills[kv.first] = 0;
            d.ffaDeaths[kv.first] = 0;
        }
    }

    if (npcs) {
        for (const auto& kv : *npcs) {
            if (kv.second.health <= 0) continue;
            d.participants.push_back(kv.first);
            d.participantNames[kv.first] = kv.second.name.empty()
                ? "NPC-" + std::to_string(kv.first) : kv.second.name;
            d.ffaKills[kv.first] = 0;
            d.ffaDeaths[kv.first] = 0;
        }
    }

    // Sort by ID for deterministic team assignment
    std::sort(d.participants.begin(), d.participants.end());

    if (d.matchMode == "tdm") {
        for (size_t i = 0; i < d.participants.size(); ++i) {
            d.matchTeams[d.participants[i]] = (int)(i % 2);
            auto playerIt = players.find(d.participants[i]);
            if (playerIt != players.end())
                playerIt->second.matchTeam = (int)(i % 2);
            if (npcs) {
                auto npcIt = npcs->find(d.participants[i]);
                if (npcIt != npcs->end())
                    npcIt->second.matchTeam = (int)(i % 2);
            }
        }
        Debug::log(Debug::Category::Duel,
            "[FFA/TDM] Assigned %zu players to teams (red=%d blue=%d)\n",
            d.participants.size(), d.redTeamKills, d.blueTeamKills);
    }
}

void resetGamemodeActorsAtMapSpawn(
    ServerGamemodeState& d,
    std::unordered_map<uint32_t, ServerPlayer>& players,
    std::unordered_map<uint32_t, ServerNpc>& npcs,
    NpcSystem& npcSystem)
{
    for (uint32_t pid : d.participants) {
        const glm::vec3 spawn = gamemodeSpawnPoint(d);
        auto playerIt = players.find(pid);
        if (playerIt != players.end()) {
            ServerPlayer& p = playerIt->second;
            // Rebuild the authoritative inventory from the currently
            // selected community weapon set at every managed-mode boundary.
            // This prevents a broad initial inventory from surviving into a
            // later restricted FFA/TDM round.
            resetPlayerForSpawn(p, true);
            p.duelSpawnPos = spawn;
            p.hasDuelSpawnPos = true;
            p.respawnSeconds = 0.0f;
            beginAuthoritativeTransform(p, spawn,
                SpawnVelocityConfig::instance().enabled()
                    ? SpawnVelocityConfig::instance().computeSpawnImpulse(p.yaw)
                    : glm::vec3(0.0f), p.yaw, "gamemode-spawn");
            p.justRespawned = true;
            continue;
        }

        auto mirrorIt = npcs.find(pid);
        if (mirrorIt == npcs.end()) continue;
        for (Npc& npc : npcSystem.all()) {
            if (npc.id != pid) continue;
            npc.body.pos = spawn;
            npc.body.respawnPosition = spawn;
            npc.body.vel = glm::vec3(0.0f);
            npc.body.externalImpulse = glm::vec3(0.0f);
            npc.body.currentHp = npc.body.maxHp;
            npc.body.dead = false;
            npc.body.respawnTimer = 0.0f;
            for (auto it = npc.body.weaponRuntimes.begin();
                 it != npc.body.weaponRuntimes.end(); ) {
                if (!serverCommunityWeaponAllowed(it->first))
                    it = npc.body.weaponRuntimes.erase(it);
                else
                    ++it;
            }
            if (!serverCommunityWeaponAllowed(npc.body.equippedWeaponId)) {
                npc.body.equippedWeaponId.clear();
                npc.body.equippedSlot = -1;
                npc.body.hasValidWeapon = false;
                if (!npc.body.weaponRuntimes.empty()) {
                    npc.body.equippedWeaponId = npc.body.weaponRuntimes.begin()->first;
                    if (const WeaponDefinition* def =
                            WeaponRegistry::instance().get(npc.body.equippedWeaponId))
                        npc.body.equippedSlot = def->slot;
                    npc.body.hasValidWeapon = true;
                }
            }
            finalizeServerNpcSpawn(npc, ActorSpawnReason::GamemodeStart);
            npc.body.syncLegacyStateToLayers();
            npc.body.updateModelWorldTransforms();
            mirrorIt->second.pos = spawn;
            mirrorIt->second.vel = glm::vec3(0.0f);
            mirrorIt->second.health = npc.body.currentHp;
            ++mirrorIt->second.transformEpoch;
            break;
        }
        Debug::log(Debug::Category::Duel,
            "[GamemodeSpawn] actor=%u spawn=(%.3f,%.3f,%.3f)\n",
            pid, spawn.x, spawn.y, spawn.z);
    }
}

void resetMatchScores(ServerGamemodeState& d)
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

void beginMatchCountdown(ServerGamemodeState& d,
                         std::unordered_map<uint32_t, ServerPlayer>& players,
                         std::unordered_map<uint32_t, ServerNpc>& npcs,
                         NpcSystem& npcSystem,
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
    resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
    Debug::log(Debug::Category::Duel,
        "[ServerMatch] countdown started mode=%s duelId=%u matchStartTick=%u timeLimitTick=%u participants=%zu\n",
        d.matchMode.c_str(), d.duelId, d.matchStartTick, d.matchTimeLimitTick, d.participants.size());
}

static void emitGamemodeMatchPersistence(ServerGamemodeState& d, uint32_t tick,
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

        if (d.matchMode == "ffa") {
            auto killIt = d.ffaKills.find(kv.first);
            p.kills = killIt != d.ffaKills.end() ? killIt->second : 0;
            auto deathIt = d.ffaDeaths.find(kv.first);
            p.deaths = deathIt != d.ffaDeaths.end() ? deathIt->second : 0;
            p.won = (kv.first == d.winnerPlayerId);
        } else if (d.matchMode == "tdm") {
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

void checkMatchWinConditions(ServerGamemodeState& d, uint32_t tick,
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
                emitGamemodeMatchPersistence(d, tick, players);
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
            emitGamemodeMatchPersistence(d, tick, players);
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
            emitGamemodeMatchPersistence(d, tick, players);
        } else if (d.matchMode == "tdm") {
            d.winnerTeam = d.redTeamKills >= d.blueTeamKills ? 0 : 1;
            emitGamemodeMatchPersistence(d, tick, players);
        }
        ++d.stateVersion;
        broadcastDuelState(sock, d, players, totalPacketsOut);
        Debug::warn(Debug::Category::Duel,
            "[DUEL SERVER] Time limit reached mode=%s red=%d blue=%d\n",
            d.matchMode.c_str(), d.redTeamKills, d.blueTeamKills);
    }
}

} // namespace

void serverGamemodeTick(SOCKET sock,
                    std::unordered_map<uint32_t, ServerPlayer>& players,
                    HeadlessWorld& world,
                    World& npcWorld,
                    std::unordered_map<uint32_t, ServerNpc>& npcs,
                    NpcSystem& npcSystem,
                    std::unordered_set<uint32_t>& npcIdsAlive,
                    uint32_t tick,
                    uint64_t& totalPacketsOut)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;
    d.currentServerTick = tick;
    if (!d.mapOnly && d.appliedCommunityWeaponSetId != d.communityWeaponSetId)
    {
        resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
        d.appliedCommunityWeaponSetId = d.communityWeaponSetId;
        ++d.stateVersion;
        d.stateBroadcastPending = true;
        Debug::warn(Debug::Category::Duel,
            "[GAMEMODE LOADOUT] set=%d applied actors=%zu tick=%u\n",
            d.communityWeaponSetId, d.participants.size(), tick);
    }
    if (d.mapOnly)
    {
        const uint64_t now = nowMs();
        if (d.communityRoundOver && now >= d.communityRoundResetMs) {
            d.communityRoundOver = false;
            d.communityScores.clear();
            d.communityTeamScore[0] = d.communityTeamScore[1] = 0;
            Debug::log(Debug::Category::Networking, "[COMMUNITY MATCH] new round mode=%s\n", d.communityMode.c_str());
        }
        // Promote one kill event per tick from the queue into hasPendingKill
        // so community/mapOnly FFA and TDM scoring actually runs.
        if (!d.hasPendingKill && !d.pendingKillEvents.empty())
        {
            const ServerGamemodeKillEvent ev = std::move(d.pendingKillEvents.front());
            d.pendingKillEvents.pop_front();
            d.pendingKillerId = ev.killerId;
            d.pendingVictimId = ev.victimId;
            d.pendingKillerIsNpc = ev.killerEntityType == ENTITY_NPC;
            d.pendingVictimIsNpc = ev.victimEntityType == ENTITY_NPC;
            d.hasPendingKill = true;
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
                    // Sync to ffaKills so broadcastDuelState leaderboard renders correctly
                    d.ffaKills[killer] = score;
                    if (score >= 20) {
                        d.communityRoundOver = true;
                        const auto winner = players.find(killer);
                        const std::string name = winner == players.end() ? "Player" : winner->second.name;
                        const std::string message = name + " wins Free For All (20 kills)!";
                        broadcastCommunityNotification(sock, players, message, 300, totalPacketsOut);
                        d.communityRoundResetMs = now + 5000;
                    }
                    broadcastDuelState(sock, d, players, totalPacketsOut);
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
                if (reloadGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, next, totalPacketsOut)) {
                    d.nextMapRotationMs = now + (uint64_t)d.mapRotationMinutes * 60000ull;
                    npcSystem.destroyAll();
                    size_t spawnIndex = 0;
                    for (auto& kv : npcs) {
                        ServerNpc& npc = kv.second;
                        if (!world.spawnPoints.empty()) {
                            const auto& sp = world.spawnPoints[spawnIndex % world.spawnPoints.size()];
                            npc.pos = effectiveServerSpawn(sp.position);
                            npc.yaw = sp.yaw;
                            ++spawnIndex;
                        }
                        npc.health = 100;
                        ++npc.transformEpoch;
                    }
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
            if (reloadGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, mapId, totalPacketsOut)) {
                npcSystem.destroyAll();
                size_t spawnIndex = 0;
                for (auto& kv : npcs) {
                    ServerNpc& npc = kv.second;
                    if (!world.spawnPoints.empty()) {
                        const auto& sp = world.spawnPoints[spawnIndex % world.spawnPoints.size()];
                        npc.pos = effectiveServerSpawn(sp.position);
                        npc.yaw = sp.yaw;
                        ++spawnIndex;
                    }
                    npc.health = 100;
                    ++npc.transformEpoch;
                }
                npcIdsAlive.clear();
            }
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
    }

    // Promote one ordered kill event per fixed server tick into the legacy
    // respawn/scoring body below. The queue prevents later kills from
    // overwriting earlier kills; the body remains the single score owner.
    if (!d.hasPendingKill && !d.pendingKillEvents.empty())
    {
        const ServerGamemodeKillEvent event = std::move(d.pendingKillEvents.front());
        d.pendingKillEvents.pop_front();
        d.pendingKillerId = event.killerId;
        d.pendingVictimId = event.victimId;
        d.pendingKillerIsNpc = event.killerEntityType == ENTITY_NPC;
        d.pendingVictimIsNpc = event.victimEntityType == ENTITY_NPC;
        d.hasPendingKill = true;
        Debug::log(Debug::Category::Duel,
            "[GAMEMODE KILL QUEUE] event=%u killer=%u kind=%u victim=%u kind=%u remaining=%zu tick=%u\n",
            event.eventId, event.killerId, (unsigned)event.killerEntityType,
            event.victimId, (unsigned)event.victimEntityType,
            d.pendingKillEvents.size(), tick);
        DBG(Network,
            "KILL_QUEUE_PROMOTE eventId=%u killer=%u killerKind=%u victim=%u victimKind=%u "
            "weapon=\"%s\" remaining=%zu tick=%u phase=%d matchMode=\"%s\" enabled=%d mapOnly=%d",
            event.eventId, event.killerId, (unsigned)event.killerEntityType,
            event.victimId, (unsigned)event.victimEntityType,
            event.weaponDisplayName.c_str(), d.pendingKillEvents.size(),
            tick, (int)d.phase, d.matchMode.c_str(), (int)d.enabled, (int)d.mapOnly);
    }

    // Process one queued kill recorded by authoritative damage.
    if (d.hasPendingKill)
    {
        d.hasPendingKill = false;
        const uint32_t killerId = d.pendingKillerId;
        const uint32_t victimId = d.pendingVictimId;

        // Instant respawn near the match anchor with full HP/ammo and a fresh
        // random offset so the exact respawn spot is never predictable.
        auto victimIt = players.find(victimId);
        if (!d.pendingVictimIsNpc && victimIt != players.end())
        {
            victimIt->second.respawnSeconds = 0.0f;
            victimIt->second.duelSpawnPos = gamemodeSpawnPoint(d);
        }
        else if (d.pendingVictimIsNpc)
        {
            // NPC deaths use the same current-map spawn anchor as players;
            // retaining the old body respawn position causes repeated void
            // deaths after a map transition.
            resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
        }

        // Tell the killer where the victim respawned (tracer).
        if (killerId != victimId && !d.pendingVictimIsNpc)
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
                    emitGamemodeMatchPersistence(d, tick, players);
                    Debug::warn(Debug::Category::Duel,
                        "[DUEL SERVER] match over winner=%u score=%d-%d goal=%d\n",
                        d.winnerPlayerId, d.scoreA, d.scoreB, d.goalValue);
                }
            }
            // FFA scoring
            else if (d.matchMode == "ffa") {
                const int killsBefore = d.ffaKills[killerId];
                const int deathsBefore = d.ffaDeaths[victimId];
                ++d.ffaKills[killerId];
                ++d.ffaDeaths[victimId];
                DBG(Network,
                    "FFA_SCORED killer=%u killerKind=%d victim=%u victimKind=%d "
                    "killsBefore=%d killsAfter=%d deathsBefore=%d deathsAfter=%d "
                    "tick=%u phase=%d matchOver=%d",
                    killerId, (int)d.pendingKillerIsNpc,
                    victimId, (int)d.pendingVictimIsNpc,
                    killsBefore, d.ffaKills[killerId],
                    deathsBefore, d.ffaDeaths[victimId],
                    tick, (int)d.phase, (int)d.matchOver);
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
        else
        {
            DBG(Network,
                "KILL_SCORE_SKIPPED killer=%u killerKind=%d victim=%u victimKind=%d "
                "reason=%s tick=%u phase=%d matchOver=%d suicide=%d matchMode=\"%s\"",
                killerId, (int)d.pendingKillerIsNpc,
                victimId, (int)d.pendingVictimIsNpc,
                d.phase != DUEL_PHASE_ACTIVE ? "phase_not_active" :
                    d.matchOver ? "match_over" : "suicide",
                tick, (int)d.phase, (int)d.matchOver,
                (int)(killerId == victimId), d.matchMode.c_str());
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
            if (countActivePlayers(players) >= 2 ||
                (countActivePlayers(players) >= 1 && !npcs.empty()))
            {
                // If the current map has no spawn points, rotate.
                if (world.spawnPoints.empty())
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                assignMatchParticipants(d, players, &npcs);
                beginMatchCountdown(d, players, npcs, npcSystem, tick);
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::warn(Debug::Category::Duel,
                    "[FFA/TDM] Countdown started mode=%s participants=%zu\n",
                    d.matchMode.c_str(), d.participants.size());
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
            else if (tick - d.lastBroadcastTick >= 60)
            {
                d.lastBroadcastTick = tick;
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
                resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
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
                {
                    std::string scores;
                    for (const auto& kv : d.ffaKills) {
                        if (!scores.empty()) scores += ", ";
                        auto nameIt = d.participantNames.find(kv.first);
                        scores += (nameIt != d.participantNames.end() ? nameIt->second : "id" + std::to_string(kv.first))
                                  + "=" + std::to_string(kv.second);
                    }
                    DBG(Network,
                        "HEARTBEAT mode=\"%s\" phase=%d tick=%u participants=%zu scores=[%s] "
                        "queueSize=%zu enabled=%d mapOnly=%d serverCode=\"%s\"",
                        d.matchMode.c_str(), (int)d.phase, tick,
                        d.participants.size(), scores.c_str(),
                        d.pendingKillEvents.size(), (int)d.enabled, (int)d.mapOnly,
                        getServerCoordinatorCode().c_str());
                }
            }
            break;

        case DUEL_PHASE_RESULTS:
            d.phaseTimer -= SERVER_DT;
            if (d.phaseTimer <= 0.0f)
            {
                if (d.pendingModeSwitch && !d.pendingGamemodeId.empty())
                {
                    const std::string nextMode = d.pendingGamemodeId;
                    const bool directCountdown = d.pendingModeSwitchCountdown;
                    d.pendingModeSwitch = false;
                    d.pendingModeSwitchCountdown = false;
                    d.pendingGamemodeId.clear();
                    d.phase = DUEL_PHASE_WAITING;
                    d.matchMode.clear();
                    serverCommunityStartMatch(directCountdown, nextMode);
                    return;
                }
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
                    rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
                assignGamemodeSpawns(d, world);
                assignMatchParticipants(d, players, &npcs);
                beginMatchCountdown(d, players, npcs, npcSystem, tick);
                ++d.stateVersion;
                broadcastDuelState(sock, d, players, totalPacketsOut);
                Debug::log(Debug::Category::Duel,
                    "[FFA/TDM] Countdown started mode=%s participants=%zu\n",
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
    if (d.hasBombFeature)
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
            assignGamemodeParticipants(d, players);
            // If the current map has no spawn points (e.g. the host picked a
            // spawn-less map), rotate to a spawn-capable one before starting.
            if (world.spawnPoints.empty())
                rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
            // Drop the practice NPC(s) once the real duel is about to start.
            npcs.clear();
            npcSystem.destroyAll();
            npcIdsAlive.clear();
            assignGamemodeSpawns(d, world);
            beginGamemodeCountdown(d, players);
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
                "[ServerGamemode] countdown complete duelId=%u stateVersion=%u phase=ACTIVE\n",
                d.duelId, d.stateVersion);
            broadcastDuelState(sock, d, players, totalPacketsOut);
        }
        // During countdown, do NOT broadcast every tick. Reliable delivery of
        // every countdown snapshot creates a huge backlog that blocks the
        // ACTIVE transition from being delivered promptly. The initial
        // countdown start is broadcast by beginGamemodeCountdown(); the ACTIVE
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
                rotateToNextGamemodeMap(sock, d, players, world, npcWorld, npcs, npcSystem, totalPacketsOut);
            // Each new match picks a fresh random anchor (fights spread around).
            assignGamemodeSpawns(d, world);
            Debug::log(Debug::Category::Duel, "[DUEL SERVER] rematch\n");
            beginGamemodeCountdown(d, players);
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

void serverGamemodeRematchNow()
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;
    // The MATCH_END branch starts the next duel when rematchLeft hits 0.
    d.rematchLeft = 0.0f;
}

std::string serverActiveTeamList()
{
    const ServerGamemodeState& d = serverGamemodeState();
    CommunityServerConfig& config = CommunityServerConfig::instance();
    if (config.modes().empty()) config.load();
    const CommunityMode* cm = config.modeById(d.communityMode);
    const std::string id = cm ? cm->gamemodeId : d.communityMode;
    const Gamemode& gm = GamemodeRegistry::instance().get(id);
    if (gm.teamNames.empty()) return "no teams";
    std::string out;
    for (size_t i = 0; i < gm.teamNames.size(); ++i) {
        if (!out.empty()) out += " | ";
        out += gm.teamNames[i] + " = " + std::to_string(i + 1);
    }

    return out;
}

bool serverRequestTeamChange(uint32_t playerId, int requestedTeam,
                             SOCKET sock,
                             std::unordered_map<uint32_t, ServerPlayer>& players,
                             uint32_t tick, uint64_t& totalPacketsOut,
                             std::string& message)
{
    ServerGamemodeState& d = serverGamemodeState();
    auto playerIt = players.find(playerId);
    if (playerIt == players.end()) { message = "player not found"; return false; }
    const CommunityMode* cm = CommunityServerConfig::instance().modeById(d.communityMode);
    const std::string id = cm ? cm->gamemodeId : d.communityMode;
    const Gamemode& gm = GamemodeRegistry::instance().get(id);
    if (requestedTeam < 0 || requestedTeam >= static_cast<int>(gm.teamNames.size())) {
        message = gm.teamNames.empty() ? "active gamemode has no teams" : "invalid team";
        return false;
    }
    playerIt->second.matchTeam = requestedTeam;
    d.matchTeams[playerId] = requestedTeam;
    message = "switched to " + gm.teamNames[static_cast<size_t>(requestedTeam)];
    broadcastServerChatMessage(sock, players, tick, totalPacketsOut,
        (playerIt->second.name + " " + message).c_str());
    Debug::warn(Debug::Category::Duel,
        "[MATCH TEAM] player=%u team=%d mode=%s result=accepted\n",
        playerId, requestedTeam, id.c_str());
    return true;
}

void serverRespawnAllActors(SOCKET sock,
                            std::unordered_map<uint32_t, ServerPlayer>& players,
                            std::unordered_map<uint32_t, ServerNpc>& npcs,
                            uint32_t tick, uint64_t& totalPacketsOut)
{
    ServerGamemodeState& d = serverGamemodeState();
    for (auto& kv : players) {
        ServerPlayer& player = kv.second;
        if (player.spawnState != ServerPlayer::Active) continue;
        player.duelSpawnPos = gamemodeSpawnPoint(d);
        player.hasDuelSpawnPos = true;
        player.pos = player.duelSpawnPos;
        completeAuthoritativeSpawn(sock, player, false);
    }
    for (auto& kv : npcs) {
        ServerNpc& npc = kv.second;
        npc.health = 100;
        ++npc.transformEpoch;
        finalizeServerNpcMirrorSpawn(npc, ActorSpawnReason::RespawnAll, tick);
    }
    d.hasPendingKill = false;
    d.pendingKillerId = 0;
    d.pendingVictimId = 0;
    d.pendingKillerIsNpc = false;
    d.pendingVictimIsNpc = false;
    d.pendingKillEvents.clear();
    Debug::warn(Debug::Category::Duel,
        "[MATCH RESPAWN ALL] players=%zu npcs=%zu tick=%u\n",
        players.size(), npcs.size(), tick);
    broadcastDuelState(sock, d, players, totalPacketsOut);
}

void serverGamemodeRequestMapChange(const std::string& mapId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled || mapId.empty()) return;
    d.hasPendingManualMap = true;
    d.pendingManualMap = mapId;
}

void serverGamemodeOnPlayerDeath(uint32_t killerPlayerId,
                             uint32_t victimPlayerId,
                             const std::string& weaponId,
                             const std::string& weaponDisplayName,
                             uint64_t correlationId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;
    ServerGamemodeKillEvent event;
    event.killerId = killerPlayerId;
    event.victimId = victimPlayerId;
    event.killerEntityType = ENTITY_PLAYER;
    event.victimEntityType = ENTITY_PLAYER;
    event.weaponId = weaponId;
    event.weaponDisplayName = weaponDisplayName;
    event.eventId = ++d.killEventCounter;
    event.correlationId = correlationId;
    event.serverTick = d.currentServerTick;
    d.pendingKillEvents.push_back(std::move(event));
}

void serverGamemodeOnNpcDeath(uint32_t killerNpcId,
                          uint32_t victimPlayerId,
                          const std::string& weaponId,
                          const std::string& weaponDisplayName,
                          uint64_t correlationId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;
    ServerGamemodeKillEvent event;
    event.killerId = killerNpcId;
    event.victimId = victimPlayerId;
    event.killerEntityType = ENTITY_NPC;
    event.victimEntityType = ENTITY_PLAYER;
    event.weaponId = weaponId;
    event.weaponDisplayName = weaponDisplayName;
    event.eventId = ++d.killEventCounter;
    event.correlationId = correlationId;
    event.serverTick = d.currentServerTick;
    DBG(Network,
        "GAMEMODE_ENQUEUE type=NPC_KILLS_PLAYER killerNpcId=%u victimPlayerId=%u "
        "weaponId=\"%s\" weaponDisplay=\"%s\" eventId=%u queueSize=%zu tick=%u",
        killerNpcId, victimPlayerId,
        weaponId.c_str(), weaponDisplayName.c_str(),
        event.eventId, d.pendingKillEvents.size() + 1, d.currentServerTick);
    d.pendingKillEvents.push_back(std::move(event));
}

void serverGamemodeOnPlayerKilledNpc(uint32_t killerPlayerId,
                                     uint32_t victimNpcId,
                                     const std::string& weaponId,
                                     const std::string& weaponDisplayName,
                                     uint64_t correlationId)
{
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;
    ServerGamemodeKillEvent event;
    event.killerId = killerPlayerId;
    event.victimId = victimNpcId;
    event.killerEntityType = ENTITY_PLAYER;
    event.victimEntityType = ENTITY_NPC;
    event.weaponId = weaponId;
    event.weaponDisplayName = weaponDisplayName;
    event.eventId = ++d.killEventCounter;
    event.correlationId = correlationId;
    event.serverTick = d.currentServerTick;
    DBG(Network,
        "GAMEMODE_ENQUEUE type=PLAYER_KILLS_NPC killerPlayerId=%u victimNpcId=%u "
        "weaponId=\"%s\" weaponDisplay=\"%s\" eventId=%u queueSize=%zu tick=%u",
        killerPlayerId, victimNpcId,
        weaponId.c_str(), weaponDisplayName.c_str(),
        event.eventId, d.pendingKillEvents.size() + 1, d.currentServerTick);
    d.pendingKillEvents.push_back(std::move(event));
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
glm::vec3 getBombHolderPosition(const ServerGamemodeState& d,
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
void selectNewBombHolder(ServerGamemodeState& d,
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
                           ServerGamemodeState& d,
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
                          ServerGamemodeState& d,
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
    ServerGamemodeState& d = serverGamemodeState();
    if (!d.enabled) return;

    // Load bomb tag gamemode config using the gamemode_id from the JSON.
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
    d.phase = DUEL_PHASE_INTERMISSION;
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
    ServerGamemodeState& d = serverGamemodeState();
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
            victimIt->second.duelSpawnPos = gamemodeSpawnPoint(d);
        }
    }

    // ── State machine ────────────────────────────────────────────────
    switch (d.phase) {
    case DUEL_PHASE_WAITING:
        if (countActivePlayers(players) >= 1) {
            // Start bomb tag with available players
            if (world.spawnPoints.empty())
                assignGamemodeSpawns(d, world);
            assignMatchParticipants(d, players, &npcs);
            beginMatchCountdown(d, players, npcs, npcSystem, tick);
            ++d.stateVersion;
            broadcastDuelState(sock, d, players, totalPacketsOut);
            broadcastBombTagState(sock, d, players, totalPacketsOut);
            Debug::warn(Debug::Category::Duel,
                "[BOMB TAG] countdown started participants=%zu\n", d.participants.size());
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
            resetGamemodeActorsAtMapSpawn(d, players, npcs, npcSystem);
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
        // ── Bomb holder disconnected or died? Transfer immediately ───
        if (d.bombOwnerType == 1 && d.bombOwnerPlayerId != 0) {
            auto holderIt = players.find(d.bombOwnerPlayerId);
            if (holderIt == players.end() || holderIt->second.spawnState != ServerPlayer::Active
                || holderIt->second.dead) {
                Debug::warn(Debug::Category::Duel,
                    "[BOMB TAG] holder lost id=%u — transferring\n",
                    d.bombOwnerPlayerId);
                d.bombOwnerType = 0;
                d.bombOwnerPlayerId = 0;
                d.bombInactiveTicks = 0;
                selectNewBombHolder(d, players);
                ++d.stateVersion;
                broadcastBombTagState(sock, d, players, totalPacketsOut);
            }
        }

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
                victimIt->second.duelSpawnPos = gamemodeSpawnPoint(d);
                victimIt->second.respawnSeconds = 0.0f;
                beginAuthoritativeTransform(victimIt->second,
                    victimIt->second.duelSpawnPos,
                    SpawnVelocityConfig::instance().enabled()
                        ? SpawnVelocityConfig::instance().computeSpawnImpulse(victimIt->second.yaw)
                        : glm::vec3(0.0f),
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
            if (d.pendingModeSwitch && !d.pendingGamemodeId.empty()) {
                const std::string nextMode = d.pendingGamemodeId;
                const bool directCountdown = d.pendingModeSwitchCountdown;
                d.pendingModeSwitch = false;
                d.pendingModeSwitchCountdown = false;
                d.pendingGamemodeId.clear();
                d.phase = DUEL_PHASE_WAITING;
                d.matchMode.clear();
                serverCommunityStartMatch(directCountdown, nextMode);
                return;
            }
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
            assignGamemodeSpawns(d, world);
            assignMatchParticipants(d, players, &npcs);
            beginMatchCountdown(d, players, npcs, npcSystem, tick);
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
