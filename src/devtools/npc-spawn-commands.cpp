// C:\important\mimita-priv-v8\src\devtools\npc-spawn-commands.cpp
// NPC spawn command handling

#include "npc-spawn-commands.h"
#include "npc/npc.h"
#include "camera.h"
#include "world/world.h"
#include "entities/player.h"
#include "devtools/dev-overlay.h"
#include <cstdio>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

static std::vector<std::vector<std::string>> gPendingNpcSpawnCommands;

void QueueNpcSpawnCommand(const std::vector<std::string>& args) {
    gPendingNpcSpawnCommands.push_back(args);
}

void ProcessNpcSpawnCommands(NpcSystem& npcSystem, const Camera& camera, const World& world, const Player& player) {
    for (const auto& args : gPendingNpcSpawnCommands) {
        glm::vec3 spawnPos;
        float difficulty = 1.0f;
        bool atCursor = false;
        
        if (args.empty() || args[0] == "cursor") {
            // Raycast from camera to find spawn position
            atCursor = true;
            glm::vec3 rayOrigin = camera.pos;
            glm::vec3 rayDir = camera.front;
            
            // Raycast against world (simple plane at z=0 for now)
            if (std::fabs(rayDir.z) > 0.0001f) {
                float t = (0.0f - rayOrigin.z) / rayDir.z;
                if (t > 0.0f && t < 100.0f) {
                    spawnPos = rayOrigin + rayDir * t;
                    spawnPos.z += 1.0f; // Spawn slightly above ground
                } else {
                    spawnPos = player.pos + camera.front * 5.0f;
                    spawnPos.z = player.pos.z;
                }
            } else {
                spawnPos = player.pos + camera.front * 5.0f;
                spawnPos.z = player.pos.z;
            }
            
            if (args.size() >= 2) {
                difficulty = std::stof(args[1]);
            }
        } else if (args.size() >= 3) {
            // Explicit position: spawn.npc <x> <y> <z> [difficulty]
            spawnPos.x = std::stof(args[0]);
            spawnPos.y = std::stof(args[1]);
            spawnPos.z = std::stof(args[2]);
            if (args.size() >= 4) {
                difficulty = std::stof(args[3]);
            }
        } else {
            // Default: spawn in front of player
            spawnPos = player.pos + camera.front * 5.0f;
            spawnPos.z = player.pos.z + 1.0f;
            if (args.size() >= 1) {
                difficulty = std::stof(args[0]);
            }
        }
        
        // Clamp difficulty
        difficulty = std::clamp(difficulty, 1.0f, 10.0f);
        
        npcSystem.spawnNpc(difficulty, spawnPos);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "Spawned NPC at (%.1f, %.1f, %.1f) difficulty %.1f", 
                 spawnPos.x, spawnPos.y, spawnPos.z, difficulty);
        DevOverlay::instance().showNotification(msg, 3.0f);
        printf("[NPC SPAWN] %s\n", msg);
    }
    gPendingNpcSpawnCommands.clear();
}

int gNpcTrainingMode = 0;
int gNpcTrainingHealth = 100;

static std::vector<std::vector<std::string>> gPendingTrainingSpawnCommands;

void QueueNpcTrainingSpawnCommand(const std::vector<std::string>& args) {
    gPendingTrainingSpawnCommands.push_back(args);
}

void ProcessNpcTrainingSpawnCommands(NpcSystem& npcSystem, const Camera& camera, const World& world, const Player& player) {
    for (const auto& args : gPendingTrainingSpawnCommands) {
        glm::vec3 spawnPos;
        float difficulty = 1.0f;

        if (args.empty() || args[0] == "cursor") {
            glm::vec3 rayOrigin = camera.pos;
            glm::vec3 rayDir = camera.front;
            if (std::fabs(rayDir.z) > 0.0001f) {
                float t = (0.0f - rayOrigin.z) / rayDir.z;
                if (t > 0.0f && t < 100.0f) {
                    spawnPos = rayOrigin + rayDir * t;
                    spawnPos.z += 1.0f;
                } else {
                    spawnPos = player.pos + camera.front * 5.0f;
                    spawnPos.z = player.pos.z;
                }
            } else {
                spawnPos = player.pos + camera.front * 5.0f;
                spawnPos.z = player.pos.z;
            }
            if (args.size() >= 2) difficulty = std::stof(args[1]);
        } else if (args.size() >= 3) {
            spawnPos.x = std::stof(args[0]);
            spawnPos.y = std::stof(args[1]);
            spawnPos.z = std::stof(args[2]);
            if (args.size() >= 4) difficulty = std::stof(args[3]);
        } else {
            spawnPos = player.pos + camera.front * 5.0f;
            spawnPos.z = player.pos.z + 1.0f;
            if (args.size() >= 1) difficulty = std::stof(args[0]);
        }

        difficulty = std::clamp(difficulty, 1.0f, 10.0f);

        npcSystem.spawnNpc(difficulty, spawnPos);

        // Apply training settings to the last spawned NPC
        if (!npcSystem.all().empty()) {
            Npc& npc = const_cast<Npc&>(npcSystem.all().back());
            npc.trainingMode = gNpcTrainingMode;
            if (gNpcTrainingHealth > 0)
                npc.body.maxHp = gNpcTrainingHealth;
            npc.body.currentHp = npc.body.maxHp;
        }

        char msg[128];
        snprintf(msg, sizeof(msg), "Spawned training NPC mode=%d health=%d at (%.1f %.1f %.1f)",
                 gNpcTrainingMode, gNpcTrainingHealth,
                 spawnPos.x, spawnPos.y, spawnPos.z);
        DevOverlay::instance().showNotification(msg, 3.0f);
        printf("[TRAINING NPC] %s\n", msg);
    }
    gPendingTrainingSpawnCommands.clear();
}
