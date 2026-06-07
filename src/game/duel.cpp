// C:\important\mimita-priv-v8\src\game\duel.cpp
// 6 7 2026 
/** purpose
 * duels first game mode
 * first to 5, 100 hp, spawn with revolver and shotgun, small close range map
 */

#include "game/duel.h"

#include <cstdio>
#include <algorithm>

#include "entities/player.h"
#include "npc/npc.h"
#include "world/world.h"
#include "camera.h"
#include "gui/ui-system.h"

void DuelManager::start(const DuelConfig& cfg, Player& player, NpcSystem& npcs)
{
    config = cfg;
    config.enabled = true;

    currentPhase = DuelPhase::Countdown;
    countdown = 3.0f;
    timer = 0.0f;

    playerKills = 0;
    npcKills.assign(config.numNpcs, 0);

    npcs.destroyAll();

    printf("[DUEL] started countdown npcs=%d killsToWin=%d\n",
           config.numNpcs, config.killsToWin);
}

void DuelManager::beginFight(Player& player, NpcSystem& npcs)
{
    currentPhase = DuelPhase::Active;

    glm::vec3 center = player.pos;

    for (int i = 0; i < config.numNpcs; ++i) {
        float angle = (6.2831853f * i) / std::max(1, config.numNpcs);
        glm::vec3 pos = center + glm::vec3(cosf(angle) * 8.0f, sinf(angle) * 8.0f, 1.0f);

        npcs.spawnNpc(config.npcDifficulty, pos);

        // optional later: assign npc name here if npc system exposes it
    }

    printf("[DUEL] FIGHT\n");
}

void DuelManager::update(float dt, Player& player, NpcSystem& npcs, World& world, Camera& camera)
{
    (void)world;
    (void)camera;

    if (!config.enabled) return;

    if (currentPhase == DuelPhase::Countdown) {
        countdown -= dt;

        if (countdown <= 0.0f) {
            beginFight(player, npcs);
        }

        return;
    }

    if (currentPhase == DuelPhase::Active) {
        timer += dt;

        if (config.duelLengthSeconds > 0 && timer >= config.duelLengthSeconds) {
            endDuel();
            return;
        }

        if (playerKills >= config.killsToWin) {
            endDuel();
            return;
        }

        for (int kills : npcKills) {
            if (kills >= config.killsToWin) {
                endDuel();
                return;
            }
        }
    }
}

void DuelManager::onPlayerKill(int npcIndex)
{
    if (currentPhase != DuelPhase::Active) return;

    playerKills++;

    printf("[DUEL] player kill npc=%d total=%d\n", npcIndex, playerKills);
}

void DuelManager::onNpcKill(int npcIndex)
{
    if (currentPhase != DuelPhase::Active) return;

    if (npcIndex >= 0 && npcIndex < (int)npcKills.size()) {
        npcKills[npcIndex]++;
    }

    printf("[DUEL] npc kill npc=%d\n", npcIndex);
}

void DuelManager::endDuel()
{
    currentPhase = DuelPhase::Ended;
    printf("[DUEL] ended playerKills=%d\n", playerKills);
}

void DuelManager::renderHud()
{
    if (!config.enabled) return;

    if (currentPhase == DuelPhase::Countdown) {
        char text[64];
        snprintf(text, sizeof(text), "%.0f", std::ceil(countdown));
        uiDrawText(text, uiScreenW() * 0.5f - 20.0f, uiScreenH() * 0.5f - 40.0f,
                   1.2f, {1, 1, 1, 1});
        return;
    }

    if (currentPhase == DuelPhase::Active) {
        char text[128];
        snprintf(text, sizeof(text), "DUEL | Kills: %d/%d | Time: %.1f",
                 playerKills, config.killsToWin, timer);
        uiDrawText(text, 24, 300, 0.38f, {1, 0.85f, 0.25f, 1});
        return;
    }

    if (currentPhase == DuelPhase::Ended) {
        uiDrawText("DUEL ENDED", uiScreenW() * 0.5f - 100.0f, uiScreenH() * 0.5f,
                   0.7f, {1, 1, 1, 1});
    }
}