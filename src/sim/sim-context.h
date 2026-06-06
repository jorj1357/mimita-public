#pragma once

#include <cstdint>

struct Player;
struct World;
class NpcSystem;

struct SimContext {
    Player* player = nullptr;
    World* world = nullptr;
    NpcSystem* npcSystem = nullptr;
    float randomSeed = 0.0f;
    uint64_t tick = 0;
};
