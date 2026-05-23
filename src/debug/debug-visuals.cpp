#include "debug-visuals.h"

#include <cstdio>
#include <cmath>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "entities/player.h"
#include "renderer/renderer.h"
#include "world/world.h"

extern Renderer* gRenderer;

namespace {
GLFWwindow* gWindow = nullptr;
DebugColors gColors;
bool gPhysics = true;
bool gUi = true;
bool gRender = true;
bool gCollision = true;
bool gWireframe = false;
bool gNormals = false;
bool gBounds = true;
bool gPrev[8] = {};
GLuint gLineVao = 0;
GLuint gLineVbo = 0;

bool edge(int idx, int key)
{
    bool down = glfwGetKey(gWindow, key) == GLFW_PRESS;
    bool hit = down && !gPrev[idx];
    gPrev[idx] = down;
    return hit;
}

void setLineState(const Camera& camera, glm::vec4 color)
{
    if (!gRenderer || !gRenderer->shaderProgram) {
        printf("[DEBUG WARNING] Cannot draw line: renderer/shader missing\n");
        return;
    }

    glUseProgram(gRenderer->shaderProgram);
    glm::mat4 model(1.0f);
    glm::mat4 view = camera.getView();
    glm::mat4 proj = camera.getProj((float)gRenderer->width, (float)gRenderer->height);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(gRenderer->shaderProgram, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 1);
    glUniform4fv(glGetUniformLocation(gRenderer->shaderProgram, "uColor"), 1, &color.x);
}

void drawLine(const Camera& camera, glm::vec3 a, glm::vec3 b, glm::vec4 color)
{
    if (!gLineVao) {
        glGenVertexArrays(1, &gLineVao);
        glGenBuffers(1, &gLineVbo);
    }

    glm::vec3 pts[2] = {a, b};
    glDisable(GL_DEPTH_TEST);
    setLineState(camera, color);
    glBindVertexArray(gLineVao);
    glBindBuffer(GL_ARRAY_BUFFER, gLineVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pts), pts, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_LINES, 0, 2);
    glEnable(GL_DEPTH_TEST);
}

void drawBox(const Camera& camera, glm::vec3 center, glm::vec3 half, glm::vec4 color)
{
    glm::vec3 v[8] = {
        center + glm::vec3(-half.x,-half.y,-half.z),
        center + glm::vec3( half.x,-half.y,-half.z),
        center + glm::vec3( half.x, half.y,-half.z),
        center + glm::vec3(-half.x, half.y,-half.z),
        center + glm::vec3(-half.x,-half.y, half.z),
        center + glm::vec3( half.x,-half.y, half.z),
        center + glm::vec3( half.x, half.y, half.z),
        center + glm::vec3(-half.x, half.y, half.z)
    };
    int e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (auto& edgePair : e)
        drawLine(camera, v[edgePair[0]], v[edgePair[1]], color);
}

void drawCapsuleApprox(const Player& player, const Camera& camera)
{
    Capsule c = player.getCapsule();
    glm::vec3 z(0,0,1);
    drawLine(camera, c.a, c.b, {1,0.1f,0.1f,1});
    drawLine(camera, c.a + glm::vec3(c.r,0,0), c.b + glm::vec3(c.r,0,0), {1,0.1f,0.1f,1});
    drawLine(camera, c.a - glm::vec3(c.r,0,0), c.b - glm::vec3(c.r,0,0), {1,0.1f,0.1f,1});
    drawLine(camera, c.a + glm::vec3(0,c.r,0), c.b + glm::vec3(0,c.r,0), {1,0.1f,0.1f,1});
    drawLine(camera, c.a - glm::vec3(0,c.r,0), c.b - glm::vec3(0,c.r,0), {1,0.1f,0.1f,1});
    drawLine(camera, c.a - z * c.r, c.a + z * c.r, {1,0.1f,0.1f,1});
    drawLine(camera, c.b - z * c.r, c.b + z * c.r, {1,0.1f,0.1f,1});
}
}

void DebugVis::init(GLFWwindow* win)
{
    gWindow = win;
    printf("[DEBUG] DebugVis initialized. F1 physics F2 UI F3 render F4 collision F5 wireframe F6 normals F7 bounds\n");
}

void DebugVis::update()
{
    if (!gWindow) return;
    if (edge(1, GLFW_KEY_F1)) { gPhysics = !gPhysics; printf("[DEBUG] physics=%d\n", gPhysics); }
    if (edge(2, GLFW_KEY_F2)) { gUi = !gUi; printf("[DEBUG] ui=%d\n", gUi); }
    if (edge(3, GLFW_KEY_F3)) { gRender = !gRender; printf("[DEBUG] render=%d\n", gRender); }
    if (edge(4, GLFW_KEY_F4)) { gCollision = !gCollision; printf("[DEBUG] collision=%d\n", gCollision); }
    if (edge(5, GLFW_KEY_F5)) { gWireframe = !gWireframe; printf("[DEBUG] wireframe=%d\n", gWireframe); }
    if (edge(6, GLFW_KEY_F6)) { gNormals = !gNormals; printf("[DEBUG] normals=%d\n", gNormals); }
    if (edge(7, GLFW_KEY_F7)) { gBounds = !gBounds; printf("[DEBUG] bounds=%d\n", gBounds); }
    glPolygonMode(GL_FRONT_AND_BACK, gWireframe ? GL_LINE : GL_FILL);
}

bool DebugVis::enabled() { return gPhysics || gCollision || gBounds || gNormals; }
bool DebugVis::physics() { return gPhysics; }
bool DebugVis::ui() { return gUi; }
bool DebugVis::render() { return gRender; }
bool DebugVis::collision() { return gCollision; }
bool DebugVis::wireframe() { return gWireframe; }
bool DebugVis::normals() { return gNormals; }
bool DebugVis::bounds() { return gBounds; }
const DebugColors& DebugVis::colors() { return gColors; }

void drawDebugStuff(const Player& player, const Camera& camera, const World& world)
{
    if (!DebugVis::enabled()) return;

    if (DebugVis::physics()) {
        drawCapsuleApprox(player, camera);
        drawLine(camera, player.pos, player.pos + player.vel * 0.25f, {0.0f,1.0f,0.2f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,-3), {0.2f,0.5f,1.0f,1.0f});
        drawLine(camera, player.pos, player.pos + glm::vec3(0,0,2), {1.0f,1.0f,0.0f,1.0f});
    }

    if (DebugVis::render()) {
        drawLine(camera, camera.pos, camera.pos + camera.front * 5.0f, {0.2f,0.8f,1.0f,1.0f});
    }

    if (DebugVis::bounds()) {
        drawBox(camera, player.pos + glm::vec3(0,0,1), {0.55f,0.55f,1.0f}, {1.0f,0.0f,1.0f,1.0f});
        int drawn = 0;
        for (const Block& b : world.blocks) {
            drawBox(camera, b.pos, b.size * 0.5f, {1.0f,1.0f,0.0f,0.85f});
            if (++drawn >= 24) break;
        }
    }

    if (gRenderer && gRenderer->shaderProgram) {
        glUseProgram(gRenderer->shaderProgram);
        glUniform1i(glGetUniformLocation(gRenderer->shaderProgram, "uUseColor"), 0);
    }
}
