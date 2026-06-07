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
    Ended
};

struct DuelConfig {
    int numNpcs = 3;
    float npcDifficulty = 5.0f;
    std::vector<std::string> npcNames = {"Bot 1", "Bot 2", "Bot 3"};
    int duelLengthSeconds = 300;
    int killsToWin = 10;
    float respawnDelaySeconds = 2.0f;
    bool enabled = false;
};

class DuelManager {
public:
    void start(const DuelConfig& cfg, Player& player, NpcSystem& npcs);
    void update(float dt, Player& player, NpcSystem& npcs, World& world, Camera& camera);
    void renderHud();

    void onPlayerKill(int npcIndex);
    void onNpcKill(int npcIndex);

    bool enabled() const { return config.enabled; }
    DuelPhase phase() const { return currentPhase; }

private:
    DuelConfig config;
    DuelPhase currentPhase = DuelPhase::Off;

    float countdown = 3.0f;
    float timer = 0.0f;

    int playerKills = 0;
    std::vector<int> npcKills;

    void beginFight(Player& player, NpcSystem& npcs);
    void endDuel();
};