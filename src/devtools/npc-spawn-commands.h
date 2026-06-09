#pragma once

#include <vector>
#include <string>

class NpcSystem;
class Camera;
class World;
class Player;
struct GLFWwindow;

void QueueNpcSpawnCommand(const std::vector<std::string>& args);
void ProcessNpcSpawnCommands(NpcSystem& npcSystem, const Camera& camera, const World& world, const Player& player);

extern int gNpcTrainingMode;
extern int gNpcTrainingHealth;

void QueueNpcTrainingSpawnCommand(const std::vector<std::string>& args);
void ProcessNpcTrainingSpawnCommands(NpcSystem& npcSystem, const Camera& camera, const World& world, const Player& player);
