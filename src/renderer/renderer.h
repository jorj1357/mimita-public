// C:\important\quiet\n\mimita-public-main\src\renderer\renderer.h
// jan 25 2026 small clean refactor

#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>

struct Renderer {
    GLFWwindow* window = nullptr;
    int width = 0;
    int height = 0;

    Renderer(int w, int h, const char* title);
    float beginFrame();
    void endFrame();
    bool shouldClose();
    void shutdown();
};
