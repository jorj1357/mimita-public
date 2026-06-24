#include "map_common.h"
#include "map_loader.h"
#include "map-loader-mesh.h"
#include "map-loader-gltf-accessors.h"

#include <glm/glm.hpp>
#include <vector>

extern void appendPrimitive(
    const tinygltf::Model& model,
    int meshIndex,
    int primitiveIndex,
    const tinygltf::Primitive& primitive,
    const glm::mat4& transform,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh
);

void appendGLBMeshPrimitive(
    const tinygltf::Model& model,
    int meshIndex,
    int primitiveIndex,
    const tinygltf::Primitive& primitive,
    const glm::mat4& transform,
    const std::vector<GLuint>& materialTextures,
    Mesh& mesh
) {
    appendPrimitive(model, meshIndex, primitiveIndex, primitive, transform, materialTextures, mesh);
}
