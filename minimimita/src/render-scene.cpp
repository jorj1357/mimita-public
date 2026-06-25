#include "render.h"
#include "collision-grid.h"
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

    glm::vec3 up(0.0f, 0.0f, 1.0f);

    glm::vec3 cylA = a + up * player.radius;
    glm::vec3 cylB = b - up * player.radius;

    gSphereRenderer.draw(viewProj, a, player.radius, legsColor);

    if (cylB.z > cylA.z) {
        gCylinderRenderer.draw(viewProj, cylA, cylB, player.radius, torsoColor);
    }

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

static void renderDebugGrid(const glm::mat4& viewProj, const Player& player) {
    const auto& dbg = getCollisionDebug();
    if (!dbg.drawGrid) return;
    extern CollisionGrid gGrid;
    if (gGrid.cellsX <= 0 || gGrid.cellsY <= 0) return;

    // Determine player's chunk coordinate
    float px = player.position.x;
    float py = player.position.y;
    int pcx = (int)floor((px - gGrid.originX) / gGrid.cellSize);
    int pcy = (int)floor((py - gGrid.originY) / gGrid.cellSize);

    // Render chunks within 10m radius (5 chunks each direction at 2m cellSize)
    const int CHUNK_RADIUS = 5;
    int cx0 = std::max(0, pcx - CHUNK_RADIUS);
    int cx1 = std::min(gGrid.cellsX - 1, pcx + CHUNK_RADIUS);
    int cy0 = std::max(0, pcy - CHUNK_RADIUS);
    int cy1 = std::min(gGrid.cellsY - 1, pcy + CHUNK_RADIUS);

    // Chunk volume height: from floor to 6m above (covers standard player height + headroom)
    float chunkZ = 0.0f;
    float chunkH = 6.0f;

    glm::vec4 gridColor(0.3f, 0.5f, 0.7f, 0.25f);
    for (int cy = cy0; cy <= cy1; ++cy) {
        float y0 = gGrid.originY + cy * gGrid.cellSize;
        float y1 = y0 + gGrid.cellSize;
        for (int cx = cx0; cx <= cx1; ++cx) {
            float x0 = gGrid.originX + cx * gGrid.cellSize;
            float x1 = x0 + gGrid.cellSize;

            // Bottom face
            glm::vec3 blb(x0, y0, chunkZ), brb(x1, y0, chunkZ);
            glm::vec3 tlb(x0, y1, chunkZ), trb(x1, y1, chunkZ);
            // Top face
            glm::vec3 blt(x0, y0, chunkH), brt(x1, y0, chunkH);
            glm::vec3 tlt(x0, y1, chunkH), trt(x1, y1, chunkH);

            // Vertical edges
            gLineRenderer.addLine(blb, blt);
            gLineRenderer.addLine(brb, brt);
            gLineRenderer.addLine(tlb, tlt);
            gLineRenderer.addLine(trb, trt);

            // Bottom horizontal edges
            gLineRenderer.addLine(blb, brb);
            gLineRenderer.addLine(brb, trb);
            gLineRenderer.addLine(trb, tlb);
            gLineRenderer.addLine(tlb, blb);

            // Top horizontal edges (only draw if within 3 chunks for clarity)
            if (abs(cx - pcx) <= 2 && abs(cy - pcy) <= 2) {
                gLineRenderer.addLine(blt, brt);
                gLineRenderer.addLine(brt, trt);
                gLineRenderer.addLine(trt, tlt);
                gLineRenderer.addLine(tlt, blt);
            }
        }
    }
}

static void renderDebugCandidates(const glm::mat4& viewProj, const TestMap& map) {
    const auto& dbg = getCollisionDebug();
    if (!dbg.drawCandidates || dbg.testedTriangleIndices.empty()) return;
    for (int idx : dbg.testedTriangleIndices) {
        if (idx < 0 || idx >= (int)map.triangles.size()) continue;
        const auto& tri = map.triangles[idx];
        glm::vec4 color(1.0f, 1.0f, 0.0f, 0.3f);
        gLineRenderer.addLine(tri.a, tri.b);
        gLineRenderer.addLine(tri.b, tri.c);
        gLineRenderer.addLine(tri.c, tri.a);
    }
}

static void renderDebugCapsule(const glm::mat4& viewProj) {
    const auto& dbg = getCollisionDebug();
    if (dbg.playerRadius < 0.01f) return;
    glm::vec4 wireColor(0.5f, 0.8f, 1.0f, 0.5f);
    gCylinderRenderer.draw(viewProj, dbg.playerCapA, dbg.playerCapB,
                          dbg.playerRadius, wireColor);
}

void doRender(const Player& player, const Camera& camera, const TestMap& map,
              int windowW, int windowH, bool wireframe) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)windowW / (float)windowH;
    glm::mat4 view = camera.view();
    glm::mat4 proj = camera.projection(aspect);
    glm::mat4 viewProj = proj * view;

    renderWorld(viewProj, wireframe);
    renderDebugGrid(viewProj, player);
    renderDebugCandidates(viewProj, map);
    renderDebugCapsule(viewProj);
    renderPlayer(player, viewProj);
    renderContacts(player, viewProj);
    flushLines(viewProj);
    renderHUD(player, map, windowW, windowH, wireframe);
}
