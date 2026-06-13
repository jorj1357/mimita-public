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

struct GLFWwindow;
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

enum class DuelMenuAction {
    None,
    PlayAgain,
    ExitToMenu
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
    DuelMenuAction renderMatchOverScreen(GLFWwindow* win);

    void onPlayerKill(int npcIndex);
    void onNpcKill(int npcIndex);
    void onEntityDeath(DuelTeam team);

    bool enabled() const { return config.enabled; }
    DuelPhase phase() const { return currentPhase; }
    const DuelStats& stats() const { return playerStats; }
    int playerRoundsWon() const { return playerRoundsWon_; }
    int npcRoundsWon() const { return npcRoundsWon_; }
    bool isDuelFrozen() const { return duelFrozen_; }

    DuelTeam matchWinner() const { return matchWinner_; }
    float matchOverRemaining() const { return matchOverTimer; }
    bool matchOverButtonsVisible() const { return matchOverButtonsShown; }
    glm::vec3 winnerCameraTarget() const { return matchOverCameraTarget; }

    // Final kill replay state
    bool finalKillReplayActive = false;
    std::string finalKillReplayPath;
    float finalKillReplayTime = 0.0f;
    float finalKillSlowMoFactor = 1.0f;
    int finalKillKillTick = 0;
    std::string finalKillKillerId;
    std::string finalKillVictimId;

    void restartDuel(Player& player, NpcSystem& npcs, World& world);
    void stopDuel();

    void setMapList(const std::vector<std::string>& maps);
    void rotateMap(World& world);

    // Team spawn assignment
    void assignTeamSpawns(const World& world);
    glm::vec3 getTeamSpawn(DuelTeam team, int entityIndex, int totalOnTeam) const;

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

    // Match-over state
    DuelTeam matchWinner_ = DuelTeam::Player;
    float matchOverTimer = 0.0f;
    bool matchOverButtonsShown = false;
    bool matchOverCaptured = false;
    glm::vec3 matchOverCameraTarget{0.0f};

    // Cached team spawn points for current match
    glm::vec3 mTeamASpawn{0.0f};
    glm::vec3 mTeamBSpawn{0.0f};
    int mTeamASpawnIndex = -1;
    int mTeamBSpawnIndex = -1;

    void beginFight(Player& player, NpcSystem& npcs, World& world);
    void endRound(DuelTeam winner);
    void endMatch();
    void startCountdown();
    void freezeAll(Player& player, NpcSystem& npcs);
    void unfreezeAll(Player& player, NpcSystem& npcs);
    void resetRoundEntities(Player& player, NpcSystem& npcs, World& world);
    bool checkRoundEnd();
};
