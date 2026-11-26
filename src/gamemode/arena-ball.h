#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

struct MapObjectiveConfig;
struct PhysicalEntity;

// Server-owned Vehicle Arena ball. The ball is one generic dynamic
// PhysicalEntity sphere; this owner only creates and resets it from authored map
// data. Motion, collision, and replication stay in the shared physics and
// physical-entity replication owners.
namespace ArenaBall
{
using ArenaBallSpawn = uint32_t;

// Spawns the map's arena.ball as a replicated, server-simulated dynamic sphere.
// Returns the network id, or 0 when the map has no arena ball or spawn fails.
ArenaBallSpawn spawnFromMap(const MapObjectiveConfig& config,
                            std::string* error = nullptr);

// Returns the verse ball to its authored spawn with zero velocity, angular
// velocity, residual sleep state, and contact state.
bool reset(PhysicalEntity& ball, const MapObjectiveConfig& config);

// Owned-by-the-ball test surface. Motion, collision, and replication stay with
// the shared physics owner; these only place the ball and report it, so ball
// behaviour can be measured from a journal without a human driving.
PhysicalEntity* find();
void setState(PhysicalEntity& ball, const glm::vec3& position,
              const glm::vec3& velocity);
// Test surface: apply now when the ball exists, otherwise remember the request
// and apply it as soon as the arena ball spawns (a server startup command runs
// before map load finishes).
void requestState(const glm::vec3& position, const glm::vec3& velocity);
void tick();
void registerArenaBallCommands();
}
