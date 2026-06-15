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

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "world/world-gltf-loader.h"
#include "camera.h"
#include "gui/gui-layout.h"
#include "gui/ui-system.h"
#include "gui/gui-editor.h"
#include "debug/debug-log.h"

// -------------------------------------------------------
// Team-based spawn assignment
//
// Maps may have spawn points tagged with arena_N:
//   arena_0 = Team A spawns
//   arena_1 = Team B spawns
//
// If arena tags are absent, the first two spawn points
// are used (Team A = spawn 0, Team B = nearest different spawn).
// -------------------------------------------------------

static int randomInt(int min, int max, unsigned int& seed)
{
    seed = seed * 1664525u + 1013904223u;
    return min + (int)(((seed >> 8) & 0x00ffffffu) % (unsigned int)(max - min + 1));
}

static float randomFloat(float min, float max, unsigned int& seed)
{
    seed = seed * 1664525u + 1013904223u;
    float t = (float)((seed >> 8) & 0x00ffffffu) / (float)0x01000000u;
    return min + t * (max - min);
}

void DuelManager::assignTeamSpawns(const World& world)
{
    mTeamASpawnIndex = -1;
    mTeamBSpawnIndex = -1;
    mTeamASpawn = glm::vec3(1.0f, 5.0f, 60.0f);
    mTeamBSpawn = glm::vec3(1.0f, 5.0f, 60.0f);

    if (world.spawnPoints.empty()) {
        Debug::log(Debug::Category::Duel, "[DUEL SPAWN] no spawn points in map");
        return;
    }

    // Group spawns by arenaIndex
    std::vector<int> arena0, arena1, unassigned;
    for (int i = 0; i < (int)world.spawnPoints.size(); ++i) {
        const auto& sp = world.spawnPoints[i];
        if (sp.arenaIndex == 0) arena0.push_back(i);
        else if (sp.arenaIndex == 1) arena1.push_back(i);
        else unassigned.push_back(i);
    }

    // Team A: prefer arena_0, else first unassigned spawn, else first spawn
    int teamAIdx = -1;
    if (!arena0.empty()) {
        unsigned int seed = 12345;
        teamAIdx = arena0[randomInt(0, (int)arena0.size() - 1, seed)];
    } else if (!unassigned.empty()) {
        teamAIdx = unassigned[0];
    } else {
        teamAIdx = 0;
    }
    mTeamASpawnIndex = teamAIdx;
    mTeamASpawn = world.spawnPoints[teamAIdx].position;

    // Team B: prefer arena_1, else nearest spawn that is NOT Team A's spawn
    int teamBIdx = -1;
    if (!arena1.empty()) {
        unsigned int seed = 67890;
        teamBIdx = arena1[randomInt(0, (int)arena1.size() - 1, seed)];
    } else {
        // Find nearest spawn point that isn't Team A's
        float bestDist = 1e30f;
        for (int i = 0; i < (int)world.spawnPoints.size(); ++i) {
            if (i == teamAIdx) continue;
            // Prefer using a different unassigned or arena_1 if available
            float d = glm::length(world.spawnPoints[i].position - mTeamASpawn);
            if (d < bestDist) {
                bestDist = d;
                teamBIdx = i;
            }
        }
        // Fallback: if no other spawn, use the same but with offset
        if (teamBIdx < 0) teamBIdx = teamAIdx;
    }
    mTeamBSpawnIndex = teamBIdx;
    mTeamBSpawn = world.spawnPoints[teamBIdx].position;

    Debug::log(Debug::Category::Duel, "[DUEL SPAWN] Team A -> Spawn %d (%.2f, %.2f, %.2f)",
               mTeamASpawnIndex, mTeamASpawn.x, mTeamASpawn.y, mTeamASpawn.z);
    Debug::log(Debug::Category::Duel, "[DUEL SPAWN] Team B -> Spawn %d (%.2f, %.2f, %.2f)",
               mTeamBSpawnIndex, mTeamBSpawn.x, mTeamBSpawn.y, mTeamBSpawn.z);

    if (teamAIdx == teamBIdx && world.spawnPoints.size() >= 1) {
        Debug::log(Debug::Category::Duel, "[DUEL SPAWN] WARNING: teams share the same spawn point");
    }
    if (world.spawnPoints.size() < 2) {
        Debug::log(Debug::Category::Duel, "[DUEL SPAWN] WARNING: need at least 2 spawn points for separate team spawns");
    }
}

glm::vec3 DuelManager::getTeamSpawn(DuelTeam team, int entityIndex, int totalOnTeam) const
{
    glm::vec3 basePos = (team == DuelTeam::Player) ? mTeamASpawn : mTeamBSpawn;

    // Add small random offset when multiple entities share a spawn
    if (totalOnTeam > 1) {
        unsigned int seed = 99991u + (unsigned int)entityIndex * 7477u;
        float angle = randomFloat(0.0f, 6.2831853f, seed);
        float radius = randomFloat(0.5f, 2.5f, seed);
        return basePos + glm::vec3(cosf(angle) * radius, sinf(angle) * radius, 0.0f);
    }
    return basePos;
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

    assignTeamSpawns(world);
    {
        player.pos = getTeamSpawn(DuelTeam::Player, 0, 1);
        player.respawnPosition = player.pos;
    }

    Debug::log(Debug::Category::Duel, "[DUEL] started npcs=%d killsToWin=%d map=%s",
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
        player.pos = getTeamSpawn(DuelTeam::Player, 0, 1);
        player.respawnPosition = player.pos;
    }

    alivePlayerCount = 1;
    aliveNpcCount = config.numNpcs;
}

void DuelManager::beginFight(Player& player, NpcSystem& npcs, World& world)
{
    currentPhase = DuelPhase::Active;
    timer = 0.0f;

    {
        for (int i = 0; i < config.numNpcs; ++i) {
            glm::vec3 pos = getTeamSpawn(DuelTeam::NPC, i, config.numNpcs);
            npcs.spawnNpc(config.npcDifficulty, pos);
            Debug::log(Debug::Category::Duel, "[DUEL SPAWN] NPC %d team=NPC spawn=(%.2f %.2f %.2f)",
                       i, pos.x, pos.y, pos.z);
        }
    }

    Debug::log(Debug::Category::Duel, "[DUEL] FIGHT round=%d", currentRound);
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

    Debug::log(Debug::Category::Duel, "[DUEL] player kill total=%d", playerKills);
}

void DuelManager::onNpcKill(int npcIndex)
{
    if (currentPhase != DuelPhase::Active) return;

    if (npcIndex >= 0 && npcIndex < (int)npcKills.size()) {
        npcKills[npcIndex]++;
    }
    playerStats.deaths++;

    Debug::log(Debug::Category::Duel, "[DUEL] npc kill npc=%d", npcIndex);
}

void DuelManager::onEntityDeath(DuelTeam team)
{
    if (currentPhase != DuelPhase::Active)
        return;

    if (team == DuelTeam::Player)
    {
        alivePlayerCount--;

        Debug::log(Debug::Category::Duel, "[DUEL] player died alivePlayerCount=%d", alivePlayerCount);

        if (alivePlayerCount <= 0)
        {
            endRound(DuelTeam::NPC);
        }
    }
    else if (team == DuelTeam::NPC)
    {
        aliveNpcCount--;

        Debug::log(Debug::Category::Duel, "[DUEL] npc died aliveNpcCount=%d", aliveNpcCount);

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

    Debug::log(Debug::Category::Duel, "[DUEL] round %d ended winner=%d playerKills=%d",
               currentRound, (int)winner, playerKills);

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

    matchOverTimer = 3.0f;
    matchOverButtonsShown = false;
    matchOverCaptured = false;
    finalKillReplayActive = false;
    finalKillReplayLoaded = false;
    finalKillReplayTime = 0.0f;
    matchEndTick = 0;
    finalKillSavedOnce = false;

    Debug::log(Debug::Category::Duel, "[DUEL] match ended winner=%s totalKills=%d totalDeaths=%d",
               matchWinner_ == DuelTeam::Player ? "PLAYER" : "NPC",
               playerStats.kills, playerStats.deaths);
    Debug::log(Debug::Category::Duel, "[DUEL] entering result screen");
    Debug::log(Debug::Category::Duel, "[DUEL] result screen timer=%.1f", matchOverTimer);
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
    Debug::log(Debug::Category::Duel, "[DUEL] rotated to map: %s", config.mapPath.c_str());
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

    // Ensure GUI edit mode is off so buttons actually fire
    uiSetEditMode(false);

    // Force cursor unlocked for click detection
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    // Set active layout for GUI editor
    GuiEditor::instance().setActiveLayout("config/gui/duel-match-end.json");

    float sw = uiScreenW();
    float sh = uiScreenH();

    Debug::log(Debug::Category::Duel, "[DUEL UI] MatchEnd layout=left_panel");

    // Light full-screen dim so replay stays visible
    uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.15f}, "duel-end-dim");

    // Left-side menu panel
    float panelX = 40.0f;
    float panelY = sh * 0.5f - 140.0f;
    float panelW = 300.0f;
    float panelH = 290.0f;
    uiDrawRect({panelX, panelY, panelW, panelH}, {0.0f, 0.0f, 0.0f, 0.75f}, "duel-end-panel");

    // Winner announcement
    const char* winnerText = (matchWinner_ == DuelTeam::Player) ? "YOU WIN!" : "NPC WINS!";
    glm::vec4 winnerColor = (matchWinner_ == DuelTeam::Player)
        ? glm::vec4(0.3f, 1.0f, 0.3f, 1.0f)
        : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
    uiDrawText(winnerText, panelX + 20.0f, panelY + 12.0f, 0.85f, winnerColor);

    // Score
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "%d - %d", playerRoundsWon_, npcRoundsWon_);
    uiDrawText(scoreText, panelX + 20.0f, panelY + 58.0f, 0.55f, {1, 0.85f, 0.25f, 1});

    // Stats
    char statsText[128];
    snprintf(statsText, sizeof(statsText), "Kills: %d | Deaths: %d | Points: %d | XP: %d",
             playerStats.kills, playerStats.deaths, playerStats.points, playerStats.xp);
    uiDrawText(statsText, panelX + 20.0f, panelY + 88.0f, 0.34f, {1, 1, 1, 1});

    bool canShowButtons = matchOverButtonsShown;

    // During result screen phase (before replay starts), show countdown instead of buttons
    if (!canShowButtons && finalKillReplayTime < 0.5f && matchOverTimer > 0.0f) {
        char countdownText[64];
        snprintf(countdownText, sizeof(countdownText), "Match ends in %.0f...", std::ceil(matchOverTimer));
        uiDrawText(countdownText, panelX + 20.0f, panelY + 125.0f, 0.38f, {1, 1, 1, 0.7f});
        return DuelMenuAction::None;
    }

    // Show buttons after the replay slow-motion phase completes
    if (!canShowButtons && finalKillReplayActive && finalKillReplayTime > 6.0f) {
        canShowButtons = true;
        matchOverButtonsShown = true;
    }

    if (!canShowButtons) {
        return DuelMenuAction::None;
    }

    GuiLayout& duelLayout = GuiLayoutManager::instance().getLayout("config/gui/duel-match-end.json");

    // Buttons use design-coordinate layout (centered in 1920x1080 design space)
    {
        UIRect pr = duelLayout.getRectDesign("Play Again", {830.0f, 460.0f, 260.0f, 44.0f});
        UIButtonState playBtn = uiButton(win, "Play Again", pr, {0.24f, 0.82f, 0.48f, 1.0f});
        if (playBtn.clicked) {
            return DuelMenuAction::PlayAgain;
        }
    }

    {
        UIRect er = duelLayout.getRectDesign("Exit To Main Menu", {830.0f, 516.0f, 260.0f, 44.0f});
        UIButtonState exitBtn = uiButton(win, "Exit To Main Menu", er, {0.86f, 0.3f, 0.3f, 1.0f});
        if (exitBtn.clicked) {
            return DuelMenuAction::ExitToMenu;
        }
    }

    // Save Replay button, shown when final kill replay is active
    if (finalKillReplayActive) {
        UIRect sr = duelLayout.getRectDesign("Save Replay", {830.0f, 572.0f, 260.0f, 44.0f});
        UIButtonState saveBtn = uiButton(win, "Save Replay", sr, {0.2f, 0.6f, 0.3f, 1.0f});
        if (saveBtn.clicked) {
            return DuelMenuAction::SaveReplay;
        }
    }

    return DuelMenuAction::None;
}

void DuelManager::restartDuel(Player& player, NpcSystem& npcs, World& world)
{
    Debug::log(Debug::Category::Duel, "[DUEL] Play Again clicked");
    Debug::log(Debug::Category::Duel, "[DUEL] Restarting duel with existing settings");

    config.enabled = true;

    npcs.destroyAll();

    // Respawn NPCs with saved config
    for (int i = 0; i < config.numNpcs; ++i) {
        glm::vec3 spawnPos = getTeamSpawn(DuelTeam::NPC, i, config.numNpcs);
        npcs.spawnNpc(config.npcDifficulty, spawnPos);
    }

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
    finalKillReplayActive = false;
    finalKillReplayLoaded = false;
    finalKillReplayTime = 0.0f;
    matchEndTick = 0;
    finalKillSavedOnce = false;

    assignTeamSpawns(world);
    {
        player.pos = getTeamSpawn(DuelTeam::Player, 0, 1);
        player.respawnPosition = player.pos;
    }

    Debug::log(Debug::Category::Duel, "[DUEL] restart complete");
}

void DuelManager::stopDuel()
{
    Debug::log(Debug::Category::Duel, "[DUEL] Exit To Main Menu clicked");
    Debug::log(Debug::Category::Duel, "[DUEL] Returning to main menu");
    Debug::log(Debug::Category::Duel, "[DUEL] duel UI cleared");
    currentPhase = DuelPhase::Off;
    config.enabled = false;
    playerStats = DuelStats{};
    matchOverCaptured = false;
    matchOverButtonsShown = false;
    matchOverTimer = 0.0f;
    finalKillReplayActive = false;
    finalKillReplayLoaded = false;
    finalKillReplayTime = 0.0f;
    finalKillSlowMoFactor = 1.0f;
    matchEndTick = 0;
    finalKillSavedOnce = false;
    finalKillReplayPath.clear();
    finalKillKillerId.clear();
    finalKillVictimId.clear();
    currentRound = 1;
    playerRoundsWon_ = 0;
    npcRoundsWon_ = 0;
    playerKills = 0;
    npcKills.clear();
    alivePlayerCount = 0;
    aliveNpcCount = 0;
    duelFrozen_ = false;
    timer = 0.0f;
    roundEndTimer = 0.0f;
    countdown = 0.0f;
    matchWinner_ = DuelTeam::Player;
    matchOverCameraTarget = glm::vec3(0.0f);
    Debug::log(Debug::Category::Duel, "[DUEL] stopDuel complete — all state cleared");
}
