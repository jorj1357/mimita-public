#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

class Camera;
struct WeaponDefinition;

// Generates and renders a simple textured cylinder/capsule mesh for projectiles.
// Uses the same Vertex format as GLB meshes: pos, normal, uv.

struct ProjectileRenderMesh {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    int vertexCount = 0;
    std::string loadedTexturePath;
};

struct ProjectileVisualConfig {
    // Main projectile
    std::string texturePath = "assets/textureshq/colorful2.png";
    float length = 1.5f;
    float radius = 0.18f;
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 rotationOffsetDegrees = glm::vec3(0.0f);
    glm::vec2 textureTiling = glm::vec2(1.0f);
    float fillAlpha = 1.0f;

    // Outline (rendered slightly larger behind main mesh)
    bool outlineEnabled = true;
    glm::vec3 outlineColor = glm::vec3(1.0f, 0.8f, 0.2f);
    float outlineAlpha = 0.4f;
    float outlineScale = 1.15f;

    // Glow sphere (large semi-transparent sphere around projectile)
    bool glowEnabled = true;
    glm::vec3 glowColor = glm::vec3(1.0f, 0.6f, 0.0f);
    float glowAlpha = 0.15f;
    float glowRadiusMultiplier = 3.0f;
};

ProjectileRenderMesh& getProjectileMesh(float length, float radius, int segments = 16);

void renderProjectile(
    const Camera& camera,
    const glm::vec3& position,
    const glm::quat& orientation,
    const ProjectileVisualConfig& cfg);

// Builds the same visual settings for live, network, and replay projectiles.
ProjectileVisualConfig projectileVisualConfigForWeapon(
    const std::string& weaponId);

// Free cached mesh resources
void clearProjectileMeshes();
