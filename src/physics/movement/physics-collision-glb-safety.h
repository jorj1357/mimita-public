#pragma once

struct CollisionTraceSnapshot;
class Player;
class World;

// TODO-DELETE: legacy capsule safety/ground helpers. Superseded by
// solveActorTriangleCollision (actor-triangle-solver.h). doGroundSnap,
// doRotationSafetyPass, and doFinalSafetyPass are already dead (no callers).
// doFloorRecovery is only called by the legacy pipeline when
// "actorTriangleSolver" is off. See the TODO-DELETE comments at the definitions
// in physics-collision-glb-safety.cpp for removal conditions.
void doGroundSnap(Player& p, const World& world, bool& groundedThisFrame);
void doFloorRecovery(Player& p, const World& world, bool& groundedThisFrame);
void doRotationSafetyPass(Player& p, const World& world, bool& groundedThisFrame, CollisionTraceSnapshot& trace);
void doFinalSafetyPass(Player& p, const World& world, CollisionTraceSnapshot& trace);
