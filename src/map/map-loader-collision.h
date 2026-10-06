#pragma once
#include <vector>
#include "physics/physics-types.h"

class World;
struct MapLoadMetrics;

// A triangle is "large" when it would touch more than this many collision
// chunks. Shared by buildCollisionChunks (classification) and the actor-solve
// diagnostics (large-triangle candidate count) so both use one threshold.
inline constexpr int kMaxChunksPerTriangle = 256;

void buildCollisionMeshFromRenderMesh(World& world);
void buildCollisionChunks(World& world, MapLoadMetrics* metrics = nullptr);
void buildCollisionSubGrids(World& world);
void redecimateCollision(World& world);
void decimateCollisionTriangleList(std::vector<CollisionTriangle>& tris, float cellSize);
