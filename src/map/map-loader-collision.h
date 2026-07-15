#pragma once
class World;
struct MapLoadMetrics;

void buildCollisionMeshFromRenderMesh(World& world);
void buildCollisionChunks(World& world, MapLoadMetrics* metrics = nullptr);
