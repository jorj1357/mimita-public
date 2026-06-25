#include "render.h"
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

GLuint gShader = 0;
bool gShaderReady = false;

SphereRenderer gSphereRenderer;
CylinderRenderer gCylinderRenderer;
LineRenderer gLineRenderer;
Mesh gWorldMesh;
bool gWorldMeshValid = false;

static const char* vertSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uMVP;
out vec4 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

static const char* fragSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
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

void createMesh(Mesh& mesh, const float* verts, int vertCount,
                const unsigned int* indices, int indexCount) {
    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertCount * 7 * sizeof(float), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int),
                 indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    mesh.indexCount = indexCount;
}

void destroyMesh(Mesh& mesh) {
    if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
    if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
    if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
    mesh = Mesh();
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
