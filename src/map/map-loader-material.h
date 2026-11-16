#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <tinygltf/tiny_gltf.h>

void processGLBMaterials(
    tinygltf::Model& model,
    const std::string& glbDir,
    std::vector<std::uint32_t>& imageTextures,
    std::vector<std::uint32_t>& materialTextures,
    std::vector<std::uint32_t>& colorTextures);
