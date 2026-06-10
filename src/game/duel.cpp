// C:\important\mimita-priv-v8\src\game\duel.cpp
// 6 7 2026
/** purpose
 * duels first game mode
 * first to 5, 100 hp, spawn with revolver and shotgun, small close range map
 */

#include "game/duel.h"

#include <cstdio>
#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>

#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "world/world-gltf-loader.h"
#include "camera.h"
#include "gui/ui-system.h"

// -------------------------------------------------------
// Duel spawn pair selection
//
// Rules:
//   0 spawnpoints  -> player at hardcoded pos, NPCs circle around player
//   1 spawnpoint   -> player at spawnPoints[0], NPCs circle around player
//   2+ spawnpoints -> player at spawnPoints[0], NPCs at spawnPoints[1..N]
// -------------------------------------------------------
static void getDuelSpawnPositions(
    const World& world,
    glm::vec3& outPlayerPos,
    std::vector<glm::vec3>& outNpcPositions,
    int numNpcs)
{
    outNpcPositions.clear();
    outPlayerPos = glm::vec3(1.0f, 5.0f, 60.0f);

    if (world.spawnPoints.empty()) {
        for (int i = 0; i < numNpcs; ++i) {
            float angle = (6.2831853f * i) / std::max(1, numNpcs);
            outNpcPositions.push_back(outPlayerPos + glm::vec3(cosf(angle) * 8.0f, sinf(angle) * 8.0f, 1.0f));
        }
        return;
    }

    outPlayerPos = world.spawnPoints[0].position;

    for (int i = 0; i < numNpcs; ++i) {
        int idx = 1 + i;
        if (idx < (int)world.spawnPoints.size()) {
            outNpcPositions.push_back(world.spawnPoints[idx].position);
        } else {
            float angle = (6.2831853f * i) / std::max(1, numNpcs);
            outNpcPositions.push_back(outPlayerPos + glm::vec3(cosf(angle) * 4.0f, sinf(angle) * 4.0f, 1.0f));
        }
    }
}

void DuelManager::start(const DuelConfig& cfg, Player& player, NpcSystem& npcs, World& world)
{
    config = cfg;
    config.enabled = true;

    currentPhase = DuelPhase::Countdown;
    countdown = 3.0f;
    timer = 0.0f;
    currentRound = 1;
    currentMapIndex = 0;
    playerRoundsWon_ = 0;
    npcRoundsWon_ = 0;
    
    playerKills = 0;
    npcKills.assign(config.numNpcs, 0);
    alivePlayerCount = 1;
    aliveNpcCount = config.numNpcs;
    playerStats = DuelStats{};

    matchOverCaptured = false;
    matchOverButtonsShown = false;
    matchOverTimer = 0.0f;

    npcs.destroyAll();

    if (!mapList.empty()) {
        config.mapPath = mapList[0];
        loadWorldFromGLB(world, config.mapPath.c_str());
    }

    {
        glm::vec3 playerSpawn;
        std::vector<glm::vec3> npcSpawns;
        getDuelSpawnPositions(world, playerSpawn, npcSpawns, config.numNpcs);
        player.pos = playerSpawn;
        player.respawnPosition = playerSpawn;
    }

    printf("[DUEL] started npcs=%d killsToWin=%d map=%s\n",
           config.numNpcs, config.killsToWin, config.mapPath.c_str());
}

void DuelManager::startCountdown()
{
    currentPhase = DuelPhase::Countdown;
    countdown = 3.0f;
    timer = 0.0f;
    playerKills = 0;
    npcKills.assign(config.numNpcs, 0);
    alivePlayerCount = 1;
    aliveNpcCount = config.numNpcs;
}

void DuelManager::resetRoundEntities(
    Player& player,
    NpcSystem& npcs,
    World& world)
{
    npcs.destroyAll();

    player.dead = false;
    player.currentHp = 100.0f;
    player.vel = glm::vec3(0.0f);
    player.externalImpulse = glm::vec3(0.0f);

    {
        glm::vec3 playerSpawn;
        std::vector<glm::vec3> npcSpawns;
        getDuelSpawnPositions(world, playerSpawn, npcSpawns, config.numNpcs);
        player.pos = playerSpawn;
        player.respawnPosition = playerSpawn;
    }

    alivePlayerCount = 1;
    aliveNpcCount = config.numNpcs;
}

void DuelManager::beginFight(Player& player, NpcSystem& npcs, World& world)
{
    currentPhase = DuelPhase::Active;
    timer = 0.0f;

    {
        glm::vec3 playerSpawn;
        std::vector<glm::vec3> npcSpawns;
        getDuelSpawnPositions(world, playerSpawn, npcSpawns, config.numNpcs);
        for (int i = 0; i < config.numNpcs; ++i) {
            glm::vec3 pos = i < (int)npcSpawns.size() ? npcSpawns[i]
                : player.pos + glm::vec3(
                    cosf((6.2831853f * i) / std::max(1, config.numNpcs)) * 8.0f,
                    sinf((6.2831853f * i) / std::max(1, config.numNpcs)) * 8.0f,
                    1.0f);
            npcs.spawnNpc(config.npcDifficulty, pos);
        }
    }

    printf("[DUEL] FIGHT round=%d\n", currentRound);
}

void DuelManager::update(float dt, Player& player, NpcSystem& npcs, World& world, Camera& camera)
{
    (void)camera;
    if (!config.enabled) return;

    switch (currentPhase) {
        case DuelPhase::Off:
            break;

        case DuelPhase::Countdown:
            countdown -= dt;
            if (countdown <= 0.0f)
                beginFight(player, npcs, world);
            break;

        case DuelPhase::Active:
            timer += dt;

            // timeout = enemies win
            if (config.duelLengthSeconds > 0 &&
                timer >= config.duelLengthSeconds)
            {
                endRound(DuelTeam::NPC);
                return;
            }

            break;
        case DuelPhase::RoundEnd:
            roundEndTimer -= dt;
            if (roundEndTimer <= 0.0f) {
                currentRound++;

                resetRoundEntities(player, npcs, world);
                startCountdown();
            }
            break;

        case DuelPhase::MatchEnd:
            if (!matchOverCaptured) {
                matchOverCameraTarget = player.pos;
                matchOverCaptured = true;
            }
            if (!matchOverButtonsShown) {
                matchOverTimer -= dt;
                if (matchOverTimer <= 0.0f) {
                    matchOverButtonsShown = true;
                }
            }
            break;
    }
}

void DuelManager::onPlayerKill(int npcIndex)
{
    if (currentPhase != DuelPhase::Active) return;

    playerKills++;
    playerStats.kills++;
    playerStats.points += 100;
    playerStats.xp += 50;

    printf("[DUEL] player kill total=%d\n", playerKills);
}

void DuelManager::onNpcKill(int npcIndex)
{
    if (currentPhase != DuelPhase::Active) return;

    if (npcIndex >= 0 && npcIndex < (int)npcKills.size()) {
        npcKills[npcIndex]++;
    }
    playerStats.deaths++;

    printf("[DUEL] npc kill npc=%d\n", npcIndex);
}

void DuelManager::onEntityDeath(DuelTeam team)
{
    if (currentPhase != DuelPhase::Active)
        return;

    if (team == DuelTeam::Player)
    {
        alivePlayerCount--;

        printf(
            "[DUEL] player died alivePlayerCount=%d\n",
            alivePlayerCount
        );

        if (alivePlayerCount <= 0)
        {
            endRound(DuelTeam::NPC);
        }
    }
    else if (team == DuelTeam::NPC)
    {
        aliveNpcCount--;

        printf(
            "[DUEL] npc died aliveNpcCount=%d\n",
            aliveNpcCount
        );

        if (aliveNpcCount <= 0)
        {
            endRound(DuelTeam::Player);
        }
    }
}

void DuelManager::endRound(DuelTeam winner)
{
    currentPhase = DuelPhase::RoundEnd;
    roundEndTimer = 3.0f;

    if (winner == DuelTeam::Player)
    {
        playerRoundsWon_++;
        playerStats.roundsWon++;
    }
    else
    {
        npcRoundsWon_++;
    }

    printf(
        "[DUEL] round %d ended winner=%d playerKills=%d\n",
        currentRound,
        (int)winner,
        playerKills
    );

    if (playerRoundsWon_ >= config.killsToWin)
    {
        endMatch();
        return;
    }

    if (npcRoundsWon_ >= config.killsToWin)
    {
        endMatch();
        return;
    }
}

void DuelManager::endMatch()
{
    currentPhase = DuelPhase::MatchEnd;
    playerStats.matchesWon++;

    if (playerRoundsWon_ >= config.killsToWin)
        matchWinner_ = DuelTeam::Player;
    else
        matchWinner_ = DuelTeam::NPC;

    matchOverTimer = 4.0f;
    matchOverButtonsShown = false;
    matchOverCaptured = false;

    printf("[DUEL] match ended winner=%s totalKills=%d totalDeaths=%d\n",
           matchWinner_ == DuelTeam::Player ? "PLAYER" : "NPC",
           playerStats.kills, playerStats.deaths);
}

void DuelManager::setMapList(const std::vector<std::string>& maps)
{
    mapList = maps;
}

void DuelManager::rotateMap(World& world)
{
    if (mapList.empty()) return;
    currentMapIndex = (currentMapIndex + 1) % mapList.size();
    config.mapPath = mapList[currentMapIndex];
    loadWorldFromGLB(world, config.mapPath.c_str());
    printf("[DUEL] rotated to map: %s\n", config.mapPath.c_str());
}

void DuelManager::renderHud()
{
    if (!config.enabled) return;

    float cx = uiScreenW() * 0.5f;

    if (currentPhase == DuelPhase::Countdown) {
        char text[64];
        snprintf(text, sizeof(text), "%.0f", std::ceil(countdown));
        uiDrawText(text, cx - 20.0f, uiScreenH() * 0.5f - 40.0f, 1.2f, {1, 1, 1, 1});
        return;
    }

    if (currentPhase == DuelPhase::Active) {
        char scoreText[64];
        snprintf(
            scoreText,
            sizeof(scoreText),
            "%d - %d",
            playerRoundsWon_,
            npcRoundsWon_);
        uiDrawText(scoreText, cx - 60.0f, 40.0f, 0.8f, {1, 0.85f, 0.25f, 1});

        float remaining = std::max(0.0f, (float)config.duelLengthSeconds - timer);
        char timerText[32];
        snprintf(timerText, sizeof(timerText), "%.0f", remaining);
        uiDrawText(timerText, cx - 25.0f, 90.0f, 0.45f, {1, 1, 1, 0.8f});

        char roundText[32];
        snprintf(roundText, sizeof(roundText), "Round %d", currentRound);
        uiDrawText(roundText, cx - 40.0f, 130.0f, 0.32f, {0.7f, 0.8f, 1.0f, 0.9f});
        return;
    }

    if (currentPhase == DuelPhase::RoundEnd) {
        uiDrawText("ROUND OVER", cx - 100.0f, uiScreenH() * 0.5f, 0.7f, {1, 0.85f, 0.25f, 1});
        char nextText[64];
        snprintf(nextText, sizeof(nextText), "Next round in %.0f...", std::ceil(roundEndTimer));
        uiDrawText(nextText, cx - 80.0f, uiScreenH() * 0.5f + 50.0f, 0.38f, {1, 1, 1, 1});
        return;
    }

    if (currentPhase == DuelPhase::MatchEnd) {
        return;
    }
}

DuelMenuAction DuelManager::renderMatchOverScreen(GLFWwindow* win)
{
    if (currentPhase != DuelPhase::MatchEnd)
        return DuelMenuAction::None;

    // Force cursor unlocked so UI hover/click works.
    // This runs AFTER gDuelManager.update() changes phase to MatchEnd,
    // so the start-of-frame duel phase check hasn't unlocked it yet.
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    float cx = uiScreenW() * 0.5f;
    float cy = uiScreenH() * 0.5f;

    // Winner announcement
    const char* winnerText = (matchWinner_ == DuelTeam::Player) ? "YOU WIN!" : "NPC WINS!";
    glm::vec4 winnerColor = (matchWinner_ == DuelTeam::Player)
        ? glm::vec4(0.3f, 1.0f, 0.3f, 1.0f)
        : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
    uiDrawText(winnerText, cx - 100.0f, cy - 140.0f, 1.0f, winnerColor);

    // Score
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "%d - %d", playerRoundsWon_, npcRoundsWon_);
    uiDrawText(scoreText, cx - 30.0f, cy - 60.0f, 0.6f, {1, 0.85f, 0.25f, 1});

    // Stats
    char statsText[128];
    snprintf(statsText, sizeof(statsText), "Kills: %d | Deaths: %d | Points: %d | XP: %d",
             playerStats.kills, playerStats.deaths, playerStats.points, playerStats.xp);
    uiDrawText(statsText, cx - 200.0f, cy - 20.0f, 0.38f, {1, 1, 1, 1});

    if (!matchOverButtonsShown) {
        char countdownText[64];
        snprintf(countdownText, sizeof(countdownText), "Match ends in %.0f...", std::ceil(matchOverTimer));
        uiDrawText(countdownText, cx - 100.0f, cy + 40.0f, 0.38f, {1, 1, 1, 0.7f});
        return DuelMenuAction::None;
    }

    // Buttons
    float btnW = 280.0f;
    float btnH = 54.0f;
    float btnX = cx - btnW * 0.5f;
    float btnY = cy + 60.0f;
    float gap = 20.0f;

    if (uiButton(win, "Play Again", {btnX, btnY, btnW, btnH}, {0.24f, 0.82f, 0.48f, 1.0f}).clicked)
        return DuelMenuAction::PlayAgain;

    if (uiButton(win, "Exit To Main Menu", {btnX, btnY + btnH + gap, btnW, btnH}, {0.86f, 0.3f, 0.3f, 1.0f}).clicked)
        return DuelMenuAction::ExitToMenu;

    return DuelMenuAction::None;
}

void DuelManager::restartDuel(Player& player, NpcSystem& npcs, World& world)
{
    printf("[DUEL] restarting duel with same config\n");

    npcs.destroyAll();
    player.dead = false;
    player.currentHp = 100.0f;
    player.vel = glm::vec3(0.0f);
    player.externalImpulse = glm::vec3(0.0f);
    player.respawnTimer = 0.0f;
    player.killedBy.clear();

    currentPhase = DuelPhase::Countdown;
    countdown = 3.0f;
    timer = 0.0f;
    currentRound = 1;
    playerRoundsWon_ = 0;
    npcRoundsWon_ = 0;
    playerKills = 0;
    npcKills.assign(config.numNpcs, 0);
    alivePlayerCount = 1;
    aliveNpcCount = config.numNpcs;
    playerStats = DuelStats{};
    matchOverCaptured = false;
    matchOverButtonsShown = false;
    matchOverTimer = 0.0f;

    {
        glm::vec3 playerSpawn;
        std::vector<glm::vec3> npcSpawns;
        getDuelSpawnPositions(world, playerSpawn, npcSpawns, config.numNpcs);
        player.pos = playerSpawn;
        player.respawnPosition = playerSpawn;
    }

    printf("[DUEL] restart complete\n");
}

void DuelManager::stopDuel()
{
    printf("[DUEL] stopping duel\n");
    currentPhase = DuelPhase::Off;
    config.enabled = false;
    playerStats = DuelStats{};
    matchOverCaptured = false;
    matchOverButtonsShown = false;
    matchOverTimer = 0.0f;
}
