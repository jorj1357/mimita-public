#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Player;
class NpcSystem;
struct World;
class Camera;

enum class DuelPhase {
    CONFIG,
    COUNTDOWN,
    ACTIVE,
    ENDED
};

struct DuelConfig {
    int numNpcs = 3;
    float npcDifficulty = 5.0f;
    std::vector<std::string> npcNames;
    int duelLengthSeconds = 300;
    int killsToWin = 10;
    float respawnDelaySeconds = 2.0f;
    bool enabled = false;

    DuelConfig() {
        npcNames = {"Bot 1", "Bot 2", "Bot 3"};
    }
};

struct DuelState {
    DuelPhase phase = DuelPhase::CONFIG;
    float countdownTimer = 3.0f;
    float duelTimer = 0.0f;
    int playerKills = 0;
    std::vector<int> npcKills;
    bool duelEnded = false;
    int winner = -1;
    float respawnTimer = 0.0f;
    bool playerDead = false;
    int deadNpcIndex = -1;
};

class DuelManager {
public:
    DuelManager();

    void start(const DuelConfig& cfg);
    void update(float dt, Player& player, NpcSystem& npcSystem, const World& world, Camera& camera);
    void onPlayerKill(int npcIndex);
    void onNpcKill(int npcIndex);
    void respawnPlayer(Player& player, const World& world);
    void respawnNpc(NpcSystem& npcSystem, int index, const World& world, const glm::vec3& playerPos);
    bool checkWinCondition();
    void endDuel(int winner_);
    void renderCountdown();
    void renderHUD();
    void renderVictoryScreen();

    const DuelConfig& getConfig() const { return config; }
    const DuelState& getState() const { return state; }
    DuelPhase getPhase() const { return state.phase; }
    bool isActive() const { return state.phase == DuelPhase::ACTIVE; }
    bool isInCountdown() const { return state.phase == DuelPhase::COUNTDOWN; }
    bool isEnded() const { return state.phase == DuelPhase::ENDED; }

    void requestRematch();
    void requestMainMenu();

    bool wantsRematch() const { return rematchRequested; }
    bool wantsMainMenu() const { return mainMenuRequested; }

private:
    DuelConfig config;
    DuelState state;
    bool rematchRequested = false;
    bool mainMenuRequested = false;
    uint32_t npcSpawnRadius = 8;
    std::vector<glm::vec3> spawnPositions;

    void generateSpawnPositions(const glm::vec3& playerPos);
    void applyDifficultyToNpcs(NpcSystem& npcSystem);
    void handlePlayerDeath(Player& player, NpcSystem& npcSystem, const World& world, Camera& camera);
    void handleNpcDeath(int npcIndex, NpcSystem& npcSystem, const World& world, const glm::vec3& playerPos);
};

extern DuelConfig gDuelConfig;
extern DuelManager gDuelManager;