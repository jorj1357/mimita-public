// C:\important\mimita-priv-v8\src\game\duel.h
// 6 7 2026
/** purpose
 * duels first game mode
 * first to 5, 100 hp, spawn with revolver and shotgun, small close range map
 */

#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Player;
class NpcSystem;
struct World;
class Camera;

enum class DuelPhase {
    Off,
    Countdown,
    Active,
    RoundEnd,
    MatchEnd
};

enum class DuelTeam {
    Player,
    NPC
};

enum class MapRotationMode {
    PerDeath,
    PerRound
};

struct DuelConfig {
    int numNpcs = 3;
    float npcDifficulty = 5.0f;
    std::vector<std::string> npcNames = {"Bot 1", "Bot 2", "Bot 3"};
    int duelLengthSeconds = 300;
    int killsToWin = 10;
    float respawnDelaySeconds = 2.0f;
    MapRotationMode mapRotationMode = MapRotationMode::PerRound;
    std::string mapPath = "assets/maps/mimita-aabb-only-interior-small-v4.glb";
    bool enabled = false;
};

struct DuelStats {
    int kills = 0;
    int deaths = 0;
    int points = 0;
    int xp = 0;
    int roundsWon = 0;
    int matchesWon = 0;
};

struct DuelRoundResult {
    DuelTeam winningTeam = DuelTeam::Player;
    int playerRoundsWon = 0;
    int npcRoundsWon = 0;
    bool matchEnded = false;
    DuelTeam matchWinner = DuelTeam::Player;
};

class DuelManager {
public:
    void start(const DuelConfig& cfg, Player& player, NpcSystem& npcs, World& world);
    void update(float dt, Player& player, NpcSystem& npcs, World& world, Camera& camera);
    void renderHud();

    void onPlayerKill(int npcIndex);
    void onNpcKill(int npcIndex);
    void onEntityDeath(DuelTeam team);

    bool enabled() const { return config.enabled; }
    DuelPhase phase() const { return currentPhase; }
    const DuelStats& stats() const { return playerStats; }
    int playerRoundsWon() const { return playerRoundsWon_; }
    int npcRoundsWon() const { return npcRoundsWon_; }
    bool isDuelFrozen() const { return duelFrozen_; }

    void setMapList(const std::vector<std::string>& maps);
    void rotateMap(World& world);

private:
    DuelConfig config;
    DuelPhase currentPhase = DuelPhase::Off;

    float countdown = 3.0f;
    float timer = 0.0f;
    float roundEndTimer = 0.0f;
    int currentRound = 0;
    int currentMapIndex = 0;

    int playerKills = 0;
    std::vector<int> npcKills;

    int alivePlayerCount = 1;
    int aliveNpcCount = 0;
    int playerRoundsWon_ = 0;
    int npcRoundsWon_ = 0;
    bool duelFrozen_ = false;

    DuelStats playerStats;
    std::vector<std::string> mapList;

    void beginFight(Player& player, NpcSystem& npcs, World& world);
    void endRound(DuelTeam winner);
    void endMatch();
    void startCountdown();
    void freezeAll(Player& player, NpcSystem& npcs);
    void unfreezeAll(Player& player, NpcSystem& npcs);
    void resetRoundEntities(Player& player, NpcSystem& npcs, World& world);
    bool checkRoundEnd();
};
