#include "render.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vector>
#include <cmath>

void SphereRenderer::init(int sectors, int rings) {
    std::vector<float> verts;
    std::vector<unsigned int> indices;

    for (int i = 0; i <= rings; ++i) {
        float theta = (float)i * 3.14159265f / (float)rings;
        float sinT = sin(theta), cosT = cos(theta);
        for (int j = 0; j <= sectors; ++j) {
            float phi = (float)j * 2.0f * 3.14159265f / (float)sectors;
            float sinP = sin(phi), cosP = cos(phi);
            verts.push_back(sinT * cosP);
            verts.push_back(sinT * sinP);
            verts.push_back(cosT);
            verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f);
        }
    }

    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < sectors; ++j) {
            int first = i * (sectors + 1) + j;
            int second = first + sectors + 1;
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    createMesh(mesh, verts.data(), (int)verts.size() / 7,
               indices.data(), (int)indices.size());
}

void SphereRenderer::draw(const glm::mat4& viewProj, glm::vec3 pos,
                           float radius, glm::vec4 color) {
    (void)color;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos)
                    * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
    if (!gShaderReady || mesh.indexCount == 0) return;
    glUseProgram(gShader);
    setShaderMVP(gShader, viewProj * model);
    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
}

void CylinderRenderer::init(int segments) {
    std::vector<float> verts;
    std::vector<unsigned int> indices;

    for (int i = 0; i <= segments; ++i) {
        float a = (float)i * 2.0f * 3.14159265f / (float)segments;
        float x = cos(a), y = sin(a);
        verts.push_back(x);  verts.push_back(y);  verts.push_back(-0.5f);
        verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f);
        verts.push_back(x);  verts.push_back(y);  verts.push_back(0.5f);
        verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f); verts.push_back(1.0f);
    }

    for (int i = 0; i < segments; ++i) {
        int a = i * 2, b = a + 1, c = (i + 1) * 2, d = c + 1;
        indices.push_back(a); indices.push_back(c); indices.push_back(b);
        indices.push_back(b); indices.push_back(c); indices.push_back(d);
    }

    createMesh(mesh, verts.data(), (int)verts.size() / 7,
               indices.data(), (int)indices.size());
}

void CylinderRenderer::draw(const glm::mat4& viewProj, glm::vec3 from,
                             glm::vec3 to, float radius, glm::vec4 color) {
    (void)color;
    glm::vec3 dir = to - from;
    float len = glm::length(dir);
    if (len < 0.0001f) return;
    dir /= len;

    glm::vec3 up(0.0f, 0.0f, 1.0f);
    glm::mat4 rot(1.0f);
    float dotUp = glm::dot(dir, up);
    if (dotUp < -0.999f) {
        rot = glm::rotate(rot, 3.14159265f, glm::vec3(1.0f, 0.0f, 0.0f));
    } else if (dotUp < 0.999f) {
        glm::vec3 axis = glm::normalize(glm::cross(up, dir));
        float angle = acos(glm::clamp(dotUp, -1.0f, 1.0f));
        rot = glm::rotate(rot, angle, axis);
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), (from + to) * 0.5f)
                    * rot
                    * glm::scale(glm::mat4(1.0f), glm::vec3(radius, radius, len));
    if (!gShaderReady || mesh.indexCount == 0) return;
    glUseProgram(gShader);
    setShaderMVP(gShader, viewProj * model);
    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
}

void LineRenderer::init() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void LineRenderer::addLine(glm::vec3 from, glm::vec3 to) {
    auto push = [&](const glm::vec3& p) {
        data.push_back(p.x); data.push_back(p.y); data.push_back(p.z);
        data.push_back(1.0f); data.push_back(1.0f); data.push_back(1.0f); data.push_back(1.0f);
    };
    push(from);
    push(to);
}

void LineRenderer::flush(const glm::mat4& viewProj) {
    if (data.empty()) return;
    glUseProgram(gShader);
    setShaderMVP(gShader, viewProj);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float),
                 data.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, (int)data.size() / 7);
    data.clear();
}

void LineRenderer::destroy() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
}
