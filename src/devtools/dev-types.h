#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <glm/glm.hpp>

class Npc;
class NpcSystem;
class Player;
class Camera;
struct GLFWwindow;

enum class TeleportTargetType {
    LocalPlayer,
    NamedPlayer,
    Coordinates,
    SpawnPoint
};

struct TeleportParams {
    TeleportTargetType targetType = TeleportTargetType::LocalPlayer;
    std::string playerName;
    glm::vec3 coordinates{0.0f, 0.0f, 50.0f};
    int spawnPointIndex = 0;
};

struct DevBinding {
    int key = 0;
    std::string action;
    std::string description;
};

struct DevCommand {
    std::string name;
    std::string description;
    std::string usage;
    std::function<void(const std::vector<std::string>& args)> handler;
    bool requireSelection = false;
};

using DevCommandFn = std::function<void(const std::vector<std::string>& args)>;
