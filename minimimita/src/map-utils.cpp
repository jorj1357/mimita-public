#include "map-utils.h"
#include "globals.h"
#include "maps.h"
#include "glb-loader.h"
#include "collision-grid.h"
#include "render.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

void loadTestMap(int idx) {
    gCurrentMapIndex = idx;
    gCurrentMap = getMap(gCurrentMapIndex);
    gPlayer.position = gCurrentMap.spawnPosition;
    gPlayer.velocity = glm::vec3(0.0f);
    gPlayer.contacts.clear();
    gCamera.target = gPlayer.position;
    buildCollisionGrid(gCurrentMap.triangles, 2.0f);
    rebuildWorldMesh(gCurrentMap.triangles);
    printf("[MAP] Loaded: %s (%zu triangles)\n",
           getMapName(gCurrentMapIndex), gCurrentMap.triangles.size());
}

void loadGLBMap(const char* name) {
    char path[512];
    snprintf(path, sizeof(path),
             "C:\\important\\mimita-priv-v8\\assets\\maps\\%s.glb", name);
    printf("[MAP] Loading GLB: %s\n", path);

    TestMap glbMap;
    if (!::loadGLBMap(path, glbMap)) {
        printf("[MAP] GLB load FAILED: %s\n", path);
        return;
    }

    gCurrentMapIndex = -1;
    gCurrentMap = glbMap;
    gPlayer.position = gCurrentMap.spawnPosition;
    gPlayer.velocity = glm::vec3(0.0f);
    gPlayer.contacts.clear();
    gCamera.target = gPlayer.position;
    printf("[MAP] Rebuilding world mesh...\n");
    buildCollisionGrid(gCurrentMap.triangles, 2.0f);
    rebuildWorldMesh(gCurrentMap.triangles);
    printf("[MAP] GLB loaded: %s (%zu triangles)\n", name, gCurrentMap.triangles.size());
}

void processCommand() {
    if (gInput.commandLen == 0) return;
    gInput.commandBuffer[gInput.commandLen] = '\0';

    if (strncmp(gInput.commandBuffer, "loadmap ", 8) == 0) {
        const char* name = gInput.commandBuffer + 8;
        loadGLBMap(name);
    } else if (strncmp(gInput.commandBuffer, "tp ", 3) == 0) {
        float x, y, z;
        if (sscanf(gInput.commandBuffer + 3, "%f %f %f", &x, &y, &z) == 3) {
            gPlayer.position = glm::vec3(x, y, z);
            gPlayer.velocity = glm::vec3(0.0f);
            printf("[CMD] Teleported to %.2f %.2f %.2f\n", x, y, z);
        } else {
            printf("[CMD] Usage: tp x y z\n");
        }
    } else {
        printf("[CMD] Unknown: %s\n", gInput.commandBuffer);
    }

    gInput.commandLen = 0;
    gInput.commandBuffer[0] = '\0';
}
