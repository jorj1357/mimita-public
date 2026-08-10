// 08 10 2026, 14 34
/* purpose
* Declares the authoritative PvP duel state and tick entry points for the server.
* Runs a first-to-goal duel between two connected players: waiting, countdown,
* active scoring, match end, and the post-match rematch window.
* Does NOT simulate players, apply damage, or render anything.
* Does NOT own the client queue/matchmaking or the coordinator protocol.
* Does NOT create team spawns - it reads them from the loaded headless world.
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "network/server.h"

namespace MimitaNet {

struct ServerDuelState
{
    bool enabled = false;
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
    // Team spawn positions (picked from world spawn points when the duel starts)
    glm::vec3 spawnA{0.0f};
    glm::vec3 spawnB{0.0f};
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
};

// Singleton duel state for the current server process.
ServerDuelState& serverDuelState();

// Start duel mode with the given gamemode rules. Safe to call repeatedly.
void serverDuelStart(const ServerDuelState& rules);

// Called every server tick while the server runs in duel mode.
void serverDuelTick(SOCKET sock,
                    std::unordered_map<uint32_t, ServerPlayer>& players,
                    const HeadlessWorld& world,
                    uint32_t tick,
                    uint64_t& totalPacketsOut);

// Called from applyServerDamage when a kill is confirmed. Has no socket, so it
// only records the kill (instant respawn + pending flag) and defers score and
// tracer broadcast to the next serverDuelTick.
void serverDuelOnPlayerDeath(uint32_t killerPlayerId,
                             uint32_t victimPlayerId);

} // namespace MimitaNet
