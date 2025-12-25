// C:\important\go away v5\s\mimita-v5\src\renderer\renderer.h

#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

struct Renderer {
    GLFWwindow* window;
    Renderer(int w, int h, const char* title);
    float beginFrame();
    /*
    todo nov 6 2025
    remove phase out
        drawCube
        drawCubeColored
    remove these
    */

    void drawCube(const glm::vec3& pos, const glm::mat4& view, const glm::mat4& proj);
    void drawCubeColored(const glm::vec3& pos, const glm::vec3& color,
                         const glm::mat4& view, const glm::mat4& proj);
    // void drawGround(const glm::mat4& view, const glm::mat4& proj);
    void endFrame();
    bool shouldClose();
    void shutdown();
    void drawGridCell(const glm::vec3& pos, const glm::mat4& view, const glm::mat4& proj);
    // idk where to add this nov 6 2025 todo
    GLuint getShaderProgram(); // add this inside Renderer
    void drawDebugSphere(
        const glm::vec3& center,
        float radius,
        const glm::vec3& color,
        const glm::mat4& view,
        const glm::mat4& proj);

    void drawLine(
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec3& color,
        const glm::mat4& view,
        const glm::mat4& proj
    );
};

// Add this after the class (not inside)
void drawText2D(const char* text, float x, float y, float scale);
