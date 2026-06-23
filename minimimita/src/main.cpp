#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "types.h"
#include "physics.h"
#include "render.h"
#include "maps.h"

static GLFWwindow* gWindow = nullptr;
static int gWinW = 1280, gWinH = 800;
static Player gPlayer;
static Camera gCamera;
static InputState gInput;
static TestMap gCurrentMap;
static int gCurrentMapIndex = 0;
static double gLastTime = 0.0;
static float gAccumulator = 0.0f;
static double gPrevMouseX = 0.0, gPrevMouseY = 0.0;
static bool gFirstMouse = true;

static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    if (action == GLFW_PRESS || action == GLFW_RELEASE) {
        bool pressed = (action == GLFW_PRESS);
        switch (key) {
            case GLFW_KEY_W: gInput.w = pressed; break;
            case GLFW_KEY_A: gInput.a = pressed; break;
            case GLFW_KEY_S: gInput.s = pressed; break;
            case GLFW_KEY_D: gInput.d = pressed; break;
            case GLFW_KEY_SPACE:
                gInput.spacePrev = gInput.space;
                gInput.space = pressed;
                break;
            case GLFW_KEY_R:
                if (pressed) {
                    gPlayer.position = gCurrentMap.spawnPosition;
                    gPlayer.velocity = glm::vec3(0.0f);
                    gPlayer.grounded = false;
                    gPlayer.contacts.clear();
                }
                break;
        }
    }

    if (action == GLFW_PRESS && key >= GLFW_KEY_1 && key <= GLFW_KEY_7) {
        int idx = key - GLFW_KEY_1;
        if (idx >= 0 && idx < MAP_COUNT) {
            gCurrentMapIndex = idx;
            gCurrentMap = getMap(gCurrentMapIndex);
            gPlayer.position = gCurrentMap.spawnPosition;
            gPlayer.velocity = glm::vec3(0.0f);
            gPlayer.grounded = false;
            gPlayer.contacts.clear();
            gCamera.target = gPlayer.position;
            printf("[MAP] Switched to: %s\n", getMapName(gCurrentMapIndex));
        }
    }
}

static void mouseButtonCallback(GLFWwindow* win, int btn, int action, int) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        gInput.mouseDown = (action == GLFW_PRESS);
        if (action == GLFW_PRESS) {
            glfwGetCursorPos(win, &gPrevMouseX, &gPrevMouseY);
            gFirstMouse = true;
        }
    }
}

static void cursorPosCallback(GLFWwindow* win, double x, double y) {
    if (!gInput.mouseDown) return;
    if (gFirstMouse) {
        gPrevMouseX = x;
        gPrevMouseY = y;
        gFirstMouse = false;
        return;
    }

    double dx = x - gPrevMouseX;
    double dy = y - gPrevMouseY;
    gPrevMouseX = x;
    gPrevMouseY = y;

    gCamera.yaw += (float)dx * 0.005f;
    gCamera.pitch += (float)dy * 0.005f;
    gCamera.pitch = glm::clamp(gCamera.pitch, -1.4f, 1.4f);
}

static void scrollCallback(GLFWwindow*, double, double yoff) {
    gCamera.distance *= (float)(1.0 - yoff * 0.1);
    gCamera.distance = glm::clamp(gCamera.distance, 1.0f, 50.0f);
}

static void windowSizeCallback(GLFWwindow*, int w, int h) {
    gWinW = w > 0 ? w : 1;
    gWinH = h > 0 ? h : 1;
    glViewport(0, 0, gWinW, gWinH);
}

static void errorCallback(int err, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}

int main() {
    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to init GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    gWindow = glfwCreateWindow(gWinW, gWinH, "Mini-Mimita Collision Lab", nullptr, nullptr);
    if (!gWindow) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(gWindow);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to init GLAD\n");
        return 1;
    }

    glfwSetKeyCallback(gWindow, keyCallback);
    glfwSetMouseButtonCallback(gWindow, mouseButtonCallback);
    glfwSetCursorPosCallback(gWindow, cursorPosCallback);
    glfwSetScrollCallback(gWindow, scrollCallback);
    glfwSetWindowSizeCallback(gWindow, windowSizeCallback);
    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    glViewport(0, 0, gWinW, gWinH);
    glfwGetCursorPos(gWindow, &gPrevMouseX, &gPrevMouseY);

    if (!initRenderer()) {
        fprintf(stderr, "Failed to init renderer\n");
        return 1;
    }

    gCurrentMap = getMap(0);
    gPlayer.position = gCurrentMap.spawnPosition;
    gCamera.target = gPlayer.position;
    gLastTime = glfwGetTime();

    printf("=== MINI-MIMITA COLLISION LAB ===\n");
    printf("[1-7] Switch Map  [R] Reset  [WASD] Move  [Space] Jump\n");
    printf("[Drag] Orbit Camera  [Scroll] Zoom\n");
    printf("\nCurrent: %s\n", getMapName(0));

    while (!glfwWindowShouldClose(gWindow)) {
        double now = glfwGetTime();
        float dt = (float)(now - gLastTime);
        gLastTime = now;
        if (dt > 0.1f) dt = 0.1f;

        gAccumulator += dt;
        while (gAccumulator >= PHYSICS_DT) {
            updatePlayer(gPlayer, gInput, gCurrentMap.triangles, PHYSICS_DT);
            gAccumulator -= PHYSICS_DT;
        }

        gCamera.target += (gPlayer.position - gCamera.target) * 0.1f;

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (float)gWinW / (float)gWinH;
        glm::mat4 view = gCamera.view();
        glm::mat4 proj = gCamera.projection(aspect);
        glm::mat4 viewProj = proj * view;

        renderWorld(gCurrentMap.triangles, viewProj);
        renderPlayer(gPlayer, viewProj);
        renderContacts(gPlayer, viewProj);
        flushLines(viewProj);
        renderHUD(gPlayer, gCurrentMap, gWinW, gWinH);

        glfwSwapBuffers(gWindow);
        glfwPollEvents();
    }

    shutdownRenderer();
    glfwDestroyWindow(gWindow);
    glfwTerminate();
    return 0;
}
