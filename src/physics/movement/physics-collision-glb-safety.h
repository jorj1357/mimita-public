#pragma once

struct CollisionTraceSnapshot;
class Player;
class World;

void doGroundSnap(Player& p, const World& world, bool& groundedThisFrame);
void doFloorRecovery(Player& p, const World& world);
void doRotationSafetyPass(Player& p, const World& world, bool& groundedThisFrame, CollisionTraceSnapshot& trace);
void doFinalSafetyPass(Player& p, const World& world, CollisionTraceSnapshot& trace);
