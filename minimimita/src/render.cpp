#include "render.h"
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

GLuint gShader = 0;
bool gShaderReady = false;

static SphereRenderer gSphereRenderer;
static CylinderRenderer gCylinderRenderer;
static LineRenderer gLineRenderer;
static Mesh gWorldMesh;
static bool gWorldMeshValid = false;

static const char* vertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* fragSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader compile error (%s): %s\n",
                type == GL_VERTEX_SHADER ? "vert" : "frag", log);
    }
    return s;
}

bool initRenderer() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) return false;

    gShader = glCreateProgram();
    glAttachShader(gShader, vs);
    glAttachShader(gShader, fs);
    glLinkProgram(gShader);
    GLint ok;
    glGetProgramiv(gShader, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(gShader, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader link error: %s\n", log);
        return false;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    gShaderReady = true;

    gSphereRenderer.init();
    gCylinderRenderer.init();
    gLineRenderer.init();

    return true;
}

void setShaderMVP(GLuint shader, const glm::mat4& mvp) {
    glUniformMatrix4fv(glGetUniformLocation(shader, "uMVP"), 1, GL_FALSE,
                       glm::value_ptr(mvp));
}

void setShaderColor(GLuint shader, const glm::vec4& color) {
    glUniform4fv(glGetUniformLocation(shader, "uColor"), 1, &color[0]);
}

static void createMesh(Mesh& mesh, const float* verts, int vertCount,
                       const unsigned int* indices, int indexCount) {
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertCount * 3 * sizeof(float), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int),
                 indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    mesh.indexCount = indexCount;
}

static void destroyMesh(Mesh& mesh) {
    if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
    if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
    if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    mesh = Mesh();
}

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

    createMesh(mesh, verts.data(), (int)verts.size() / 3,
               indices.data(), (int)indices.size());
}

void SphereRenderer::draw(const glm::mat4& viewProj, glm::vec3 pos,
                           float radius, glm::vec4 color) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), pos)
                    * glm::scale(glm::mat4(1.0f), glm::vec3(radius));
    drawMeshInstanced(mesh, viewProj, model, color);
}

void CylinderRenderer::init(int segments) {
    std::vector<float> verts;
    std::vector<unsigned int> indices;

    for (int i = 0; i <= segments; ++i) {
        float a = (float)i * 2.0f * 3.14159265f / (float)segments;
        float x = cos(a), y = sin(a);
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(-0.5f);
        verts.push_back(x);
        verts.push_back(y);
        verts.push_back(0.5f);
    }

    for (int i = 0; i < segments; ++i) {
        int a = i * 2, b = a + 1, c = (i + 1) * 2, d = c + 1;
        indices.push_back(a); indices.push_back(c); indices.push_back(b);
        indices.push_back(b); indices.push_back(c); indices.push_back(d);
    }

    createMesh(mesh, verts.data(), (int)verts.size() / 3,
               indices.data(), (int)indices.size());
}

void CylinderRenderer::draw(const glm::mat4& viewProj, glm::vec3 from,
                             glm::vec3 to, float radius, glm::vec4 color) {
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
    drawMeshInstanced(mesh, viewProj, model, color);
}

void LineRenderer::init() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void LineRenderer::addLine(glm::vec3 from, glm::vec3 to) {
    data.push_back(from.x); data.push_back(from.y); data.push_back(from.z);
    data.push_back(to.x);   data.push_back(to.y);   data.push_back(to.z);
}

void LineRenderer::flush(const glm::mat4& viewProj) {
    if (data.empty()) return;
    glUseProgram(gShader);
    setShaderMVP(gShader, viewProj);
    setShaderColor(gShader, glm::vec4(1.0f));

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float),
                 data.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, (int)data.size() / 3);

    data.clear();
}

void LineRenderer::destroy() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
}

void drawMeshInstanced(const Mesh& mesh, const glm::mat4& viewProj,
                       const glm::mat4& model, const glm::vec4& color) {
    if (!gShaderReady || mesh.indexCount == 0) return;
    glUseProgram(gShader);
    setShaderMVP(gShader, viewProj * model);
    setShaderColor(gShader, color);

    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, 0);
}

void rebuildWorldMesh(const std::vector<Triangle>& triangles) {
    if (gWorldMeshValid) {
        destroyMesh(gWorldMesh);
        gWorldMeshValid = false;
    }
    if (triangles.empty()) return;

    std::vector<float> verts(triangles.size() * 9);
    std::vector<unsigned int> indices(triangles.size() * 3);
    for (size_t i = 0; i < triangles.size(); ++i) {
        verts[i*9+0] = triangles[i].a.x;
        verts[i*9+1] = triangles[i].a.y;
        verts[i*9+2] = triangles[i].a.z;
        verts[i*9+3] = triangles[i].b.x;
        verts[i*9+4] = triangles[i].b.y;
        verts[i*9+5] = triangles[i].b.z;
        verts[i*9+6] = triangles[i].c.x;
        verts[i*9+7] = triangles[i].c.y;
        verts[i*9+8] = triangles[i].c.z;
        indices[i*3+0] = (unsigned int)(i*3);
        indices[i*3+1] = (unsigned int)(i*3+1);
        indices[i*3+2] = (unsigned int)(i*3+2);
    }

    createMesh(gWorldMesh, verts.data(), (int)verts.size()/3,
               indices.data(), (int)indices.size());
    gWorldMeshValid = true;
}

void renderWorld(const glm::mat4& viewProj, bool wireframe) {
    if (!gShaderReady || !gWorldMeshValid) return;

    if (wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glUseProgram(gShader);
    setShaderMVP(gShader, viewProj);
    setShaderColor(gShader, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
    glBindVertexArray(gWorldMesh.VAO);
    glDrawElements(GL_TRIANGLES, gWorldMesh.indexCount, GL_UNSIGNED_INT, 0);

    if (wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void renderPlayer(const Player& player, const glm::mat4& viewProj) {
    glm::vec3 a = player.capA();
    glm::vec3 b = player.capB();
    glm::vec4 color = player.grounded
        ? glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)
        : glm::vec4(1.0f, 0.3f, 0.0f, 1.0f);

    gSphereRenderer.draw(viewProj, a, player.radius, color);
    gSphereRenderer.draw(viewProj, b, player.radius, color);
    gCylinderRenderer.draw(viewProj, a, b, player.radius, color);
}

void renderContacts(const Player& player, const glm::mat4& viewProj) {
    for (const auto& c : player.contacts.contacts) {
        glm::vec4 color;
        switch (c.side) {
            case Contact::FLOOR:   color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); break;
            case Contact::WALL:    color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); break;
            case Contact::CEILING: color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); break;
        }
        gSphereRenderer.draw(viewProj, c.point, 0.08f, color);
        gLineRenderer.addLine(c.point, c.point + c.normal * 0.5f);
    }
}

void renderHUD(const Player& player, const TestMap& map, int w, int h,
               bool wireframe) {
    (void)w;
    (void)h;
    (void)wireframe;

    // Console-based HUD only
    static double lastPrint = 0.0;
    double now = glfwGetTime();
    if (now - lastPrint > 0.25) {
        lastPrint = now;
        printf("[HUD] Map: %s (%zu tri)  Pos: %.2f %.2f %.2f  Vel: %.2f %.2f %.2f  "
               "Grounded: %s  Contacts: %zu (F:%d W:%d C:%d)  Wire:%s\n",
               map.name.c_str(), map.triangles.size(),
               player.position.x, player.position.y, player.position.z,
               player.velocity.x, player.velocity.y, player.velocity.z,
               player.grounded ? "Y" : "N",
               player.contacts.contacts.size(),
               player.contacts.touchingFloor,
               player.contacts.touchingWall,
               player.contacts.touchingCeiling,
               wireframe ? "ON" : "OFF");
    }
}

void flushLines(const glm::mat4& viewProj) {
    gLineRenderer.flush(viewProj);
}

void shutdownRenderer() {
    if (gWorldMeshValid) {
        destroyMesh(gWorldMesh);
        gWorldMeshValid = false;
    }
    gSphereRenderer.mesh = Mesh();
    gCylinderRenderer.mesh = Mesh();
    gLineRenderer.destroy();
}
