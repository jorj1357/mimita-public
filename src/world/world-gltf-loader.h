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

bool loadWorldFromGLB(
    World& world,
    const char* path
);

void extractSpawnPointsFromGLB(World& world, const char* path);