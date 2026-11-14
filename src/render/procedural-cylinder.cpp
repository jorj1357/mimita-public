// 2026-10-27

#include "render/procedural-cylinder.h"
#include "game/gl-resource-bridge-module.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "map/map_common.h"
#include "renderer/renderer.h"
#include "world/texture-store.h"
#include "render/render-command.h"
#include "render/world-shader.h"

namespace r = mimita::game::render;

extern Renderer* gRenderer;

namespace {

std::unordered_map<std::string, TexturedCylinderMesh> gCylinderCache;

void buildCylinderVertices(std::vector<Vertex>& verts, float radius,
                           float length, int segments)
{
    const int seg = std::max(segments, 4);
    const float halfLength = length * 0.5f;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;

    for (int i = 0; i <= seg; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(seg);
        const float angle = u * 2.0f * 3.14159265358979323846f;
        const float cx = std::cos(angle) * radius;
        const float cy = std::sin(angle) * radius;
        const glm::vec3 normal =
            glm::normalize(glm::vec3(cx, cy, 0.0f));
        positions.push_back(glm::vec3(cx, cy, -halfLength));
        normals.push_back(normal);
        uvs.push_back(glm::vec2(u, 0.0f));
        positions.push_back(glm::vec3(cx, cy, halfLength));
        normals.push_back(normal);
        uvs.push_back(glm::vec2(u, 1.0f));
    }

    const glm::vec3 bottomCenter(0.0f, 0.0f, -halfLength);
    for (int i = 0; i <= seg; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(seg);
        const float angle = u * 2.0f * 3.14159265358979323846f;
        positions.push_back(glm::vec3(std::cos(angle) * radius,
                                      std::sin(angle) * radius,
                                      -halfLength));
        normals.push_back(glm::vec3(0.0f, 0.0f, -1.0f));
        uvs.push_back(glm::vec2(0.5f + 0.5f * std::cos(angle),
                                0.5f + 0.5f * std::sin(angle)));
    }
    positions.push_back(bottomCenter);
    normals.push_back(glm::vec3(0.0f, 0.0f, -1.0f));
    uvs.push_back(glm::vec2(0.5f, 0.5f));

    const glm::vec3 topCenter(0.0f, 0.0f, halfLength);
    for (int i = 0; i <= seg; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(seg);
        const float angle = u * 2.0f * 3.14159265358979323846f;
        positions.push_back(glm::vec3(std::cos(angle) * radius,
                                      std::sin(angle) * radius,
                                      halfLength));
        normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
        uvs.push_back(glm::vec2(0.5f + 0.5f * std::cos(angle),
                                0.5f + 0.5f * std::sin(angle)));
    }
    positions.push_back(topCenter);
    normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
    uvs.push_back(glm::vec2(0.5f, 0.5f));

    std::vector<unsigned int> indices;
    for (int i = 0; i < seg; ++i)
    {
        const unsigned int bl = i * 2;
        const unsigned int tl = bl + 1;
        const unsigned int br = (i + 1) * 2;
        const unsigned int tr = br + 1;
        indices.push_back(bl);
        indices.push_back(br);
        indices.push_back(tl);
        indices.push_back(tl);
        indices.push_back(br);
        indices.push_back(tr);
    }

    const int bottomRingStart = (seg + 1) * 2;
    const int bottomCenterIndex = bottomRingStart + (seg + 1);
    for (int i = 0; i < seg; ++i)
    {
        indices.push_back(bottomCenterIndex);
        indices.push_back(bottomRingStart + ((i + 1) % (seg + 1)));
        indices.push_back(bottomRingStart + i);
    }

    const int topRingStart = bottomCenterIndex + 1;
    const int topCenterIndex = topRingStart + (seg + 1);
    for (int i = 0; i < seg; ++i)
    {
        indices.push_back(topCenterIndex);
        indices.push_back(topRingStart + i);
        indices.push_back(topRingStart + ((i + 1) % (seg + 1)));
    }

    verts.clear();
    verts.reserve(indices.size());
    for (unsigned int index : indices)
    {
        Vertex v;
        v.pos = positions[index];
        v.normal = normals[index];
        v.uv = uvs[index];
        verts.push_back(v);
    }
}

} // namespace

const TexturedCylinderMesh& getTexturedCylinderMesh(float radius,
                                                    float length,
                                                    int segments)
{
    char key[80];
    std::snprintf(key, sizeof(key), "cyl_%.3f_%.3f_%d", radius, length,
                  segments);
    auto it = gCylinderCache.find(key);
    if (it != gCylinderCache.end())
        return it->second;

    std::vector<Vertex> verts;
    buildCylinderVertices(verts, radius, length, segments);

    TexturedCylinderMesh mesh;
    mesh.vertexCount = static_cast<int>(verts.size());
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

    auto result = gCylinderCache.emplace(key, mesh);
    return result.first->second;
}

void drawTexturedCylinder(const Camera& camera, const glm::mat4& model,
                          const TexturedCylinderMesh& mesh,
                          unsigned int texture, const glm::vec3& tint,
                          float alpha)
{
    if (!gRenderer || !r::worldShader().ensureLoaded() || mesh.vertexCount <= 0)
        return;

    auto& shader = r::worldShader();
    const glm::mat4 view = camera.getView();
    const glm::mat4 projection =
        camera.getProj(static_cast<float>(gRenderer->sceneWidth),
                       static_cast<float>(gRenderer->sceneHeight));

    r::useProgram(shader.program());
    r::uniformMatrix4fv(shader.uniform("view"), 1, &view[0][0]);
    r::uniformMatrix4fv(shader.uniform("projection"), 1, &projection[0][0]);
    r::uniformMatrix4fv(shader.uniform("model"), 1, &model[0][0]);
    r::uniform1i(shader.uniform("uUseColor"), 0);
    r::uniform1i(shader.uniform("uTex"), 0);
    r::uniform3f(shader.uniform("uTint"), tint.x, tint.y, tint.z);
    // Wheels are opaque; the texture has no intended cutout.
    r::uniform1f(shader.uniform("uAlphaCutoff"), 0.0f);

    r::pushState();
    r::disable(r::Capability::CullFace);

    r::activeTexture(0);
    r::bindTexture(r::TextureTarget::Texture2D, texture);
    r::bindVertexArray(mesh.vao);
    r::drawArrays(r::Primitive::Triangles, 0, mesh.vertexCount);
    r::bindVertexArray(0);
    r::popState();
}
