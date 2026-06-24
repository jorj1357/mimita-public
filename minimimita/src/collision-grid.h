#pragma once
#include "types.h"
#include <vector>

struct CollisionGrid {
    float cellSize = 2.0f;
    float originX = 0, originY = 0;
    int cellsX = 0, cellsY = 0;
    std::vector<std::vector<int>> cells;
    bool valid = false;
    void build(const std::vector<Triangle>& triangles, float cellSize_);
    void clear();
};

struct CollisionProfile {
    double totalTimeMs = 0;
    int collectTimeUs = 0;
    int resolveTimeUs = 0;
    int trianglesTested = 0;
    int contactsGenerated = 0;
    int depenetrationIters = 0;
    int queriesPerFrame = 0;
};

void buildCollisionGrid(const std::vector<Triangle>& triangles, float cellSize = 2.0f);
void clearCollisionGrid();
void collectContacts(const Player& player, const std::vector<Triangle>& triangles,
                     ContactState& state);
void resolveContactsIterative(Player& player, const std::vector<Triangle>& triangles,
                              ContactState& state);
const CollisionProfile& getCollisionProfile();
void resetCollisionProfile();
void printCollisionProfile();
