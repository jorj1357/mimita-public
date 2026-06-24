#include "input.h"
#include "globals.h"
#include "map-utils.h"
#include <cstdio>

static void handleNormalKeys(int key, bool pressed) {
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
                glfwSetWindowShouldClose(gWindow, 1);
            break;
    }
}

static void handleCommandKeys(int key, bool pressed) {
    (void)pressed;
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        if (gInput.commandLen < 255)
            gInput.commandBuffer[gInput.commandLen++] = 'a' + (key - GLFW_KEY_A);
        gInput.commandBuffer[gInput.commandLen] = '\0';
        return;
    }
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        if (gInput.commandLen < 255)
            gInput.commandBuffer[gInput.commandLen++] = '0' + (key - GLFW_KEY_0);
        gInput.commandBuffer[gInput.commandLen] = '\0';
        return;
    }
    if (key == GLFW_KEY_MINUS) {
        if (gInput.commandLen < 255)
            gInput.commandBuffer[gInput.commandLen++] = '-';
        gInput.commandBuffer[gInput.commandLen] = '\0';
        return;
    }
    if (key == GLFW_KEY_BACKSPACE && gInput.commandLen > 0)
        gInput.commandBuffer[--gInput.commandLen] = '\0';
    if (key == GLFW_KEY_ENTER)
        gInput.commandEnter = true;
    if (key == GLFW_KEY_ESCAPE) {
        gInput.commandLen = 0;
        gInput.commandBuffer[0] = '\0';
        glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    if (key == GLFW_KEY_SPACE) {
        if (gInput.commandLen < 255)
            gInput.commandBuffer[gInput.commandLen++] = ' ';
        gInput.commandBuffer[gInput.commandLen] = '\0';
    }
}

void keyCallback(GLFWwindow* win, int key, int, int action, int) {
    bool cmdMode = glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;

    if (action == GLFW_PRESS || action == GLFW_RELEASE) {
        bool pressed = (action == GLFW_PRESS);
        if (!cmdMode)
            handleNormalKeys(key, pressed);
        if (pressed && key == GLFW_KEY_GRAVE_ACCENT) {
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            gInput.commandLen = 0;
            gInput.commandBuffer[0] = '\0';
        }
    }

    if (action == GLFW_PRESS && !cmdMode && key >= GLFW_KEY_1 && key <= GLFW_KEY_7)
        loadTestMap(key - GLFW_KEY_1);

    if (action == GLFW_PRESS && cmdMode)
        handleCommandKeys(key, action == GLFW_PRESS);
}

void cursorPosCallback(GLFWwindow* win, double x, double y) {
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

void scrollCallback(GLFWwindow*, double, double yoff) {
    gCamera.distance *= (float)(1.0 - yoff * 0.1);
    gCamera.distance = glm::clamp(gCamera.distance, 0.5f, 30.0f);
}

void windowSizeCallback(GLFWwindow*, int w, int h) {
    gWinW = w > 0 ? w : 1;
    gWinH = h > 0 ? h : 1;
    glViewport(0, 0, gWinW, gWinH);
}

void errorCallback(int err, const char* desc) {
    fprintf(stderr, "GLFW error %d: %s\n", err, desc);
}
