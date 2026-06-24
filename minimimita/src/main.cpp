#include "globals.h"
#include "init.h"
#include "map-utils.h"
#include "collision-grid.h"
#include "physics.h"
#include "render.h"
#include <cstdio>
#include <exception>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static void runFrame() {
    double now = glfwGetTime();
    float dt = (float)(now - gLastTime);
    gLastTime = now;
    if (dt > 0.1f) dt = 0.1f;

    if (gInput.commandEnter) {
        processCommand();
        gInput.commandEnter = false;
        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    computeWishDir(gInput, gCamera);

    gAccumulator += dt;
    while (gAccumulator >= PHYSICS_DT) {
        updatePlayer(gPlayer, gInput, gCurrentMap.triangles, PHYSICS_DT);
        gAccumulator -= PHYSICS_DT;
    }

    gCamera.target = gPlayer.position;
    doRender(gPlayer, gCamera, gCurrentMap, gWinW, gWinH, gWireframeMode);

    gFrameCount++;
    if (gFrameCount % 60 == 0) {
        printCollisionProfile();
        resetCollisionProfile();
    }

    glfwSwapBuffers(gWindow);
    glfwPollEvents();
}

int main() {
    try {
        if (!initGame()) return 1;
        while (!glfwWindowShouldClose(gWindow))
            runFrame();
        shutdownGame();
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "[FATAL] Unhandled exception: %s\n", e.what());
        return 1;
    } catch (...) {
        fprintf(stderr, "[FATAL] Unknown exception\n");
        return 1;
    }
}
