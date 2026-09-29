#pragma once

struct CollisionTraceSnapshot;
class Player;
class World;

// TODO-DELETE: legacy capsule floor recovery. Superseded by
// solveActorTriangleCollision (actor-triangle-solver.h), but still called by
// the legacy pipeline when "actorTriangleSolver" is off. See the
// TODO-DELETE comment at the definition in physics-collision-glb-safety.cpp.
void doFloorRecovery(Player& p, const World& world, bool& groundedThisFrame);
