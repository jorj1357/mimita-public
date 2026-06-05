// C:\important\quiet\n\mimita-public-main\src\renderer\renderer.h
// jan 25 2026 small clean refactor

#pragma once
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
};
