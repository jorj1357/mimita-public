#include "render.h"
#include <cstdio>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static glm::vec4 colorForNormal(const glm::vec3& n) {
    float up = glm::dot(n, glm::vec3(0.0f, 0.0f, 1.0f));
    if (up > 0.85f)
        return glm::vec4(0.7f, 0.5f, 0.3f, 1.0f);
    if (up > 0.4f)
        return glm::vec4(0.6f, 0.55f, 0.4f, 1.0f);
    if (up > -0.4f)
        return glm::vec4(0.55f, 0.55f, 0.6f, 1.0f);
    if (up > -0.85f)
        return glm::vec4(0.35f, 0.35f, 0.4f, 1.0f);
    return glm::vec4(0.2f, 0.2f, 0.25f, 1.0f);
}

void rebuildWorldMesh(const std::vector<Triangle>& triangles) {
    if (gWorldMeshValid) {
        destroyMesh(gWorldMesh);
        gWorldMeshValid = false;
    }
    if (triangles.empty()) return;

    std::vector<float> verts(triangles.size() * 3 * 7);
    std::vector<unsigned int> indices(triangles.size() * 3);
    for (size_t i = 0; i < triangles.size(); ++i) {
        glm::vec4 c = colorForNormal(triangles[i].normal);
        size_t base = i * 3 * 7;
        auto addVert = [&](const glm::vec3& p) {
            verts[base++] = p.x; verts[base++] = p.y; verts[base++] = p.z;
            verts[base++] = c.r; verts[base++] = c.g; verts[base++] = c.b; verts[base++] = c.a;
        };
        addVert(triangles[i].a);
        addVert(triangles[i].b);
        addVert(triangles[i].c);
        indices[i*3+0] = (unsigned int)(i*3);
        indices[i*3+1] = (unsigned int)(i*3+1);
        indices[i*3+2] = (unsigned int)(i*3+2);
    }

    createMesh(gWorldMesh, verts.data(), (int)verts.size() / 7,
               indices.data(), (int)indices.size());
    gWorldMeshValid = true;
}

void renderWorld(const glm::mat4& viewProj, bool wireframe) {
    if (!gShaderReady || !gWorldMeshValid) return;

    if (wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glUseProgram(gShader);
    setShaderMVP(gShader, viewProj);
    glBindVertexArray(gWorldMesh.VAO);
    glDrawElements(GL_TRIANGLES, gWorldMesh.indexCount, GL_UNSIGNED_INT, 0);

    if (wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void renderPlayer(const Player& player, const glm::mat4& viewProj) {
    glm::vec3 a = player.capA();
    glm::vec3 b = player.capB();

    glm::vec4 headColor(0.95f, 0.8f, 0.7f, 1.0f);
    glm::vec4 torsoColor(0.2f, 0.4f, 0.8f, 1.0f);
    glm::vec4 legsColor(0.8f, 0.2f, 0.2f, 1.0f);

    gSphereRenderer.draw(viewProj, a, player.radius, legsColor);
    gCylinderRenderer.draw(viewProj, a, b, player.radius, torsoColor);
    gSphereRenderer.draw(viewProj, b, player.radius, headColor);
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

    static double lastPrint = 0.0;
    double now = glfwGetTime();
    if (now - lastPrint > 0.25) {
        lastPrint = now;
        printf("[HUD] Map: %s (%zu tri)  Pos: %.2f %.2f %.2f  Vel: %.2f %.2f %.2f  "
               "Grounded: %s  Contacts: %zu (F:%d W:%d C:%d)  Wire:%s\n",
               map.name.c_str(), map.triangles.size(),
               player.position.x, player.position.y, player.position.z,
               player.velocity.x, player.velocity.y, player.velocity.z,
               player.contacts.touchingFloor ? "Y" : "N",
               player.contacts.contacts.size(),
               player.contacts.touchingFloor,
               player.contacts.touchingWall,
               player.contacts.touchingCeiling,
               wireframe ? "ON" : "OFF");
    }
}

void doRender(const Player& player, const Camera& camera, const TestMap& map,
              int windowW, int windowH, bool wireframe) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)windowW / (float)windowH;
    glm::mat4 view = camera.view();
    glm::mat4 proj = camera.projection(aspect);
    glm::mat4 viewProj = proj * view;

    renderWorld(viewProj, wireframe);
    renderPlayer(player, viewProj);
    renderContacts(player, viewProj);
    flushLines(viewProj);
    renderHUD(player, map, windowW, windowH, wireframe);
}
