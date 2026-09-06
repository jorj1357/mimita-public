// 09 01 2026, 00 00
/* purpose
* Define authoritative gameplay events for backend persistence.
* Carry enough data for backend to process XP, gold, kills, matches.
* Does NOT block gameplay or wait for backend confirmation.
* Does NOT define database schema or HTTP transport.
* Does NOT implement reward logic or level calculation.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PersistenceKillEvent
{
    std::string eventId;
    std::string matchId;
    uint32_t serverTick = 0;
    uint32_t attackerPlayerId = 0;
    uint32_t victimPlayerId = 0;
    uint64_t victimIdentity = 0;
    uint64_t deathGeneration = 0;

    std::string attackerType;     // "player" | "npc"
    int64_t attackerId = 0;       // website user_id (0 = guest)
    std::string attackerName;

    std::string victimType;       // "player" | "npc"
    int64_t victimId = 0;         // website user_id (0 = guest)
    std::string victimName;

    std::string weaponId;
    float distanceMeters = 0.0f;
};

struct PersistenceMatchParticipant
{
    int64_t userId = 0;
    std::string username;
    std::string team;             // "red" | "blue" | ""
    int kills = 0;
    int deaths = 0;
    bool won = false;
};

struct PersistenceMatchEvent
{
    std::string eventId;
    std::string matchId;
    std::string mode;             // "team_deathmatch" | "free_for_all" | "duel"
    std::string victoryType;      // "score_limit" | "time_limit" | "draw"
    int redScore = 0;
    int blueScore = 0;
    std::string winnerTeam;       // "red" | "blue" | ""
    int64_t winnerPlayerId = 0;   // FFA only
    std::vector<PersistenceMatchParticipant> participants;
};
