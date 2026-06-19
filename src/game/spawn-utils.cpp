#include "spawn-utils.h"
#include "world/world.h"
#include "entities/player.h"
#include "npc/npc.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

glm::vec3 getSpawnPosition(const World& world, int entityIndex) {
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
    glm::vec3 basePos = getSpawnPosition(world, entityIndex);

    // Add slight random offset to prevent overlapping spawns
    glm::vec3 spawnPos = basePos + glm::vec3(
        (float)(rand() % 40 - 20) * 0.5f,
        (float)(rand() % 40 - 20) * 0.5f,
        0.0f
    );

    // Clamp Z to at least 1 unit above the map surface
    // The fallback position at (0,0,100) already ensures this
    if (spawnPos.z < 1.0f && world.spawnPoints.empty())
        spawnPos.z = FALLBACK_SPAWN_POS.z;

    npcs.spawnNpc(npcId, difficulty, spawnPos);

    printf("[SPAWN NPC] id=%u difficulty=%.1f position=(%.2f, %.2f, %.2f) "
           "spawnpoint=%d totalNpcs=%zu\n",
           npcId, difficulty, spawnPos.x, spawnPos.y, spawnPos.z,
           entityIndex, npcs.all().size());

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
