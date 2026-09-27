// 2026-09-27
/* purpose
* One authoritative actor-triangle collision solver.
* Sweeps every actor collision mesh (body parts + weapon) from its safe previous
* pose to its desired pose, builds one contact manifold, applies one position
* correction, one velocity response per distinct surface, and a final
* penetration validation.
* First version. It runs alongside the existing pipeline until Phase 7; it is
* not yet called by doCollisions.
* Does NOT render, play effects, send packets, or own movement reset formulas.
* Does NOT delete or replace the legacy owners.
*/
#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

class Player;
class World;

// One triangle contact between an actor part and the world.
struct ActorWorldContact {
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 point{0.0f};
    glm::vec3 impactVelocity{0.0f};   // part displacement this tick (sweepDelta)
    float penetration = 0.0f;
    float timeOfImpact = 0.0f;        // reserved; not yet computed
    int worldTriangle = -1;
    const char* actorPart = nullptr;
};

struct ActorTriangleCollisionResult {
    glm::vec3 startPos{0.0f};
    glm::vec3 correctedPos{0.0f};
    glm::vec3 remainingMovement{0.0f};
    std::vector<ActorWorldContact> contacts;
    bool grounded = false;
    bool anyImpact = false;
    float maxPenetration = 0.0f;
    int candidates = 0;
    int iterations = 0;
};

// Solves the actor's movement for one tick. The caller must have already applied
// the desired pose (player transforms previous = safe pose, world = desired
// pose). Mutates player.pos and actor/weapon sweep-start transforms. Returns
// true when any contact was found.
bool solveActorTriangleCollision(
    Player& player,
    const World& world,
    const glm::vec3& desiredMovement,
    ActorTriangleCollisionResult& result);

// Deterministic Phase 3 test: synthetic box actor vs floor, wall, corner, and a
// leaving-old-contact case.
bool actorTriangleSolverSelfTest(std::string* outSummary = nullptr);
