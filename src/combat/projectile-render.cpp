#include "projectile-render.h"
#include "game/gl-resource-bridge-module.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "camera.h"
#include "combat/weapon-data.h"
#include "combat/weapon-registry.h"
#include "debug/debug-visuals.h"
#include "map/map_common.h"
#include "map/map_loader.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "render/render-command.h"
#include "render/world-shader.h"

namespace r = mimita::game::render;

extern Renderer* gRenderer;

static std::unordered_map<std::string, ProjectileRenderMesh> gMeshCache;

namespace {

struct ProjectileModelMesh {
    Mesh mesh;
    unsigned int vao = 0;
    unsigned int vbo = 0;
    bool attempted = false;
    bool ready = false;
};

std::unordered_map<std::string, ProjectileModelMesh> gModelCache;

ProjectileModelMesh& projectileModelFor(const std::string& path)
{
    ProjectileModelMesh& cached = gModelCache[path];
    if (cached.attempted)
        return cached;
    cached.attempted = true;
    cached.mesh = loadGLB(path, false);
    if (cached.mesh.verts.empty())
        return cached;
    cached.vao = mimita::game::genVertexArray();
    cached.vbo = mimita::game::genBuffer();
    r::bindVertexArray(cached.vao);
    r::bindBuffer(r::BufferTarget::Array, cached.vbo);
    r::bufferData(r::BufferTarget::Array, cached.vbo, cached.mesh.verts.data(),
                  static_cast<std::uint32_t>(cached.mesh.verts.size() * sizeof(Vertex)),
                  r::BufferUsage::Static);
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, pos)));
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(1, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, normal)));
    r::enableVertexAttribArray(2);
    r::vertexAttribPointer(2, 2, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, uv)));
    r::bindVertexArray(0);
    cached.ready = true;
    return cached;
}

glm::quat facingAxisCorrection(const std::string& axis)
{
    if (axis == "+X" || axis == "X") return glm::angleAxis(glm::radians(-90.0f), glm::vec3(0, 1, 0));
    if (axis == "-X") return glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    if (axis == "+Y" || axis == "Y") return glm::angleAxis(glm::radians(90.0f), glm::vec3(1, 0, 0));
    if (axis == "-Y") return glm::angleAxis(glm::radians(-90.0f), glm::vec3(1, 0, 0));
    if (axis == "-Z") return glm::angleAxis(glm::radians(180.0f), glm::vec3(1, 0, 0));
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

bool drawProjectileModel(const Camera& camera, const glm::vec3& position,
                         const glm::quat& orientation,
                         const ProjectileVisualConfig& cfg)
{
    if (cfg.modelPath.empty() || !gRenderer || !r::worldShader().ensureLoaded())
        return false;
    ProjectileModelMesh& model = projectileModelFor(cfg.modelPath);
    if (!model.ready)
        return false;

    glm::mat4 offsetRot(1.0f);
    if (glm::length(cfg.rotationOffsetDegrees) > 0.001f) {
        offsetRot = glm::rotate(offsetRot, glm::radians(cfg.rotationOffsetDegrees.x), glm::vec3(1, 0, 0));
        offsetRot = glm::rotate(offsetRot, glm::radians(cfg.rotationOffsetDegrees.y), glm::vec3(0, 1, 0));
        offsetRot = glm::rotate(offsetRot, glm::radians(cfg.rotationOffsetDegrees.z), glm::vec3(0, 0, 1));
    }
    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), position)
        * glm::mat4_cast(orientation)
        * glm::mat4_cast(facingAxisCorrection(cfg.facingAxis))
        * offsetRot
        * glm::translate(glm::mat4(1.0f), cfg.modelOffset)
        * glm::scale(glm::mat4(1.0f), cfg.scale);

    auto& shader = r::worldShader();
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->sceneWidth, (float)gRenderer->sceneHeight);
    r::useProgram(shader.program());
    r::uniformMatrix4fv(shader.uniform("view"), 1, &view[0][0]);
    r::uniformMatrix4fv(shader.uniform("projection"), 1, &proj[0][0]);
    r::uniformMatrix4fv(shader.uniform("model"), 1, &modelMatrix[0][0]);
    r::uniform1i(shader.uniform("uUseColor"), 0);
    r::uniform1i(shader.uniform("uTex"), 0);
    r::uniform3f(shader.uniform("uTint"), 1.0f, 1.0f, 1.0f);
    r::uniform1f(shader.uniform("uAlphaCutoff"), 0.5f);

    r::enable(r::Capability::DepthTest);
    r::depthMask(true);
    r::activeTexture(0);
    r::bindVertexArray(model.vao);
    for (const Mesh::Batch& batch : model.mesh.batches) {
        std::uint32_t tex = batch.texture ? batch.texture : gTextures.get("default");
        r::bindTexture(r::TextureTarget::Texture2D, tex);
        r::drawArrays(r::Primitive::Triangles,
                      static_cast<std::int32_t>(batch.first),
                      static_cast<std::int32_t>(batch.count));
    }
    r::bindVertexArray(0);
    return true;
}

float projectileParam(const WeaponDefinition* def, const char* key, float fallback)
{
    if (!def)
        return fallback;
    const auto it = def->customParams.find(key);
    return it == def->customParams.end() ? fallback : it->second;
}

}

ProjectileVisualConfig projectileVisualConfigForWeapon(const std::string& weaponId)
{
    const bool rocket = weaponId == "rocket_launcher";
    const WeaponDefinition* def = WeaponRegistry::instance().get(weaponId);
    ProjectileVisualConfig cfg;
    cfg.modelPath = def ? def->projectileModelPath : "";
    cfg.facingAxis = def ? def->projectileFacingAxis : "+Z";
    cfg.modelOffset = def ? def->projectileModelOffset : glm::vec3(0.0f);
    cfg.sphere = projectileParam(def, "projectileVisualSphere", 0.0f) > 0.0f;
    cfg.fillColor = {
        projectileParam(def, "projectileVisualFillR", 1.0f),
        projectileParam(def, "projectileVisualFillG", 1.0f),
        projectileParam(def, "projectileVisualFillB", 1.0f)};
    cfg.texturePath = rocket ? "assets/textureshq/colorful2.png"
        : (def && def->grenadeEffect.enabled
            ? "assets/textures/default.png" : "assets/textureshq/meat1.png");
    cfg.length = projectileParam(def, "projectileVisualLength", rocket ? 1.5f : 1.8f);
    cfg.radius = def && def->id == "deathball" && def->projectileRadius > 0.0f
        ? def->projectileRadius
        : projectileParam(def, "projectileVisualRadius", rocket ? 0.18f : 0.28f);
    cfg.scale = {
        projectileParam(def, "projectileVisualScaleX", 1.0f),
        projectileParam(def, "projectileVisualScaleY", 1.0f),
        projectileParam(def, "projectileVisualScaleZ", 1.0f)};
    cfg.rotationOffsetDegrees = {
        projectileParam(def, "projectileVisualRotationOffsetX", 0.0f),
        projectileParam(def, "projectileVisualRotationOffsetY", 0.0f),
        projectileParam(def, "projectileVisualRotationOffsetZ", 0.0f)};
    if (def && !def->projectileModelPath.empty()) {
        cfg.scale = def->projectileModelScale;
        cfg.rotationOffsetDegrees = def->projectileModelRotation;
    }
    cfg.textureTiling = {
        projectileParam(def, "projectileVisualTextureTilingU", 1.0f),
        projectileParam(def, "projectileVisualTextureTilingV", 1.0f)};
    cfg.fillAlpha = projectileParam(def, "projectileFillAlpha", 1.0f);
    cfg.outlineEnabled = projectileParam(def, "projectileOutlineEnabled", 1.0f) > 0.0f;
    cfg.outlineColor = {
        projectileParam(def, "projectileOutlineColorR", 1.0f),
        projectileParam(def, "projectileOutlineColorG", 0.8f),
        projectileParam(def, "projectileOutlineColorB", 0.2f)};
    cfg.outlineAlpha = projectileParam(def, "projectileOutlineAlpha", 0.4f);
    cfg.outlineScale = projectileParam(def, "projectileOutlineScale", 1.15f);
    cfg.glowEnabled = projectileParam(def, "projectileGlowEnabled", 1.0f) > 0.0f;
    cfg.glowColor = {
        projectileParam(def, "projectileGlowColorR", 1.0f),
        projectileParam(def, "projectileGlowColorG", 0.6f),
        projectileParam(def, "projectileGlowColorB", 0.0f)};
    cfg.glowAlpha = projectileParam(def, "projectileGlowAlpha", 0.15f);
    cfg.glowRadiusMultiplier = projectileParam(def, "projectileGlowRadiusMultiplier", 3.0f);
    return cfg;
}

// Generates a cylinder mesh oriented along local +Z (forward).
static void generateCylinderMesh(
    std::vector<glm::vec3>& positions,
    std::vector<glm::vec3>& normals,
    std::vector<glm::vec2>& uvs,
    float length,
    float radius,
    int segments)
{
    positions.clear();
    normals.clear();
    uvs.clear();

    int seg = std::max(segments, 4);
    float halfLen = length * 0.5f;

    // Body: two rings of vertices around the cylinder
    for (int i = 0; i <= seg; ++i) {
        float u = (float)i / (float)seg;
        float angle = u * 2.0f * 3.14159265f;
        float cx = std::cos(angle) * radius;
        float cy = std::sin(angle) * radius;
        glm::vec3 n(cx / (radius + 0.0001f), cy / (radius + 0.0001f), 0.0f);

        // Bottom ring
        positions.push_back(glm::vec3(cx, cy, -halfLen));
        normals.push_back(n);
        uvs.push_back(glm::vec2(u, 0.0f));

        // Top ring
        positions.push_back(glm::vec3(cx, cy, halfLen));
        normals.push_back(n);
        uvs.push_back(glm::vec2(u, 1.0f));
    }

    // Bottom cap (fan toward center)
    glm::vec3 bottomCenter(0.0f, 0.0f, -halfLen);
    for (int i = 0; i <= seg; ++i) {
        float u = (float)i / (float)seg;
        float angle = u * 2.0f * 3.14159265f;
        float cx = std::cos(angle) * radius;
        float cy = std::sin(angle) * radius;
        positions.push_back(glm::vec3(cx, cy, -halfLen));
        normals.push_back(glm::vec3(0.0f, 0.0f, -1.0f));
        uvs.push_back(glm::vec2(u, 0.0f));
    }
    positions.push_back(bottomCenter);
    normals.push_back(glm::vec3(0.0f, 0.0f, -1.0f));
    uvs.push_back(glm::vec2(0.5f, 0.0f));

    // Top cap (fan toward center)
    glm::vec3 topCenter(0.0f, 0.0f, halfLen);
    for (int i = 0; i <= seg; ++i) {
        float u = (float)i / (float)seg;
        float angle = u * 2.0f * 3.14159265f;
        float cx = std::cos(angle) * radius;
        float cy = std::sin(angle) * radius;
        positions.push_back(glm::vec3(cx, cy, halfLen));
        normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
        uvs.push_back(glm::vec2(u, 1.0f));
    }
    positions.push_back(topCenter);
    normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
    uvs.push_back(glm::vec2(0.5f, 1.0f));
}

// Build indices for the cylinder mesh as indexed triangles
static void buildCylinderIndices(
    std::vector<unsigned int>& indices,
    int segments)
{
    indices.clear();
    int seg = std::max(segments, 4);

    // Body (quad strip): 2 triangles per segment
    for (int i = 0; i < seg; ++i) {
        int bl = i * 2;
        int tl = bl + 1;
        int br = (i + 1) * 2;
        int tr = br + 1;
        indices.push_back(bl); indices.push_back(br); indices.push_back(tl);
        indices.push_back(tl); indices.push_back(br); indices.push_back(tr);
    }

    // Bottom cap (fan)
    int bottomRingStart = (seg + 1) * 2;
    int bottomCenterIdx = bottomRingStart + (seg + 1);
    for (int i = 0; i < seg; ++i) {
        indices.push_back(bottomCenterIdx);
        indices.push_back(bottomRingStart + i);
        indices.push_back(bottomRingStart + ((i + 1) % (seg + 1)));
    }

    // Top cap (fan)
    int topRingStart = bottomCenterIdx + 1;
    int topCenterIdx = topRingStart + (seg + 1);
    for (int i = 0; i < seg; ++i) {
        indices.push_back(topCenterIdx);
        indices.push_back(topRingStart + ((i + 1) % (seg + 1)));
        indices.push_back(topRingStart + i);
    }
}

// Build a non-indexed triangle list from the cylinder data
static void flattenCylinder(
    std::vector<Vertex>& vertsOut,
    float length,
    float radius,
    int segments)
{
    std::vector<glm::vec3> positions, normals;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned int> indices;

    generateCylinderMesh(positions, normals, uvs, length, radius, segments);
    buildCylinderIndices(indices, segments);

    vertsOut.clear();
    vertsOut.reserve(indices.size());
    for (unsigned int idx : indices) {
        Vertex v;
        v.pos = positions[idx];
        v.normal = normals[idx];
        v.uv = uvs[idx];
        vertsOut.push_back(v);
    }
}

ProjectileRenderMesh& getProjectileMesh(float length, float radius, int segments)
{
    char key[64];
    std::snprintf(key, sizeof(key), "cyl_%.3f_%.3f_%d", length, radius, segments);

    auto it = gMeshCache.find(key);
    if (it != gMeshCache.end())
        return it->second;

    ProjectileRenderMesh mesh;
    std::vector<Vertex> verts;
    flattenCylinder(verts, length, radius, segments);
    mesh.vertexCount = (int)verts.size();

    mesh.vao = mimita::game::genVertexArray();
    mesh.vbo = mimita::game::genBuffer();
    r::bindVertexArray(mesh.vao);
    r::bindBuffer(r::BufferTarget::Array, mesh.vbo);
    r::bufferData(r::BufferTarget::Array, mesh.vbo, verts.data(),
                  static_cast<std::uint32_t>(verts.size() * sizeof(Vertex)),
                  r::BufferUsage::Static);
    r::enableVertexAttribArray(0);
    r::vertexAttribPointer(0, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, pos)));
    r::enableVertexAttribArray(1);
    r::vertexAttribPointer(1, 3, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, normal)));
    r::enableVertexAttribArray(2);
    r::vertexAttribPointer(2, 2, r::VertexType::Float, false, sizeof(Vertex),
                           static_cast<std::uint32_t>(offsetof(Vertex, uv)));
    r::bindVertexArray(0);

    auto result = gMeshCache.emplace(key, mesh);
    return result.first->second;
}

// Internal: set common shader uniforms and bind mesh for drawing
static void bindShaderAndMesh(
    const Camera& camera,
    const glm::mat4& modelMatrix,
    const ProjectileRenderMesh& mesh,
    std::uint32_t texture,
    const glm::vec3& tint,
    float alpha)
{
    if (!gRenderer || !r::worldShader().ensureLoaded() || mesh.vertexCount <= 0)
        return;

    auto& shader = r::worldShader();
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->sceneWidth, (float)gRenderer->sceneHeight);

    r::useProgram(shader.program());
    r::uniformMatrix4fv(shader.uniform("view"), 1, &view[0][0]);
    r::uniformMatrix4fv(shader.uniform("projection"), 1, &proj[0][0]);
    r::uniformMatrix4fv(shader.uniform("model"), 1, &modelMatrix[0][0]);
    r::uniform1i(shader.uniform("uUseColor"), 0);
    r::uniform1i(shader.uniform("uTex"), 0);
    r::uniform3f(shader.uniform("uTint"), tint.x, tint.y, tint.z);
    r::uniform1f(shader.uniform("uAlphaCutoff"), alpha >= 0.999f ? 0.5f : 0.0f);

    r::activeTexture(0);
    r::bindTexture(r::TextureTarget::Texture2D, texture);
    r::bindVertexArray(mesh.vao);
    r::drawArrays(r::Primitive::Triangles, 0, mesh.vertexCount);
    r::bindVertexArray(0);
}

static glm::mat4 buildModelMatrix(
    const glm::vec3& position,
    const glm::quat& orientation,
    const glm::vec3& rotationOffsetDegrees,
    const glm::vec3& scale)
{
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    glm::mat4 rotMat = glm::mat4_cast(orientation);
    glm::mat4 offsetRot(1.0f);
    if (glm::length(rotationOffsetDegrees) > 0.001f) {
        offsetRot = glm::rotate(offsetRot, glm::radians(rotationOffsetDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        offsetRot = glm::rotate(offsetRot, glm::radians(rotationOffsetDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        offsetRot = glm::rotate(offsetRot, glm::radians(rotationOffsetDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    }
    model = model * rotMat * offsetRot;
    model = glm::scale(model, scale);
    return model;
}

void renderProjectile(
    const Camera& camera,
    const glm::vec3& position,
    const glm::quat& orientation,
    const ProjectileVisualConfig& cfg)
{
    if (!gRenderer || !r::worldShader().ensureLoaded())
        return;

    if (cfg.sphere) {
        const float scale = std::max({cfg.scale.x, cfg.scale.y, cfg.scale.z});
        DebugVis::drawFilledSphere(camera, position, cfg.radius * scale,
            glm::vec4(cfg.fillColor, cfg.fillAlpha));
        return;
    }

    if (!cfg.modelPath.empty() && drawProjectileModel(camera, position, orientation, cfg)) {
        if (cfg.glowEnabled && cfg.glowAlpha > 0.001f) {
            float glowRadius = cfg.radius * cfg.glowRadiusMultiplier * std::max({cfg.scale.x, cfg.scale.y, cfg.scale.z});
            DebugVis::drawFilledSphere(camera, position, glowRadius,
                glm::vec4(cfg.glowColor, cfg.glowAlpha));
        }
        return;
    }


    // Load texture
    std::uint32_t tex = gTextures.getPath(cfg.texturePath);
    if (tex == 0) {
        static std::unordered_set<std::string> sTexWarned;
        if (sTexWarned.insert(cfg.texturePath).second)
            printf("[PROJECTILE] WARNING: Failed to load texture: %s\n", cfg.texturePath.c_str());
    }

    ProjectileRenderMesh& mesh = getProjectileMesh(cfg.length, cfg.radius, 16);
    if (mesh.vertexCount <= 0)
        return;

    // The rocket body is an opaque, closed cylinder. Only the outline and glow
    // are allowed to use blending; the fill must write depth so the far cap
    // cannot show through the projectile.
    r::pushState();
    r::disable(r::Capability::Blend);
    r::depthMask(true);

    // 1. Outline pass: slightly larger mesh with outline color
    if (cfg.outlineEnabled && cfg.outlineAlpha > 0.001f) {
        glm::vec3 outlineScale = cfg.scale * cfg.outlineScale;
        glm::mat4 outlineModel = buildModelMatrix(position, orientation, cfg.rotationOffsetDegrees, outlineScale);
        bindShaderAndMesh(camera, outlineModel, mesh, tex, cfg.outlineColor, cfg.outlineAlpha);
    }

    // 2. Main fill pass: textured mesh with fill alpha
    {
        glm::mat4 fillModel = buildModelMatrix(position, orientation, cfg.rotationOffsetDegrees, cfg.scale);
        bindShaderAndMesh(camera, fillModel, mesh, tex, cfg.fillColor, 1.0f);
    }

    r::popState();

    // 3. Glow sphere: large semi-transparent sphere
    if (cfg.glowEnabled && cfg.glowAlpha > 0.001f) {
        float glowRadius = cfg.radius * cfg.glowRadiusMultiplier * std::max({cfg.scale.x, cfg.scale.y, cfg.scale.z});
        glm::vec4 glowColor(cfg.glowColor, cfg.glowAlpha);
        // Check if the texture handle for glow is valid; use a fallback color-only approach
        // If we have a valid texture, use it; otherwise the color-only approach still works
        // For simplicity, render a DebugVis filled sphere (goes through overlay, survives render pass)
        DebugVis::drawFilledSphere(camera, position, glowRadius, glowColor);
    }
}

void clearProjectileMeshes()
{
    for (auto& kv : gMeshCache) {
        ProjectileRenderMesh& m = kv.second;
        if (m.vbo) mimita::game::deleteBuffer(m.vbo);
        if (m.vao) mimita::game::deleteVertexArray(m.vao);
    }
    gMeshCache.clear();
}
