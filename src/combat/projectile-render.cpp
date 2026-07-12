#include "projectile-render.h"

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
#include "map/map_common.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"

extern Renderer* gRenderer;

static std::unordered_map<std::string, ProjectileRenderMesh> gMeshCache;

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
    // V goes from 0 (bottom) to 1 (top), U goes around circumference
    for (int i = 0; i <= seg; ++i) {
        float u = (float)i / (float)seg;
        float angle = u * 2.0f * 3.14159265f;
        float cx = std::cos(angle) * radius;
        float cy = std::sin(angle) * radius;
        glm::vec3 n(cx / radius, cy / radius, 0.0f);

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

    // Bottom cap (fan): bottom ring starts at (seg+1)*2
    int bottomRingStart = (seg + 1) * 2;
    int bottomCenterIdx = bottomRingStart + (seg + 1);
    for (int i = 0; i < seg; ++i) {
        indices.push_back(bottomCenterIdx);
        indices.push_back(bottomRingStart + i);
        indices.push_back(bottomRingStart + ((i + 1) % (seg + 1)));
    }

    // Top cap: top ring starts after bottom cap
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

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
    glBindVertexArray(0);

    auto result = gMeshCache.emplace(key, mesh);
    return result.first->second;
}

void renderProjectile(
    const Camera& camera,
    const glm::vec3& position,
    const glm::quat& orientation,
    float length,
    float radius,
    const glm::vec3& scale,
    const glm::vec3& rotationOffsetDegrees,
    const glm::vec2& textureTiling,
    const std::string& texturePath)
{
    if (!gRenderer || !gRenderer->shaderProgram)
        return;

    ProjectileRenderMesh& mesh = getProjectileMesh(length, radius, 16);
    if (mesh.vertexCount <= 0)
        return;

    // Cache texture using absolute path.
    // NOTE: gTextures.get() mangles paths by stripping "assets/textures/" prefix
    // and re-adding it, which breaks "assets/textureshq/" paths. Use getPath instead.
    GLuint tex = gTextures.getPath(texturePath);
    if (tex == 0) {
        static std::unordered_set<std::string> sTexWarned;
        if (sTexWarned.insert(texturePath).second)
            printf("[PROJECTILE] WARNING: Failed to load texture: %s — projectile will be invisible\n", texturePath.c_str());
    }
    mesh.loadedTexturePath = texturePath;

    // Build model matrix: position, rotation (orientation + offset), scale
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);

    // Apply orientation quaternion
    glm::mat4 rotMat = glm::mat4_cast(orientation);

    // Apply rotation offset (local-space)
    glm::mat4 offsetRot(1.0f);
    if (glm::length(rotationOffsetDegrees) > 0.001f) {
        offsetRot = glm::rotate(offsetRot, glm::radians(rotationOffsetDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        offsetRot = glm::rotate(offsetRot, glm::radians(rotationOffsetDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        offsetRot = glm::rotate(offsetRot, glm::radians(rotationOffsetDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    }

    model = model * rotMat * offsetRot;
    model = glm::scale(model, scale);

    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);

    unsigned int shader = gRenderer->shaderProgram;
    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE, &model[0][0]);
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glUniform3f(glGetUniformLocation(shader, "uTint"), 1.0f, 1.0f, 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
    glBindVertexArray(0);
}

void clearProjectileMeshes()
{
    for (auto& kv : gMeshCache) {
        ProjectileRenderMesh& m = kv.second;
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
    }
    gMeshCache.clear();
}
