#pragma once

#include <glm/glm.hpp>
#include <vector>

struct CollisionTraceSnapshot;
class Player;
class World;

// TODO-DELETE: legacy root-capsule sweep/slide declaration. Superseded by
// solveActorTriangleCollision (actor-triangle-solver.h). See the TODO-DELETE
// comment at the definition in physics-collision-glb-sweep-slide.cpp for the
// human-testing and step-up conditions that must be met before removal.
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
