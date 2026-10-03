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

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

class Player;
class World;
struct PhysicalEntity;

// One triangle contact between an actor part and the world.
struct ActorWorldContact {
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 point{0.0f};
    glm::vec3 impactVelocity{0.0f};   // part displacement this tick (sweepDelta)
    float penetration = 0.0f;
    float timeOfImpact = 0.0f;        // reserved; not yet computed
    int worldTriangle = -1;
    const char* actorPart = nullptr;
    // Support entity for a moving-entity contact (0 = static world) and its
    // surface velocity, used to carry the actor and inherit velocity on departure.
    uint32_t entityId = 0;
    glm::vec3 surfaceVelocity{0.0f};
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
// When `entities` is non-null, moving physical entities participate alongside
// static world triangles and their contacts carry the entity id and surface
// velocity. The default (null) preserves static-world-only behavior.
bool solveActorTriangleCollision(
    Player& player,
    const World& world,
    const glm::vec3& desiredMovement,
    ActorTriangleCollisionResult& result,
    const std::vector<PhysicalEntity>* entities = nullptr,
    float dt = 1.0f / 60.0f);

// Deterministic Phase 3 test: synthetic box actor vs floor, wall, corner, and a
// leaving-old-contact case.
bool actorTriangleSolverSelfTest(std::string* outSummary = nullptr);

// Active-path wrapper for the local player: integrates the tick's intended
// movement into the desired pose, runs the single triangle solver, reports
// grounding, and preserves the body-contact spark boundary. Returns true when
// the actor solver handled the step (world has triangles and the actor has body
// triangles); false lets the caller fall back to the legacy pipeline.
bool runActorTriangleCollisionStep(
    Player& player,
    const World& world,
    bool& groundedThisFrame,
    float dt);
