#pragma once
#include <vector>
#include "physics/physics-types.h"

class World;
struct MapLoadMetrics;

void buildCollisionMeshFromRenderMesh(World& world);
void buildCollisionChunks(World& world, MapLoadMetrics* metrics = nullptr);
void buildCollisionSubGrids(World& world);
void redecimateCollision(World& world);
void decimateCollisionTriangleList(std::vector<CollisionTriangle>& tris, float cellSize);
