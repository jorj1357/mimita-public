#pragma once
#include <vector>
#include <string>
#include <glad/glad.h>
#include <tinygltf/tiny_gltf.h>

void processGLBMaterials(
    tinygltf::Model& model,
    const std::string& glbDir,
    std::vector<GLuint>& imageTextures,
    std::vector<GLuint>& materialTextures,
    std::vector<GLuint>& colorTextures);
