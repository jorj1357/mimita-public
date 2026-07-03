#include "game/duel.h"
#include "combat/weapon-runtime.h"

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
#include "gui/gui-element-render.h"
#include "gui/ui-system.h"
#include "gui/gui-editor.h"
#include "debug/debug-log.h"
#include "replay/replay-export.h"

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

    std::vector<int> arena0, arena1, unassigned;
    for (int i = 0; i < (int)world.spawnPoints.size(); ++i) {
        const auto& sp = world.spawnPoints[i];
        if (sp.arenaIndex == 0) arena0.push_back(i);
        else if (sp.arenaIndex == 1) arena1.push_back(i);
        else unassigned.push_back(i);
    }

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

    int teamBIdx = -1;
    if (!arena1.empty()) {
        unsigned int seed = 67890;
        teamBIdx = arena1[randomInt(0, (int)arena1.size() - 1, seed)];
    } else {
        float bestDist = 1e30f;
        for (int i = 0; i < (int)world.spawnPoints.size(); ++i) {
            if (i == teamAIdx) continue;
            float d = glm::length(world.spawnPoints[i].position - mTeamASpawn);
            if (d < bestDist) {
                bestDist = d;
                teamBIdx = i;
            }
        }
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
    victoryTimer = 0.0f;
    countdownTimer = 0.0f;
    currentCountdownNumber = 0;
    replayReady = false;
    duelEndState = DuelEndState::None;

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

void DuelManager::onPlayerKill(int npcIndex)
{
    (void)npcIndex;
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

    GuiLayout& hudLayout = GuiLayoutManager::instance().getLayout("config/gui/duel-hud.json");

    auto drawTextElement = [&](const std::string& id, const std::string& text) {
        const GuiElement* el = hudLayout.get(id);
        if (!el) return;
        float scale = el->fontSize > 0.0f ? el->fontSize : 0.32f;
        glm::vec4 color = el->getTextColorVec();
        uiDrawText(text.c_str(), uiScaleX(el->x), uiScaleY(el->y), scale, color);
    };

    if (currentPhase == DuelPhase::Countdown) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f", std::ceil(countdown));
        drawTextElement("countdownText", buf);
        return;
    }

    if (currentPhase == DuelPhase::Active) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d - %d", playerRoundsWon_, npcRoundsWon_);
        drawTextElement("scoreText", buf);

        float remaining = std::max(0.0f, (float)config.duelLengthSeconds - timer);
        snprintf(buf, sizeof(buf), "%.0f", remaining);
        drawTextElement("timerText", buf);

        snprintf(buf, sizeof(buf), "Round %d", currentRound);
        drawTextElement("roundText", buf);
        return;
    }

    if (currentPhase == DuelPhase::RoundEnd) {
        drawTextElement("roundOverText", "ROUND OVER");
        char buf[64];
        snprintf(buf, sizeof(buf), "Next round in %.0f...", std::ceil(roundEndTimer));
        drawTextElement("roundOverNextText", buf);
        return;
    }
}

DuelMenuAction DuelManager::renderMatchOverScreen(GLFWwindow* win)
{
    if (currentPhase != DuelPhase::MatchEnd)
        return DuelMenuAction::None;

    uiSetEditMode(false);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    float sw = uiScreenW();
    float sh = uiScreenH();

    if (duelEndState == DuelEndState::VictoryScreen) {
        uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.6f}, "victory-dim");

        const char* winnerText = (matchWinner_ == DuelTeam::Player) ? "YOU WIN" : "YOU LOSE";
        glm::vec4 winnerColor = (matchWinner_ == DuelTeam::Player)
            ? glm::vec4(0.3f, 1.0f, 0.3f, 1.0f)
            : glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
        uiDrawText(winnerText, sw * 0.5f - 180.0f, sh * 0.4f, 1.5f, winnerColor);

        char scoreText[32];
        snprintf(scoreText, sizeof(scoreText), "%d - %d", playerRoundsWon_, npcRoundsWon_);
        uiDrawText(scoreText, sw * 0.5f - 60.0f, sh * 0.4f + 80.0f, 0.8f, {1, 0.85f, 0.25f, 1});

        return DuelMenuAction::None;
    }

    if (duelEndState == DuelEndState::Countdown) {
        uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.6f}, "countdown-dim");

        char numText[8];
        snprintf(numText, sizeof(numText), "%d", currentCountdownNumber);
        uiDrawText(numText, sw * 0.5f - 30.0f, sh * 0.5f - 40.0f, 2.0f, {1.0f, 1.0f, 1.0f, 1.0f});

        return DuelMenuAction::None;
    }

    GuiEditor::instance().setActiveLayout("config/gui/duel-match-end.json");

    if (duelEndState == DuelEndState::FinalKillReplay) {
        uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.15f}, "fk-bg");

        uiDrawRect({sw - 280.0f, sh - 80.0f, 260.0f, 44.0f}, {0.0f, 0.0f, 0.0f, 0.6f}, "fk-save-bg");

        const ReplayExportJob& job = getReplayExportJob();
        bool exportBusy = (job.state == ReplayExportJob::Capturing || job.state == ReplayExportJob::Encoding);
        bool exportDone = (job.state == ReplayExportJob::Done);
        bool exportFailed = (job.state == ReplayExportJob::Failed);

        const char* btnLabel = "Save Replay";
        glm::vec4 btnColor = {0.2f, 0.6f, 0.3f, 1.0f};
        if (exportBusy) {
            btnLabel = "Saving Replay...";
            btnColor = {0.5f, 0.5f, 0.2f, 1.0f};
        } else if (exportDone) {
            btnLabel = "Replay Saved!";
            btnColor = {0.2f, 0.8f, 0.3f, 1.0f};
        } else if (exportFailed) {
            btnLabel = "Export Failed";
            btnColor = {0.8f, 0.2f, 0.2f, 1.0f};
        }

        UIRect saveRect = {sw - 280.0f, sh - 80.0f, 260.0f, 44.0f};
        UIButtonState saveBtn = uiButton(win, btnLabel, saveRect, btnColor, "duel-save-replay-fk");
        if (!exportBusy && saveBtn.clicked) {
            Debug::log(Debug::Category::Duel, "[DUEL FLOW] Save Replay clicked during FinalKillReplay");
            return DuelMenuAction::SaveReplay;
        }

        std::string status = getReplayExportStatusText();
        if (!status.empty()) {
            uiDrawText(status.c_str(), saveRect.x - 10.0f, saveRect.y + saveRect.h + 4.0f, 0.28f, {1.0f, 1.0f, 1.0f, 0.9f});
        }

        if (!replayReady) {
            uiDrawText("FINAL KILL REPLAY FAILED", sw * 0.5f - 180.0f, sh * 0.5f,
                       0.6f, {1.0f, 0.3f, 0.3f, 1.0f});
        }

        return DuelMenuAction::None;
    }

    if (duelEndState == DuelEndState::ReplayMenu) {
        uiDrawRect({0, 0, sw, sh}, {0.0f, 0.0f, 0.0f, 0.5f}, "replay-menu-dim");

        GuiLayout& duelLayout = GuiLayoutManager::instance().getLayout("config/gui/duel-match-end.json");

        const GuiElement* playEl = duelLayout.get("Play Again");
        if (playEl && drawGuiElement(win, *playEl).clicked)
            return DuelMenuAction::PlayAgain;

        const GuiElement* exitEl = duelLayout.get("Exit To Main Menu");
        if (exitEl && drawGuiElement(win, *exitEl).clicked)
            return DuelMenuAction::ExitToMenu;

        {
            const ReplayExportJob& job = getReplayExportJob();
            bool exportBusy = (job.state == ReplayExportJob::Capturing || job.state == ReplayExportJob::Encoding);
            bool exportDone = (job.state == ReplayExportJob::Done);
            bool exportFailed = (job.state == ReplayExportJob::Failed);

            UIRect sr = duelLayout.getRectDesign("Save Replay", {830.0f, 572.0f, 260.0f, 44.0f});

            const char* btnLabel = "Save Replay";
            glm::vec4 btnColor = {0.2f, 0.6f, 0.3f, 1.0f};
            if (exportBusy) {
                btnLabel = "Saving Replay...";
                btnColor = {0.5f, 0.5f, 0.2f, 1.0f};
            } else if (exportDone) {
                btnLabel = "Replay Saved!";
                btnColor = {0.2f, 0.8f, 0.3f, 1.0f};
            } else if (exportFailed) {
                btnLabel = "Export Failed";
                btnColor = {0.8f, 0.2f, 0.2f, 1.0f};
            }

            UIButtonState saveBtn = uiButton(win, btnLabel, sr, btnColor, "duel-save-replay");
            if (!exportBusy && saveBtn.clicked)
                return DuelMenuAction::SaveReplay;

            std::string status = getReplayExportStatusText();
            if (!status.empty()) {
                uiDrawText(status.c_str(), sr.x - 10.0f, sr.y + sr.h + 4.0f, 0.28f, {1.0f, 1.0f, 1.0f, 0.9f});
            }
        }

        return DuelMenuAction::None;
    }

    return DuelMenuAction::None;
}

void DuelManager::restartDuel(Player& player, NpcSystem& npcs, World& world)
{
    Debug::log(Debug::Category::Duel, "[DUEL] Play Again clicked");
    Debug::log(Debug::Category::Duel, "[DUEL] Restarting duel with existing settings");

    config.enabled = true;

    npcs.destroyAll();

    for (int i = 0; i < config.numNpcs; ++i) {
        glm::vec3 spawnPos = getTeamSpawn(DuelTeam::NPC, i, config.numNpcs);
        uint32_t id = npcs.nextNpcId();
        npcs.spawnNpc(id, config.npcDifficulty, spawnPos);
        Debug::log(Debug::Category::Duel, "[DUEL SPAWN] NPC %d id=%u team=NPC pos=(%.2f %.2f %.2f)",
                   i, id, spawnPos.x, spawnPos.y, spawnPos.z);
    }

    resetAllWeaponRuntimesForSpawn(player, "DuelManager::restartDuel");
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
    victoryTimer = 0.0f;
    countdownTimer = 0.0f;
    currentCountdownNumber = 0;
    replayReady = false;
    duelEndState = DuelEndState::None;
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
    victoryTimer = 0.0f;
    countdownTimer = 0.0f;
    currentCountdownNumber = 0;
    replayReady = false;
    duelEndState = DuelEndState::None;
    matchEndTick = 0;
    finalKillSavedOnce = false;
    finalKillReplayPath.clear();
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
