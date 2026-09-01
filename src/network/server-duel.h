// 09 01 2026, 00 00
/* purpose
* Declares the authoritative PvP duel state and tick entry points for the server.
* Runs a first-to-goal duel between two connected players: waiting, countdown,
* active scoring, match end, and the post-match rematch window.
* Also supports FFA and TDM match modes with multi-player scoring.
* Does NOT simulate players, apply damage, or render anything.
* Does NOT own the client queue/matchmaking or the coordinator protocol.
* Does NOT create team spawns - it reads them from the loaded headless world.
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "network/server.h"

namespace MimitaNet {

struct ServerDuelState
{
    bool enabled = false;
    bool mapOnly = false;
    std::string communityMode = "sandbox";
    int communityWeaponSetId = 1;
    std::unordered_map<uint32_t, int> communityScores;
    std::unordered_map<uint32_t, int> communityTeams;
    int communityTeamScore[2] = {0, 0};
    bool communityRoundOver = false;
    uint64_t communityRoundResetMs = 0;
    // DuelStatePhase (packets.h)
    uint8_t phase = DUEL_PHASE_WAITING;
    bool matchOver = false;
    int scoreA = 0;
    int scoreB = 0;
    int goalValue = 20;
    float countdown = 0.0f;
    float countdownSeconds = 3.0f;
    float rematchLeft = 0.0f;
    float rematchSeconds = 5.0f;
    std::string teamAName = "RED";
    std::string teamBName = "BLUE";
    uint32_t playerAId = 0;
    uint32_t playerBId = 0;
    uint32_t winnerPlayerId = 0;
    // The single match anchor: both teams always spawn near this one point
    // (picked from the map's spawn points, fixed for the whole match), with a
    // fresh random XY offset on every spawn/respawn. Floating on purpose.
    glm::vec3 spawnA{0.0f};
    glm::vec3 spawnB{0.0f};
    // Random XY offset radius around the anchor (meters).
    float spawnOffsetRadius = 5.0f;
    bool spawnsAssigned = false;
    // The last DuelStatePacket sent, to avoid re-broadcasting identical state.
    bool stateSent = false;
    // Pending kill deferred from applyServerDamage (no socket there). Processed
    // at the next serverDuelTick, which has the socket + packet counters.
    bool hasPendingKill = false;
    uint32_t pendingKillerId = 0;
    uint32_t pendingVictimId = 0;
    // Periodic DuelState broadcast cadence so clients can detect a dead server.
    uint32_t lastBroadcastTick = 0;
    // Live map rotation (auto on rematch) + manual changemap request.
    std::vector<std::string> mapPool;
    bool rotateMaps = false;
    bool autoMapRotation = false;
    uint32_t mapRotationMinutes = 15;
    uint64_t nextMapRotationMs = 0;
    uint64_t mapChangeCountdownStartMs = 0;
    std::string pendingAutomaticMap;
    bool hasPendingManualMap = false;
    std::string pendingManualMap;
    // Maps already used this rotation cycle (so each new duel picks a map we
    // weren't just on, and never repeats until the whole pool is used).
    std::unordered_set<std::string> usedMaps;
    // Server's current loaded map (name only, for HUD/logging).
    std::string mapId;
    uint32_t duelId = 0;
    uint32_t mapVersion = 0;
    uint32_t spawnAnchorVersion = 0;
    uint32_t respawnSequence = 0;
    uint32_t stateVersion = 0;
    uint32_t spawnAnchorIndex = 0;

    // ── FFA/TDM match mode fields ──────────────────────────────────
    // Match mode: "duel", "ffa", "tdm"
    std::string matchMode = "duel";

    // Authoritative tick references for countdown/start/end
    uint32_t countdownStartTick = 0;
    uint32_t matchStartTick = 0;
    uint32_t matchTimeLimitTick = 0;  // matchStartTick + timeLimitTicks
    uint32_t currentServerTick = 0;

    // Intermission/results phase timers
    float phaseTimer = 0.0f;
    bool startCountdownImmediately = false;
    float intermissionSeconds = 15.0f;
    float resultsSeconds = 8.0f;
    int timeLimitSeconds = 300;

    // FFA scoring: per-player kills/deaths
    std::unordered_map<uint32_t, int> ffaKills;
    std::unordered_map<uint32_t, int> ffaDeaths;

    // TDM scoring
    int redTeamKills = 0;
    int blueTeamKills = 0;

    // Team assignments (persistent per match, 0=red, 1=blue)
    std::unordered_map<uint32_t, int> matchTeams;

    // All participating player IDs (FFA/TDM can have >2 players)
    std::vector<uint32_t> participants;

    // Victory info
    int victoryType = 0;  // 0=ScoreLimit, 1=TimeLimit
    int winnerTeam = -1;  // for TDM: 0=red, 1=blue

    // Match event counter for KillEvent IDs
    uint32_t killEventCounter = 0;
};

// Singleton duel state for the current server process.
ServerDuelState& serverDuelState();

// Start duel mode with the given gamemode rules. Safe to call repeatedly.
void serverDuelStart(const ServerDuelState& rules);

// Called every server tick while the server runs in duel mode.
// `npcs`/`npcSystem`/`npcIdsAlive` let the engine drop the practice NPC(s)
// the moment the real duel starts (both players active).
void serverDuelTick(SOCKET sock,
                    std::unordered_map<uint32_t, ServerPlayer>& players,
                    HeadlessWorld& world,
                    World& npcWorld,
                    std::unordered_map<uint32_t, ServerNpc>& npcs,
                    NpcSystem& npcSystem,
                    std::unordered_set<uint32_t>& npcIdsAlive,
                    uint32_t tick,
                    uint64_t& totalPacketsOut);

// Called from applyServerDamage when a kill is confirmed. Has no socket, so it
// only records the kill (instant respawn + pending flag) and defers score and
// tracer broadcast to the next serverDuelTick.
void serverDuelOnPlayerDeath(uint32_t killerPlayerId,
                             uint32_t victimPlayerId);

// A player pressed Space on the win/lose screen: skip the rematch timer and
// start the next duel immediately (next tick).
void serverDuelRematchNow();

// Host-only changemap command: reload the given map live on the next tick.
void serverDuelRequestMapChange(const std::string& mapId);

// Starts the shared community map runtime without enabling duel scoring.
void serverCommunityMapStart(const std::vector<std::string>& mapPool,
                             const std::string& mapId,
                             bool autoRotation,
                             uint32_t rotationMinutes,
                             int weaponSetId);
void serverCommunitySetMode(const std::string& modeId);
void serverCommunitySetWeaponSet(int weaponSetId);
bool serverCommunityWeaponAllowed(const std::string& weaponId);
void serverCommunityStartMatch(bool skipIntermission = false);

} // namespace MimitaNet
