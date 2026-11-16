#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <tinygltf/tiny_gltf.h>
#include "map_common.h"

void walkGLBScene(
    const tinygltf::Model& model,
    const std::vector<std::uint32_t>& materialTextures,
    Mesh& mesh,
    int sceneIndex,
    Mesh* skyMesh = nullptr);

// Resolves the shared default texture, lazily on first use. Must be called from
// the main thread at least once before background GLB parse threads run.
std::uint32_t getGLBDefaultTexture();

void generateTriangleNormals(std::vector<Vertex>& verts, size_t first, size_t count);
