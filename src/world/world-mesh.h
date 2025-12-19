// C:\important\quiet\n\mimita-public\mimita-public\src\world\world-mesh.h
// dec 18 2025
/**
 * purpose 
 * header for HARD CODING LOGIC RENDERING the objects we have
 * NOT an obj not a mesh just render it
 * 1:1 i see it and its real and collidable
 */

#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/vec2.hpp>   // <-- REQUIRED
#include "world/world.h"

// BLENDER IS Z-UP
// MIMITA IS Y-UP
// static inline glm::vec3 toYUp(glm::vec3 p) {
//     return { p.x, p.z, -p.y };
// }

struct WorldVertex {
    glm::vec3 pos;
    glm::vec2 uv;
    float texIndex;
    glm::vec3 normal;
};

void buildWorldMesh(
    const World& world,
    std::vector<WorldVertex>& outVerts
);
