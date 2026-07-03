#include "game/duel.h"
#include "competitive/competitive-match.h"
#include "combat/weapon-runtime.h"

#include <cstdio>
#include <algorithm>
#include <cmath>

#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "debug/debug-log.h"

void DuelManager::beginFight(Player& player, NpcSystem& npcs, World& world)
{
    currentPhase = DuelPhase::Active;
    timer = 0.0f;

    {
        for (int i = 0; i < config.numNpcs; ++i) {
            glm::vec3 spawnPos = getTeamSpawn(DuelTeam::NPC, i, config.numNpcs);
            uint32_t id = npcs.nextNpcId();
            npcs.spawnNpc(id, config.npcDifficulty, spawnPos);
            Debug::log(Debug::Category::Duel, "[DUEL SPAWN] NPC %d id=%u team=NPC spawn=(%.2f %.2f %.2f)",
                       i, id, spawnPos.x, spawnPos.y, spawnPos.z);
        }
    }

    Debug::log(Debug::Category::Duel, "[DUEL] FIGHT round=%d", currentRound);
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
    (void)world;
    npcs.destroyAll();

    resetAllWeaponRuntimesForSpawn(player, "DuelManager::resetRoundEntities");
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

            switch (duelEndState) {
            case DuelEndState::VictoryScreen:
                victoryTimer -= dt;
                if (victoryTimer <= 0.0f) {
                    duelEndState = DuelEndState::Countdown;
                    countdownTimer = 3.0f;
                    currentCountdownNumber = 3;
                    Debug::log(Debug::Category::Duel, "[DUEL] MatchEnd -> Countdown");
                }
                break;

            case DuelEndState::Countdown:
            {
                static int prevCountdownNum = 999;
                int prevNum = currentCountdownNumber;
                countdownTimer -= dt;
                currentCountdownNumber = (int)std::ceil(countdownTimer);
                if (currentCountdownNumber < 0) currentCountdownNumber = 0;
                if (currentCountdownNumber != prevCountdownNum) {
                    if (currentCountdownNumber >= 0 && currentCountdownNumber <= 3)
                        Debug::log(Debug::Category::Duel, "[DUEL] Countdown %d", currentCountdownNumber);
                    prevCountdownNum = currentCountdownNumber;
                }
                if (countdownTimer <= 0.0f) {
                    prevCountdownNum = 999;
                    Debug::log(Debug::Category::Duel, "[DUEL] Countdown 0");
                    duelEndState = DuelEndState::FinalKillReplay;
                    currentCountdownNumber = 0;
                    Debug::log(Debug::Category::Duel, "[DUEL] Starting Final Kill Replay (replayReady=%d)", (int)replayReady);
                }
                break;
            }

            case DuelEndState::FinalKillReplay:
            case DuelEndState::ReplayMenu:
            case DuelEndState::None:
                break;
            }
            break;
    }
}

void DuelManager::endRound(DuelTeam winner)
{
    Debug::log(Debug::Category::Duel, "[DUEL FLOW] RoundEnd winner=%s scores=%d-%d",
               winner == DuelTeam::Player ? "PLAYER" : "NPC",
               playerRoundsWon_, npcRoundsWon_);
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

    Debug::log(Debug::Category::Duel, "[DUEL] round %d ended winner=%d playerKills=%d newScores=%d-%d",
               currentRound, (int)winner, playerKills, playerRoundsWon_, npcRoundsWon_);

    if (playerRoundsWon_ >= config.killsToWin || npcRoundsWon_ >= config.killsToWin)
    {
        Debug::log(Debug::Category::Duel, "[DUEL FLOW] RoundEnd -> MatchEnd (killsToWin=%d)", config.killsToWin);
        endMatch();
        return;
    }
}

void DuelManager::endMatch()
{
    Debug::log(Debug::Category::Duel, "[DUEL] Active -> MatchEnd winner=%s",
               playerRoundsWon_ >= config.killsToWin ? "PLAYER" : "NPC");
    currentPhase = DuelPhase::MatchEnd;
    playerStats.matchesWon++;

    bool playerWon = playerRoundsWon_ >= config.killsToWin;
    if (playerWon)
        matchWinner_ = DuelTeam::Player;
    else
        matchWinner_ = DuelTeam::NPC;

    // Hook for competitive duels
    if (isCompetitiveMatchActive())
        onCompetitiveMatchEnd(*this, playerWon);

    duelEndState = DuelEndState::VictoryScreen;
    victoryTimer = 3.0f;
    countdownTimer = 0.0f;
    currentCountdownNumber = 0;
    replayReady = false;
    matchOverCaptured = false;
    matchEndTick = 0;
    finalKillSavedOnce = false;

    Debug::log(Debug::Category::Duel, "[DUEL FLOW] VictoryScreen start (3s) winner=%s",
               matchWinner_ == DuelTeam::Player ? "YOU WIN" : "YOU LOSE");
    Debug::log(Debug::Category::Duel, "[DUEL] match ended winner=%s totalKills=%d totalDeaths=%d",
               matchWinner_ == DuelTeam::Player ? "PLAYER" : "NPC",
               playerStats.kills, playerStats.deaths);
}
