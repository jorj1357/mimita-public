#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <exception>
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
#include "glb-loader.h"

#define BOOT(fmt, ...) printf("[BOOT] " fmt "\n", ##__VA_ARGS__)

static GLFWwindow* gWindow = nullptr;
static int gWinW = 1280, gWinH = 800;
static Player gPlayer;
static Camera gCamera;
static InputState gInput;
static TestMap gCurrentMap;
static int gCurrentMapIndex = -1;
static bool gWireframeMode = false;
static double gLastTime = 0.0;
static float gAccumulator = 0.0f;

static void loadTestMap(int idx) {
    gCurrentMapIndex = idx;
    gCurrentMap = getMap(gCurrentMapIndex);
    gPlayer.position = gCurrentMap.spawnPosition;
    gPlayer.velocity = glm::vec3(0.0f);
    gPlayer.grounded = false;
    gPlayer.contacts.clear();
    gCamera.target = gPlayer.position;
    rebuildWorldMesh(gCurrentMap.triangles);
    printf("[MAP] Loaded: %s (%zu triangles)\n",
           getMapName(gCurrentMapIndex), gCurrentMap.triangles.size());
}

static void loadGLBMap(const char* name) {
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
    gPlayer.grounded = false;
    gPlayer.contacts.clear();
    gCamera.target = gPlayer.position;
    printf("[MAP] Rebuilding world mesh...\n");
    rebuildWorldMesh(gCurrentMap.triangles);
    printf("[MAP] GLB loaded: %s (%zu triangles)\n", name, gCurrentMap.triangles.size());
}

static void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    bool cmdMode = glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;

    if (action == GLFW_PRESS || action == GLFW_RELEASE) {
        bool pressed = (action == GLFW_PRESS);

        if (!cmdMode) {
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
                case GLFW_KEY_F:
                    if (pressed) {
                        gWireframeMode = !gWireframeMode;
                        printf("[DEBUG] Wireframe: %s\n", gWireframeMode ? "ON" : "OFF");
                    }
                    break;
                case GLFW_KEY_ESCAPE:
                    if (pressed)
                        glfwSetWindowShouldClose(win, 1);
                    break;
            }
        }

        if (pressed && key == GLFW_KEY_GRAVE_ACCENT) {
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            gInput.commandLen = 0;
            gInput.commandBuffer[0] = '\0';
        }
    }

    if (action == GLFW_PRESS && !cmdMode && key >= GLFW_KEY_1 && key <= GLFW_KEY_7) {
        loadTestMap(key - GLFW_KEY_1);
    }

    if (action == GLFW_PRESS && cmdMode) {
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
            if (gInput.commandLen < 255) {
                gInput.commandBuffer[gInput.commandLen++] = 'a' + (key - GLFW_KEY_A);
            }
            gInput.commandBuffer[gInput.commandLen] = '\0';
        }
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
            if (gInput.commandLen < 255) {
                gInput.commandBuffer[gInput.commandLen++] = '0' + (key - GLFW_KEY_0);
            }
            gInput.commandBuffer[gInput.commandLen] = '\0';
        }
        if (key == GLFW_KEY_MINUS) {
            if (gInput.commandLen < 255) {
                gInput.commandBuffer[gInput.commandLen++] = '-';
            }
            gInput.commandBuffer[gInput.commandLen] = '\0';
        }
        if (key == GLFW_KEY_BACKSPACE && gInput.commandLen > 0) {
            gInput.commandBuffer[--gInput.commandLen] = '\0';
        }
        if (key == GLFW_KEY_ENTER) {
            gInput.commandEnter = true;
        }
        if (key == GLFW_KEY_ESCAPE) {
            gInput.commandLen = 0;
            gInput.commandBuffer[0] = '\0';
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        if (key == GLFW_KEY_SPACE) {
            if (gInput.commandLen < 255) {
                gInput.commandBuffer[gInput.commandLen++] = ' ';
            }
            gInput.commandBuffer[gInput.commandLen] = '\0';
        }
    }
}

static void cursorPosCallback(GLFWwindow* win, double x, double y) {
    static double prevX = 0.0, prevY = 0.0;
    static bool first = true;
    if (first) { prevX = x; prevY = y; first = false; return; }

    int mode = glfwGetInputMode(win, GLFW_CURSOR);
    if (mode != GLFW_CURSOR_DISABLED) { prevX = x; prevY = y; return; }

    double dx = x - prevX;
    double dy = y - prevY;
    prevX = x;
    prevY = y;

    gCamera.yaw += (float)dx * 0.003f;
    gCamera.pitch += (float)dy * 0.003f;
    gCamera.pitch = glm::clamp(gCamera.pitch, -1.4f, 1.4f);
}

static void scrollCallback(GLFWwindow*, double, double yoff) {
    gCamera.distance *= (float)(1.0 - yoff * 0.1);
    gCamera.distance = glm::clamp(gCamera.distance, 0.5f, 30.0f);
}

static void windowSizeCallback(GLFWwindow*, int w, int h) {
    gWinW = w > 0 ? w : 1;
    gWinH = h > 0 ? h : 1;
    glViewport(0, 0, gWinW, gWinH);
}

static void errorCallback(int err, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}

static void processCommand() {
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

int main() {
    // Unbuffered stdout for crash-safe logging
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    try {
        BOOT("start");

        BOOT("GLFW init");
        glfwSetErrorCallback(errorCallback);
        if (!glfwInit()) {
            fprintf(stderr, "[FATAL] glfwInit failed\n");
            return 1;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        BOOT("create window");
        gWindow = glfwCreateWindow(gWinW, gWinH, "Mini-Mimita Collision Lab", nullptr, nullptr);
        if (!gWindow) {
            fprintf(stderr, "[FATAL] glfwCreateWindow failed\n");
            glfwTerminate();
            return 1;
        }

        glfwMakeContextCurrent(gWindow);

        BOOT("GLAD load");
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            fprintf(stderr, "[FATAL] gladLoadGLLoader failed\n");
            return 1;
        }

        BOOT("input callbacks");
        glfwSetKeyCallback(gWindow, keyCallback);
        glfwSetCursorPosCallback(gWindow, cursorPosCallback);
        glfwSetScrollCallback(gWindow, scrollCallback);
        glfwSetWindowSizeCallback(gWindow, windowSizeCallback);
        glfwSwapInterval(1);
        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        BOOT("OpenGL state");
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glViewport(0, 0, gWinW, gWinH);

        BOOT("renderer");
        if (!initRenderer()) {
            fprintf(stderr, "[FATAL] initRenderer failed\n");
            return 1;
        }

        BOOT("camera");
        gCamera.target = gPlayer.position;

        BOOT("player");
        gPlayer.position = gCurrentMap.spawnPosition;

        BOOT("map loader");
        loadTestMap(0);

        BOOT("physics");
        gLastTime = glfwGetTime();

        printf("\n=== MINI-MIMITA COLLISION LAB ===\n");
        printf("[WASD] Move  [Space] Jump  [R] Reset  [F] Wireframe\n");
        printf("[1-7] Test maps  [~] Command mode: loadmap <name>\n");
        printf("Maps: duels, garage, horror, memorial, ideas, funworld\n");
        printf("\n");

        while (!glfwWindowShouldClose(gWindow)) {
            double now = glfwGetTime();
            float dt = (float)(now - gLastTime);
            gLastTime = now;
            if (dt > 0.1f) dt = 0.1f;

            if (gInput.commandEnter) {
                processCommand();
                gInput.commandEnter = false;
                glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }

            glm::vec3 fwd = gCamera.forward2D();
            glm::vec3 right = gCamera.right2D();
            gInput.wishDir = glm::vec3(0.0f);
            if (gInput.w) gInput.wishDir += fwd;
            if (gInput.s) gInput.wishDir -= fwd;
            if (gInput.d) gInput.wishDir += right;
            if (gInput.a) gInput.wishDir -= right;

            gAccumulator += dt;
            while (gAccumulator >= PHYSICS_DT) {
                updatePlayer(gPlayer, gInput, gCurrentMap.triangles, PHYSICS_DT);
                gAccumulator -= PHYSICS_DT;
            }

            gCamera.target = gPlayer.position;

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            float aspect = (float)gWinW / (float)gWinH;
            glm::mat4 view = gCamera.view();
            glm::mat4 proj = gCamera.projection(aspect);
            glm::mat4 viewProj = proj * view;

            renderWorld(viewProj, gWireframeMode);
            renderPlayer(gPlayer, viewProj);
            renderContacts(gPlayer, viewProj);
            flushLines(viewProj);
            renderHUD(gPlayer, gCurrentMap, gWinW, gWinH, gWireframeMode);

            glfwSwapBuffers(gWindow);
            glfwPollEvents();
        }

        BOOT("shutdown renderer");
        shutdownRenderer();

        BOOT("destroy window");
        glfwDestroyWindow(gWindow);

        BOOT("GLFW terminate");
        glfwTerminate();

        BOOT("exit clean");
        return 0;

    } catch (const std::exception& e) {
        fprintf(stderr, "[FATAL] Unhandled exception: %s\n", e.what());
        return 1;
    } catch (...) {
        fprintf(stderr, "[FATAL] Unknown exception\n");
        return 1;
    }
}
