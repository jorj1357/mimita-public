#include "dev-teleport.h"
#include "dev-commands.h"
#include "dev-npc-selection.h"
#include "npc/npc.h"
#include "entities/player.h"
#include "debug/debug-log.h"
#include <cstdio>

void TeleportSelectedToTarget(const std::vector<const Npc*>& npcs, const glm::vec3& targetPos) {
    for (const Npc* npc : npcs) {
        Npc* mutableNpc = const_cast<Npc*>(npc);
        mutableNpc->body.pos = targetPos;
        mutableNpc->body.vel = {0.0f, 0.0f, 0.0f};
        mutableNpc->body.syncLegacyStateToLayers();
        
        Debug::log(Debug::Category::General, 
                   "[DEV] Teleported NPC %u to (%.1f, %.1f, %.1f)\n",
                   npc->id, targetPos.x, targetPos.y, targetPos.z);
    }
}

void TeleportSelectedToPlayer(const std::vector<const Npc*>& npcs, const std::string& playerName) {
    (void)playerName;
    printf("[DEV] Teleport to named player not yet implemented (need player registry)\n");
}

void TeleportSelectedToCoords(const std::vector<const Npc*>& npcs, const glm::vec3& coords) {
    TeleportSelectedToTarget(npcs, coords);
}

void TeleportSelectedToSpawnPoint(const std::vector<const Npc*>& npcs, int index) {
    static const glm::vec3 spawnPoints[] = {
        {0.0f, 0.0f, 50.0f},
        {-10.0f, 0.0f, 50.0f},
        {10.0f, 0.0f, 50.0f},
        {0.0f, -10.0f, 50.0f},
        {0.0f, 10.0f, 50.0f}
    };
    
    if (index >= 0 && index < (int)(sizeof(spawnPoints)/sizeof(spawnPoints[0]))) {
        TeleportSelectedToTarget(npcs, spawnPoints[index]);
    } else {
        printf("[DEV] Invalid spawn point index %d\n", index);
    }
}

namespace {
void CmdTeleportToMe(const std::vector<std::string>& args) {
    (void)args;
    auto selected = NpcSelectionManager::instance().getSelected(*reinterpret_cast<NpcSystem*>(0x1));
    printf("[DEV] Need NpcSystem reference for teleport to me\n");
}

void CmdTeleportToCoords(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        printf("[DEV] Usage: tp_coords <x> <y> <z>\n");
        return;
    }
    glm::vec3 coords{
        std::stof(args[0]),
        std::stof(args[1]),
        std::stof(args[2])
    };
    auto selected = NpcSelectionManager::instance().getSelected(*reinterpret_cast<NpcSystem*>(0x1));
    printf("[DEV] Need NpcSystem reference for teleport to coords\n");
}

void CmdTeleportToSpawnPoint(const std::vector<std::string>& args) {
    int index = args.empty() ? 0 : std::stoi(args[0]);
    auto selected = NpcSelectionManager::instance().getSelected(*reinterpret_cast<NpcSystem*>(0x1));
    printf("[DEV] Need NpcSystem reference for teleport to spawn point\n");
}
}

void RegisterTeleportCommands() {
    DevCommandRegistry::instance().registerCommand({
        "tp_selected_to_me",
        "Teleport selected NPCs to local player",
        "tp_selected_to_me",
        [](const auto& args) {
            auto& selection = NpcSelectionManager::instance();
            // Need access to player position - will be hooked up in main.cpp
            printf("[DEV] tp_selected_to_me - hook up player position in main.cpp\n");
        },
        true
    });
    
    DevCommandRegistry::instance().registerCommand({
        "tp_selected_to_coords",
        "Teleport selected NPCs to coordinates",
        "tp_selected_to_coords <x> <y> <z>",
        [](const auto& args) {
            if (args.size() < 3) {
                printf("[DEV] Usage: tp_selected_to_coords <x> <y> <z>\n");
                return;
            }
            glm::vec3 coords{std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
            auto selected = NpcSelectionManager::instance().getSelected(*reinterpret_cast<NpcSystem*>(0x1));
            printf("[DEV] tp_selected_to_coords - hook up NpcSystem in main.cpp\n");
        },
        true
    });
    
    DevCommandRegistry::instance().registerCommand({
        "tp_selected_to_spawn",
        "Teleport selected NPCs to spawn point",
        "tp_selected_to_spawn [index]",
        [](const auto& args) {
            int index = args.empty() ? 0 : std::stoi(args[0]);
            auto selected = NpcSelectionManager::instance().getSelected(*reinterpret_cast<NpcSystem*>(0x1));
            printf("[DEV] tp_selected_to_spawn - hook up NpcSystem in main.cpp\n");
        },
        true
    });
}
