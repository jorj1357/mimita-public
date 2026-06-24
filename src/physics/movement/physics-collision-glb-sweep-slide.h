#pragma once

#include <glm/glm.hpp>
#include <vector>

struct CollisionTraceSnapshot;
class Player;
class World;

void doGLBSweepSlide(
    Player& p,
    const World& world,
    bool& groundedThisFrame,
    float dt,
    const glm::vec3& totalMove,
    glm::vec3& remainingMove,
    CollisionTraceSnapshot& trace,
    std::vector<int>& candidates
);
