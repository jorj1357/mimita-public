// C:\important\mimita-priv-v8\src\world\world-gltf-loader.h
// 5 23 2026
/** purpose
 * header for the loader gltf
 * i want to to use 
 * points, lines btwn points, faces between lines
 * for the world objects so we can do spheres and cones and triangles etc 
 */

#pragma once

#include "world/world.h"
#include <cstdint>

struct MapLoadTiming {
    double read_glb = 0;
    double parse_glb = 0;
    double materials_decode = 0;
    double texture_upload = 0;
    double scene_walk = 0;
    double vertex_expansion = 0;
    double batch_merge = 0;
    double collision_triangles = 0;
    double collision_chunks = 0;
    double gpu_buffer_upload = 0;
    double world_commit = 0;
    double total = 0;
};

struct MapLoadMetrics {
    uint64_t glbFileBytes = 0;
    int meshCount = 0;
    int nodeCount = 0;
    int primitiveCount = 0;
    int imageCount = 0;
    uint64_t totalDecodedImageBytes = 0;
    uint64_t totalSourceVertices = 0;
    uint64_t totalSourceIndices = 0;
    uint64_t finalExpandedVertexCount = 0;
    uint64_t renderTriangleCount = 0;
    uint64_t collisionTriangleCount = 0;
    uint64_t collisionChunkCount = 0;
    uint64_t totalTriangleToChunkRefs = 0;
    uint64_t maxChunksTouchedByOneTriangle = 0;
    double maxTriangleBoundsSize = 0;
    int largestImageW = 0;
    int largestImageH = 0;
    uint64_t largestPrimitiveVertexCount = 0;
    uint64_t largestPrimitiveIndexCount = 0;
};

bool loadWorldFromGLB(
    World& world,
    const char* path
);

void extractSpawnPointsFromGLB(World& world, const char* path);