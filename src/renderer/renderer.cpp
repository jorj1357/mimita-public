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
#include <limits>
#include <sstream>
#include <string>

// need camera here
#include "camera.h"
#include "config.h"
#include "renderer.h"
#include "stb_image.h"
#include "utils/path_utils.h"
#include "debug/gl-debug.h"

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
    MIMITA_GL_CLEAR_STAGE("compileShader");
    GLuint shader = glCreateShader(type);
    MIMITA_GL_CHECK("glCreateShader");
    if (!shader)
        return 0;
    MIMITA_GL_CALL(glShaderSource(shader, 1, &src, nullptr));
    MIMITA_GL_CALL(glCompileShader(shader));

    GLint ok = 0;
    MIMITA_GL_CALL(glGetShaderiv(shader, GL_COMPILE_STATUS, &ok));

    if (!ok) {
        char log[2048];
        MIMITA_GL_CALL(glGetShaderInfoLog(shader, sizeof(log), nullptr, log));
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
    if (!vs || !fs)
    {
        if (vs) MIMITA_GL_CALL(glDeleteShader(vs));
        if (fs) MIMITA_GL_CALL(glDeleteShader(fs));
        return 0;
    }

    MIMITA_GL_CLEAR_STAGE("createProgramFromFiles");
    GLuint program = glCreateProgram();
    MIMITA_GL_CHECK("glCreateProgram");
    if (!program)
    {
        MIMITA_GL_CALL(glDeleteShader(vs));
        MIMITA_GL_CALL(glDeleteShader(fs));
        return 0;
    }
    MIMITA_GL_CALL(glAttachShader(program, vs));
    MIMITA_GL_CALL(glAttachShader(program, fs));
    MIMITA_GL_CALL(glLinkProgram(program));

    GLint ok = 0;
    MIMITA_GL_CALL(glGetProgramiv(program, GL_LINK_STATUS, &ok));

    if (!ok) {
        char log[2048];
        MIMITA_GL_CALL(glGetProgramInfoLog(program, sizeof(log), nullptr, log));
        printf("[RENDERER] Program link failed:\n%s\n", log);
    } else {
        printf("[RENDERER] Program link OK\n");
    }

    MIMITA_GL_CALL(glDeleteShader(vs));
    MIMITA_GL_CALL(glDeleteShader(fs));

    return program;
}

static bool dimensionsAreValidForCursor(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;

    if (width > 256 || height > 256)
        return false;

    if (width > std::numeric_limits<int>::max() / height / 4)
        return false;

    return true;
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

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
        if (!cam) return;
        if (glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
            cam->firstMouse = true;
            return;
        }
        cam->updateMouse(x, y);
    });

    if (CursorConfig::CUSTOM_CURSOR_ENABLED) {
        installCustomCursor(CursorConfig::CUSTOM_CURSOR_PATH, CursorConfig::CUSTOM_CURSOR_HOTSPOT_CENTERED);
    }

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
    MIMITA_GL_CLEAR_STAGE("Renderer::Renderer after GLAD");
    MIMITA_GL_CALL(glEnable(GL_DEPTH_TEST));

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

bool Renderer::installCustomCursor(const char* path, bool centeredHotspot)
{
    if (!window) {
        printf("[CURSOR] failed to install custom cursor: window is null\n");
        return false;
    }

    if (!path || !path[0]) {
        printf("[CURSOR] failed to load cursor.png: empty path\n");
        return false;
    }

    std::string resolvedPath = resolveAssetPath(path);

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    unsigned char* pixels = stbi_load(resolvedPath.c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha);
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        printf("[CURSOR] failed to load cursor.png path=%s reason=%s\n", resolvedPath.c_str(), reason ? reason : "unknown");
        return false;
    }

    if (!dimensionsAreValidForCursor(width, height)) {
        printf("[CURSOR] invalid cursor dimensions %dx%d path=%s\n", width, height, resolvedPath.c_str());
        stbi_image_free(pixels);
        return false;
    }

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = pixels;

    int hotspotX = centeredHotspot ? width / 2 : 0;
    int hotspotY = centeredHotspot ? height / 2 : 0;
    if (hotspotX < 0 || hotspotX >= width || hotspotY < 0 || hotspotY >= height) {
        printf("[CURSOR] invalid hotspot %d,%d for cursor %dx%d path=%s\n",
               hotspotX, hotspotY, width, height, resolvedPath.c_str());
        stbi_image_free(pixels);
        return false;
    }

    const char* oldDescription = nullptr;
    glfwGetError(&oldDescription);
    GLFWcursor* nextCursor = glfwCreateCursor(&image, hotspotX, hotspotY);
    stbi_image_free(pixels);

    if (!nextCursor) {
        const char* description = nullptr;
        int errorCode = glfwGetError(&description);
        printf("[CURSOR] cursor creation failed %dx%d hotspot=%d,%d glfwError=%d %s\n",
               width,
               height,
               hotspotX,
               hotspotY,
               errorCode,
               description ? description : "");
        return false;
    }

    destroyCustomCursor();
    customCursor = nextCursor;
    glfwSetCursor(window, customCursor);

    printf("[CURSOR] loaded custom cursor %dx%d channels=%d hotspot=%d,%d mode=%s\n",
           width,
           height,
           sourceChannels,
           hotspotX,
           hotspotY,
           centeredHotspot ? "centered" : "top-left");
    return true;
}

void Renderer::destroyCustomCursor()
{
    if (!customCursor)
        return;

    if (window)
        glfwSetCursor(window, nullptr);

    glfwDestroyCursor(customCursor);
    customCursor = nullptr;
    printf("[CURSOR] destroyed custom cursor\n");
}

float Renderer::beginFrame() {
    static double last = glfwGetTime();
    double now = glfwGetTime();
    float dt = float(now - last);
    last = now;

    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    MIMITA_GL_CLEAR_STAGE("Renderer::beginFrame");
    MIMITA_GL_CALL(glViewport(0, 0, width, height));
    MIMITA_GL_CALL(glClearColor(0.1f, 0.1f, 0.12f, 1.0f));
    MIMITA_GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    return dt;
}

void Renderer::endFrame() {
    glfwSwapBuffers(window);
    glfwPollEvents();
}

bool Renderer::shouldClose() {
    return window && glfwWindowShouldClose(window);
}

void Renderer::shutdown() {
    destroyCustomCursor();

    if (shaderProgram) {
        MIMITA_GL_CLEAR_STAGE("Renderer::shutdown");
        MIMITA_GL_CALL(glDeleteProgram(shaderProgram));
        shaderProgram = 0;
    }

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

void Renderer::applyVideoMode(int w, int h, bool fullscreen)
{
    if (!window) return;

    if (fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (!monitor) {
            printf("[RENDERER] No primary monitor, falling back to windowed\n");
            fullscreen = false;
        } else {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, w, h,
                                 mode ? mode->refreshRate : GLFW_DONT_CARE);
        }
    }

    if (!fullscreen) {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        int wx = 100, wy = 100;
        if (monitor) {
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            if (mode) {
                wx = (mode->width - w) / 2;
                wy = (mode->height - h) / 2;
            }
        }
        glfwSetWindowMonitor(window, nullptr, wx, wy, w, h, GLFW_DONT_CARE);
    }

    width = w;
    height = h;
    printf("[RENDERER] Video mode applied: %dx%d fullscreen=%d\n", w, h, (int)fullscreen);
}
