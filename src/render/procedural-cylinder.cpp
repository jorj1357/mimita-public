// 2026-10-27

#include "render/procedural-cylinder.h"

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
        uvs.push_back(glm::vec2(u, 0.0f));
    }
    positions.push_back(bottomCenter);
    normals.push_back(glm::vec3(0.0f, 0.0f, -1.0f));
    uvs.push_back(glm::vec2(0.5f, 0.0f));

    const glm::vec3 topCenter(0.0f, 0.0f, halfLength);
    for (int i = 0; i <= seg; ++i)
    {
        const float u = static_cast<float>(i) / static_cast<float>(seg);
        const float angle = u * 2.0f * 3.14159265358979323846f;
        positions.push_back(glm::vec3(std::cos(angle) * radius,
                                      std::sin(angle) * radius,
                                      halfLength));
        normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
        uvs.push_back(glm::vec2(u, 1.0f));
    }
    positions.push_back(topCenter);
    normals.push_back(glm::vec3(0.0f, 0.0f, 1.0f));
    uvs.push_back(glm::vec2(0.5f, 1.0f));

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
        indices.push_back(bottomRingStart + i);
        indices.push_back(bottomRingStart + ((i + 1) % (seg + 1)));
    }

    const int topRingStart = bottomCenterIndex + 1;
    const int topCenterIndex = topRingStart + (seg + 1);
    for (int i = 0; i < seg; ++i)
    {
        indices.push_back(topCenterIndex);
        indices.push_back(topRingStart + ((i + 1) % (seg + 1)));
        indices.push_back(topRingStart + i);
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
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, pos));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, uv));
    glBindVertexArray(0);

    auto result = gCylinderCache.emplace(key, mesh);
    return result.first->second;
}

void drawTexturedCylinder(const Camera& camera, const glm::mat4& model,
                          const TexturedCylinderMesh& mesh,
                          unsigned int texture, const glm::vec3& tint,
                          float alpha)
{
    if (!gRenderer || !gRenderer->shaderProgram || mesh.vertexCount <= 0)
        return;

    const unsigned int shader = gRenderer->shaderProgram;
    const glm::mat4 view = camera.getView();
    const glm::mat4 projection =
        camera.getProj(static_cast<float>(gRenderer->sceneWidth),
                       static_cast<float>(gRenderer->sceneHeight));

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, GL_FALSE,
                       &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, GL_FALSE,
                       &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, GL_FALSE,
                       &model[0][0]);
    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glUniform3f(glGetUniformLocation(shader, "uTint"), tint.x, tint.y,
                tint.z);
    glUniform1f(glGetUniformLocation(shader, "uAlphaCutoff"),
                alpha >= 0.999f ? 0.5f : 0.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
    glBindVertexArray(0);
}
