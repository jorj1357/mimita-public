#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

struct World;
struct SpawnPoint;
class Player;
class NpcSystem;

// Shared fallback spawn position when no map spawnpoints exist.
// 100 meters above origin so entities fall onto the map rather than
// spawning inside geometry or below the world.
constexpr glm::vec3 FALLBACK_SPAWN_POS = glm::vec3(0.0f, 0.0f, 100.0f);

// Get a spawn position from the world's spawnpoints.
// If spawnpoints exist, returns the position at the given index (cycled).
// If no spawnpoints exist, returns FALLBACK_SPAWN_POS.
glm::vec3 getSpawnPosition(const World& world, int entityIndex = 0);

// Spawn an NPC at a valid position using shared spawn logic.
// Returns the NPC's spawn position (for logging).
glm::vec3 spawnNpcAtSafePosition(NpcSystem& npcs, uint32_t npcId, float difficulty,
                                  const World& world, int entityIndex = 0);

// Log the spawn state of all entities for diagnostics.
void logSpawnDiagnostics(const World& world, const Player& player, NpcSystem& npcs);
