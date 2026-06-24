#pragma once

#include <cstddef>
#include <glm/glm.hpp>

namespace tinygltf {
class Model;
struct Accessor;
}

bool glbVerbose();
bool shouldLogPrimitiveDetails(int meshIndex, int primitiveIndex);
bool validVec3(glm::vec3 v);
bool validVec2(glm::vec2 v);
const char* accessorTypeName(int type);
const char* componentTypeName(int type);
bool validateAccessor(
    const tinygltf::Model& model,
    int accessorIndex,
    const char* label,
    int meshIndex,
    int primitiveIndex,
    int requiredType,
    int requiredComponentType,
    const tinygltf::Accessor** out
);
const unsigned char* accessorPtr(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
size_t accessorStride(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
glm::vec3 readVec3(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, glm::vec3 fallback);
glm::vec2 readVec2(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t index, glm::vec2 fallback);
bool readIndex(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t i, unsigned int& out);
