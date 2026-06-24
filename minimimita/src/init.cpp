#include "init.h"
#include "globals.h"
#include "input.h"
#include "map-utils.h"
#include "collision-grid.h"
#include "physics.h"
#include "render.h"
#include <cstdio>
#include <exception>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

static bool initGLFW() {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) {
        fprintf(stderr, "[FATAL] glfwInit failed\n");
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    gWindow = glfwCreateWindow(gWinW, gWinH, "Mini-Mimita Collision Lab", nullptr, nullptr);
    if (!gWindow) {
        fprintf(stderr, "[FATAL] glfwCreateWindow failed\n");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(gWindow);
    return true;
}

static bool initGLADAndGL() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "[FATAL] gladLoadGLLoader failed\n");
        return false;
    }
    glfwSetKeyCallback(gWindow, keyCallback);
    glfwSetCursorPosCallback(gWindow, cursorPosCallback);
    glfwSetScrollCallback(gWindow, scrollCallback);
    glfwSetWindowSizeCallback(gWindow, windowSizeCallback);
    glfwSwapInterval(1);
    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glViewport(0, 0, gWinW, gWinH);
    return true;
}

static bool initScene() {
    if (!initRenderer()) {
        fprintf(stderr, "[FATAL] initRenderer failed\n");
        return false;
    }
    gCamera.target = gPlayer.position;
    gPlayer.position = gCurrentMap.spawnPosition;
    loadTestMap(0);
    gLastTime = glfwGetTime();
    printf("\n=== MINI-MIMITA COLLISION LAB ===\n");
    printf("[WASD] Move  [Space] Jump  [R] Reset  [F] Wireframe\n");
    printf("[1-7] Test maps  [~] Command mode: loadmap <name>\n");
    printf("Maps: duels, garage, horror, memorial, ideas, funworld\n\n");
    return true;
}

bool initGame() {
    printf("[BOOT] start\n");
    printf("[BOOT] GLFW init\n");
    if (!initGLFW()) return false;
    printf("[BOOT] GLAD load\n");
    if (!initGLADAndGL()) return false;
    printf("[BOOT] input callbacks\n");
    printf("[BOOT] OpenGL state\n");
    printf("[BOOT] renderer\n");
    printf("[BOOT] camera\n");
    printf("[BOOT] player\n");
    printf("[BOOT] map loader\n");
    printf("[BOOT] physics\n");
    return initScene();
}

void shutdownGame() {
    printf("[BOOT] shutdown renderer\n");
    shutdownRenderer();
    printf("[BOOT] destroy window\n");
    glfwDestroyWindow(gWindow);
    printf("[BOOT] GLFW terminate\n");
    glfwTerminate();
    printf("[BOOT] exit clean\n");
}
