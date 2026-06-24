#pragma once
#include <vector>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <tinygltf/tiny_gltf.h>
#include "map_common.h"

void walkGLBScene(
    const tinygltf::Model& model,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh,
    int sceneIndex,
    Mesh* skyMesh = nullptr);

void generateTriangleNormals(std::vector<Vertex>& verts, size_t first, size_t count);
