#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "types.h"
#include <vector>

struct Mesh {
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    int indexCount = 0;
};

struct LineRenderer {
    GLuint VAO = 0;
    GLuint VBO = 0;
    std::vector<float> data;

    void init();
    void addLine(glm::vec3 from, glm::vec3 to);
    void flush(const glm::mat4& viewProj);
    void destroy();
};

struct SphereRenderer {
    Mesh mesh;
    void init(int sectors = 16, int rings = 12);
    void draw(const glm::mat4& viewProj, glm::vec3 pos, float radius, glm::vec4 color);
};

struct CylinderRenderer {
    Mesh mesh;
    void init(int segments = 16);
    void draw(const glm::mat4& viewProj, glm::vec3 from, glm::vec3 to,
              float radius, glm::vec4 color);
};

extern GLuint gShader;
extern bool gShaderReady;
extern SphereRenderer gSphereRenderer;
extern CylinderRenderer gCylinderRenderer;
extern LineRenderer gLineRenderer;
extern Mesh gWorldMesh;
extern bool gWorldMeshValid;

bool initRenderer();
void setShaderMVP(GLuint shader, const glm::mat4& mvp);
void createMesh(Mesh& mesh, const float* verts, int vertCount,
                const unsigned int* indices, int indexCount);
void destroyMesh(Mesh& mesh);

void rebuildWorldMesh(const std::vector<Triangle>& triangles);
void renderWorld(const glm::mat4& viewProj, bool wireframe);
void renderPlayer(const Player& player, const glm::mat4& viewProj);
void renderContacts(const Player& player, const glm::mat4& viewProj);
void renderHUD(const Player& player, const TestMap& map, int windowW, int windowH,
               bool wireframe);
void doRender(const Player& player, const Camera& camera, const TestMap& map,
              int windowW, int windowH, bool wireframe);
void flushLines(const glm::mat4& viewProj);
void shutdownRenderer();
