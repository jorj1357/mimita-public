// 09 01 2026, 00 00
/* purpose
* Defines canonical kill-event and match-result data structures.
* Serves as the single source of truth for match scoring, killfeed, and future persistence.
* Does NOT contain gameplay logic, damage application, or rendering code.
* Does NOT write to databases or network sockets.
*/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class EntityType : uint8_t {
    Player = 0,
    Npc = 1
};

enum class VictoryType : uint8_t {
    ScoreLimit = 0,
    TimeLimit = 1
};

enum class MatchPhase : uint8_t {
    Intermission = 0,
    PreMatch = 1,
    Countdown = 2,
    Active = 3,
    Results = 4
};

struct KillEvent {
    uint32_t eventId = 0;
    uint32_t matchId = 0;
    EntityType attackerType = EntityType::Player;
    uint32_t attackerId = 0;
    EntityType victimType = EntityType::Player;
    uint32_t victimId = 0;
    uint8_t weaponId = 0;
    float distanceMeters = 0.0f;
    int attackerTeam = -1;
    int victimTeam = -1;
    uint32_t serverTick = 0;
};

struct MatchPlayerResult {
    uint32_t playerId = 0;
    int team = -1;
    int kills = 0;
    int deaths = 0;
    std::string name;
};

struct MatchResult {
    uint32_t matchId = 0;
    std::string mode;
    uint32_t startTick = 0;
    uint32_t endTick = 0;
    VictoryType victoryType = VictoryType::ScoreLimit;
    uint32_t winnerPlayerId = 0;
    int winnerTeam = -1;
    std::vector<MatchPlayerResult> participants;
};
