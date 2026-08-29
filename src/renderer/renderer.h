// C:\important\quiet\n\mimita-public-main\src\renderer\renderer.h
// jan 25 2026 small clean refactor

#pragma once
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <glad/glad.h>
#include <GLFW/glfw3.h>

struct Renderer {
    GLFWwindow* window = nullptr;
    GLFWcursor* customCursor = nullptr;
    int width = 0;
    int height = 0;

    // mar 6 2026 maibe fix? 
    GLuint shaderProgram = 0; 

    Renderer(int w, int h, const char* title);
    bool installCustomCursor(const char* path, bool centeredHotspot);
    void destroyCustomCursor();
    float beginFrame();
    void endFrame();
    bool shouldClose();
    void shutdown();
    void applyVideoMode(int w, int h, bool fullscreen);
    void setVSync(bool on);
    bool vsync() const { return mVSync; }

    // Force VSync OFF at the driver level. Called after context creation,
    // fullscreen transitions, config loads, and any attempt to enable VSync.
    void forceVSyncOff(const char* reason);

private:
    bool mVSync = false;
};
