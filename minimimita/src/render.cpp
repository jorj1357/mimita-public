#include "render.h"
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

GLuint gShader = 0;
bool gShaderReady = false;

static SphereRenderer gSphereRenderer;
static CylinderRenderer gCylinderRenderer;
static LineRenderer gLineRenderer;

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

void renderWorld(const std::vector<Triangle>& triangles,
                 const glm::mat4& viewProj) {
    if (!gShaderReady || triangles.empty()) return;

    glUseProgram(gShader);

    for (const auto& tri : triangles) {
        float verts[] = {
            tri.a.x, tri.a.y, tri.a.z,
            tri.b.x, tri.b.y, tri.b.z,
            tri.c.x, tri.c.y, tri.c.z
        };
        unsigned int idx[] = { 0, 1, 2 };

        GLuint vao, vbo, ebo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STREAM_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        setShaderMVP(gShader, viewProj);
        setShaderColor(gShader, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
        glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, 0);

        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
    }

    for (const auto& tri : triangles) {
        glm::vec3 e1 = tri.b - tri.a;
        glm::vec3 e2 = tri.c - tri.a;
        glm::vec3 center = (tri.a + tri.b + tri.c) / 3.0f;
        float edgeLen = glm::length(e1);
        glm::vec3 normal = glm::normalize(glm::cross(e2, e1));
        gLineRenderer.addLine(center, center + normal * edgeLen * 0.2f);
    }
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

void render2DText(const char* text, float x, float y, float scale,
                  const glm::mat4& viewProj) {
    static char buffer[99999];
    int numQuads = stb_easy_font_print(x, y, (char*)text, nullptr, buffer, sizeof(buffer));
    if (numQuads <= 0) return;

    int numVerts = numQuads * 4;
    std::vector<float> verts(numVerts * 3);
    std::vector<unsigned int> indices(numQuads * 6);

    // stb_easy_font vertex layout per vertex: float x, float y, float z, uint8_t color[4] = 16 bytes
    for (int i = 0; i < numVerts; ++i) {
        float* v = (float*)(buffer + i * 16);
        verts[i * 3 + 0] = v[0] * scale;
        verts[i * 3 + 1] = v[1] * scale;
        verts[i * 3 + 2] = 0.0f;
    }

    for (int q = 0; q < numQuads; ++q) {
        int qi = q * 4;
        indices[q * 6 + 0] = qi + 0;
        indices[q * 6 + 1] = qi + 1;
        indices[q * 6 + 2] = qi + 2;
        indices[q * 6 + 3] = qi + 0;
        indices[q * 6 + 4] = qi + 2;
        indices[q * 6 + 5] = qi + 3;
    }

    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    setShaderMVP(gShader, viewProj);
    setShaderColor(gShader, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    glDrawElements(GL_TRIANGLES, (int)indices.size(), GL_UNSIGNED_INT, 0);

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

void renderHUD(const Player& player, const TestMap& map, int w, int h) {
    glDisable(GL_DEPTH_TEST);

    glm::mat4 ortho = glm::ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    glUseProgram(gShader);
    setShaderColor(gShader, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    char buf[256];
    int lineY = 10;
    float textScale = 2.0f;

#define TEXT_LINE(fmt, ...) do { \
    snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
    render2DText(buf, 10.0f, (float)lineY, textScale, ortho); \
    lineY += 14; \
} while(0)

    TEXT_LINE("=== MINI-MIMITA COLLISION LAB ===");
    TEXT_LINE("Map: %s", map.name.c_str());
    TEXT_LINE("Pos: %.2f %.2f %.2f", player.position.x, player.position.y, player.position.z);
    TEXT_LINE("Vel: %.2f %.2f %.2f", player.velocity.x, player.velocity.y, player.velocity.z);
    TEXT_LINE("Grounded: %s  Contacts: %zu (F:%d W:%d C:%d)",
              player.grounded ? "YES" : "NO",
              player.contacts.contacts.size(),
              player.contacts.touchingFloor,
              player.contacts.touchingWall,
              player.contacts.touchingCeiling);
    TEXT_LINE("");
    TEXT_LINE("[1-7] Map  [R] Reset  [WASD] Move  [Space] Jump");
    TEXT_LINE("[Drag] Orbit  [Scroll] Zoom");

    if (player.position.z < -50.0f) {
        TEXT_LINE("");
        TEXT_LINE("*** FELL OFF MAP - PRESS R TO RESET ***");
    }

#undef TEXT_LINE

    glEnable(GL_DEPTH_TEST);
}

void flushLines(const glm::mat4& viewProj) {
    gLineRenderer.flush(viewProj);
}

void shutdownRenderer() {
    gSphereRenderer.mesh = Mesh();
    gCylinderRenderer.mesh = Mesh();
    gLineRenderer.destroy();
}
