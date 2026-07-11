#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

class Camera;

// Generates and renders a simple textured cylinder/capsule mesh for projectiles.
// Uses the same Vertex format as GLB meshes: pos, normal, uv.
// Supports hot-reloadable texture and dimensions via config.

struct ProjectileRenderMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int vertexCount = 0;
    std::string loadedTexturePath;
};

ProjectileRenderMesh& getProjectileMesh(float length, float radius, int segments = 16);

void renderProjectile(
    const Camera& camera,
    const glm::vec3& position,
    const glm::quat& orientation,
    float length,
    float radius,
    const glm::vec3& scale,
    const glm::vec3& rotationOffsetDegrees,
    const glm::vec2& textureTiling,
    const std::string& texturePath);

// Free cached mesh resources, call on shutdown or explicit cleanup
void clearProjectileMeshes();
