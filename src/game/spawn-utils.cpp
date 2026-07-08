#include "spawn-utils.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"
#include "game/spawn-override.h"
#include "debug/debug-log.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

static FILE* gSpawnDebugFile = nullptr;
static void spawnDebugOpen()
{
    if (!gSpawnDebugFile)
        gSpawnDebugFile = fopen("logs/map_spawn_debug.txt", "a");
}
static void spawnDebugClose()
{
    if (gSpawnDebugFile) {
        fclose(gSpawnDebugFile);
        gSpawnDebugFile = nullptr;
    }
}
#define SPAWNLOG(...) do { spawnDebugOpen(); if (gSpawnDebugFile) { fprintf(gSpawnDebugFile, __VA_ARGS__); fflush(gSpawnDebugFile); } } while(0)

glm::vec3 getSpawnPosition(const World& world, int entityIndex) {
    glm::vec3 overridePos;
    if (tryGetSpawnOverride(overridePos)) {
        Debug::log(Debug::Category::General, "[SPAWN] override active position=(%.1f %.1f %.1f)\n",
                   overridePos.x, overridePos.y, overridePos.z);
        return overridePos;
    }
    if (!world.spawnPoints.empty()) {
        int idx = entityIndex % (int)world.spawnPoints.size();
        glm::vec3 pos = world.spawnPoints[idx].position;
        printf("[SPAWN] using spawnpoint %d/%zu position=(%.2f, %.2f, %.2f)\n",
               idx, world.spawnPoints.size(), pos.x, pos.y, pos.z);
        return pos;
    }
    printf("[SPAWN] no spawnpoints found — using fallback at (0, 0, 100)\n");
    return FALLBACK_SPAWN_POS;
}

glm::vec3 spawnNpcAtSafePosition(NpcSystem& npcs, uint32_t npcId, float difficulty,
                                  const World& world, int entityIndex) {
    bool explicitSpawn = world.spawnPoints.empty() ? false : true;
    glm::vec3 basePos = getSpawnPosition(world, entityIndex);

    glm::vec3 spawnPos = basePos;
    // Skip random offset when override is active — spawn exactly at override position
    glm::vec3 _ignored;
    if (!tryGetSpawnOverride(_ignored)) {
        spawnPos += glm::vec3(
            (float)(rand() % 40 - 20) * 0.5f,
            (float)(rand() % 40 - 20) * 0.5f,
            0.0f
        );
    }

    // Clamp Z to at least 1 unit above the map surface
    // The fallback position at (0,0,100) already ensures this
    if (spawnPos.z < 1.0f && world.spawnPoints.empty())
        spawnPos.z = FALLBACK_SPAWN_POS.z;

    npcs.spawnNpc(npcId, difficulty, spawnPos);

    int spawnIdx = world.spawnPoints.empty() ? -1 : (entityIndex % (int)world.spawnPoints.size());
    printf("[SPAWN NPC] id=%u difficulty=%.1f position=(%.2f, %.2f, %.2f) "
           "spawnpoint=%d totalNpcs=%zu\n",
           npcId, difficulty, spawnPos.x, spawnPos.y, spawnPos.z,
           spawnIdx, npcs.all().size());

    SPAWNLOG("NPC spawned id=%u difficulty=%.1f explicit=%s spawn_index=%d position=(%.2f %.2f %.2f)\n",
             npcId, difficulty, explicitSpawn ? "yes" : "no", spawnIdx,
             spawnPos.x, spawnPos.y, spawnPos.z);

    return spawnPos;
}

void logSpawnDiagnostics(const World& world, const Player& player, NpcSystem& npcs) {
    printf("[SPAWN DIAG] spawnpoints=%zu player.pos=(%.2f,%.2f,%.2f) player.dead=%d player.hp=%d\n",
           world.spawnPoints.size(),
           player.pos.x, player.pos.y, player.pos.z,
           (int)player.dead, player.currentHp);

    for (size_t i = 0; i < npcs.all().size(); ++i) {
        const Npc& n = npcs.all()[i];
        printf("[SPAWN DIAG] npc[%zu] id=%u pos=(%.2f,%.2f,%.2f) dead=%d hp=%d bombHolder=%d\n",
               i, n.id, n.body.pos.x, n.body.pos.y, n.body.pos.z,
               (int)n.body.dead, n.body.currentHp,
               (int)n.bombTagHasBomb);
    }
}
