#pragma once

#include "dev-types.h"
#include <vector>

class Npc;

void TeleportSelectedToTarget(const std::vector<const Npc*>& npcs, const glm::vec3& targetPos);
void TeleportSelectedToPlayer(const std::vector<const Npc*>& npcs, const std::string& playerName);
void TeleportSelectedToCoords(const std::vector<const Npc*>& npcs, const glm::vec3& coords);
void TeleportSelectedToSpawnPoint(const std::vector<const Npc*>& npcs, int index);
void RegisterTeleportCommands();
