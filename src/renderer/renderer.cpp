// C:\important\quiet\n\mimita-priv-v7\src\renderer\renderer.cpp
// feb 10 2026 REFACTOR INTO BEING SMALL YAYYY

// purpose
// Renderer creates the OpenGL context and GLAD
// Renderer does NOT own world shaders
// Renderer only exposes a “draw” API
// Your basic.vert / basic.frag stay exactly how they are

#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

// need camera here
#include "camera.h"
#include "renderer.h"

static std::string readTextFile(const char* path)
{
    printf("[RENDERER] opening file: %s\n", path);

    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("[RENDERER] fopen failed\n");
        return "";
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string text;
    text.resize(size);

    fread(text.data(), 1, size, f);
    fclose(f);

    printf("[RENDERER] loaded %ld bytes\n", size);

    return text;
}

static GLuint compileShader(GLenum type, const char* src, const char* debugName)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        printf("[RENDERER] Shader compile failed: %s\n%s\n", debugName, log);
    } else {
        printf("[RENDERER] Shader compile OK: %s\n", debugName);
    }

    return shader;
}

static GLuint createProgramFromFiles(const char* vertPath, const char* fragPath)
{
    printf("[RENDERER] loading shaders\n");
    std::string vertText = readTextFile(vertPath);
    std::string fragText = readTextFile(fragPath);

    if (vertText.empty() || fragText.empty()) {
        printf("[RENDERER] Shader source missing\n");
        return 0;
    }

    GLuint vs = compileShader(GL_VERTEX_SHADER,   vertText.c_str(), vertPath);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragText.c_str(), fragPath);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);

    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        printf("[RENDERER] Program link failed:\n%s\n", log);
    } else {
        printf("[RENDERER] Program link OK\n");
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

Renderer::Renderer(int w, int h, const char* title) {
    width = w;
    height = h;

    if (!glfwInit()) {
        printf("GLFW init failed\n");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!window) {
        printf("Window creation failed\n");
        glfwTerminate();
        return;
    }

    glfwSetCursorPosCallback(window,
    [](GLFWwindow* win, double x, double y)
    {
        Camera* cam = reinterpret_cast<Camera*>(glfwGetWindowUserPointer(win));
        if (cam) cam->updateMouse(x, y);
    });

    glfwMakeContextCurrent(window);

    // this might go here idk ? mar 6 2026
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // this just to test
    printf("[WINDOW] focused=%d\n", glfwGetWindowAttrib(window, GLFW_FOCUSED));

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("GLAD init failed\n");
        return;
    }

    printf("OpenGL %s\n", glGetString(GL_VERSION));
    glEnable(GL_DEPTH_TEST);

    // do this mar 14 2026 dont do direct paths 
    shaderProgram = createProgramFromFiles(
        "shaders/basic.vert",
        "shaders/basic.frag"
    );

    // absolute paths idk why mar 6 2026 testing fix
    // dont do this it sucks mar 14 2026
    // shaderProgram = createProgramFromFiles(
    //     "C:/important/quiet/n/mimita-priv-v7/shaders/basic.vert",
    //     "C:/important/quiet/n/mimita-priv-v7/shaders/basic.frag"
    // );

    printf("[RENDERER] shaderProgram=%u\n", shaderProgram);
}

float Renderer::beginFrame() {
    static double last = glfwGetTime();
    double now = glfwGetTime();
    float dt = float(now - last);
    last = now;

    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    return dt;
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
    glfwPollEvents();
}

bool Renderer::shouldClose() {
    return glfwWindowShouldClose(window);
}

void Renderer::shutdown() {
    if (shaderProgram) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
    glfwTerminate();
}