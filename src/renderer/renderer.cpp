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
#include <cstdint>
#include <algorithm>
#include <limits>
#include <cstdlib>
#include <cstring>

extern bool gAiLaunchMode;
#include <string>

// need camera here
#include "config.h"
#include "renderer.h"
#include "stb_image.h"
#include "host/host-image-decode.h"
#include "host/host-asset-path.h"
#include "host/host-window-registry.h"
#include "debug/debug-log.h"
#include "debug/structured-log.h"

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
    sceneWidth = w;
    sceneHeight = h;

    if (!glfwInit()) {
        printf("GLFW init failed\n");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    const char* agentHeadlessEnv = std::getenv("MIMITA_AGENT_HEADLESS_GUI");
    const bool agentHeadless = agentHeadlessEnv &&
                               std::strcmp(agentHeadlessEnv, "1") == 0;
    if (gAiLaunchMode || agentHeadless)
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!window) {
        printf("Window creation failed\n");
        glfwTerminate();
        return;
    }
    mimita::host::setResidentWindow(window);

    glfwGetWindowPos(window, &mWindowedX, &mWindowedY);
    mHasWindowedPosition = true;

    // Keep AI/dev launches from taking over the user's desktop. The window is
    // created hidden, then shown minimized so it remains restorable/playable.
    if (gAiLaunchMode && !agentHeadless) {
        glfwShowWindow(window);
        glfwIconifyWindow(window);
    }

    // Look input is routed by the host InputRouter into the game generation.
    // The resident renderer owns only the window and GL device.

    if (CursorConfig::CUSTOM_CURSOR_ENABLED) {
        installCustomCursor(CursorConfig::CUSTOM_CURSOR_PATH, CursorConfig::CUSTOM_CURSOR_HOTSPOT_CENTERED);
    }

    glfwMakeContextCurrent(window);

    // VSync is FORCED OFF and cannot be re-enabled.
    glfwSwapInterval(0);
    Debug::log(Debug::Category::Render,
        "[VSYNC] forced OFF: glfwSwapInterval(0) after context creation\n");

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

    std::string resolvedPath = hostResolveAssetPath(path);

    int width = 0;
    int height = 0;
    int sourceChannels = 0;
    unsigned char* pixels = hostStbiLoad(resolvedPath.c_str(), &width, &height, &sourceChannels, STBI_rgb_alpha);
    if (!pixels) {
        const char* reason = hostStbiFailureReason();
        printf("[CURSOR] failed to load cursor.png path=%s reason=%s\n", resolvedPath.c_str(), reason ? reason : "unknown");
        return false;
    }

    if (!dimensionsAreValidForCursor(width, height)) {
        printf("[CURSOR] invalid cursor dimensions %dx%d path=%s\n", width, height, resolvedPath.c_str());
        hostStbiImageFree(pixels);
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
        hostStbiImageFree(pixels);
        return false;
    }

    const char* oldDescription = nullptr;
    glfwGetError(&oldDescription);
    GLFWcursor* nextCursor = glfwCreateCursor(&image, hotspotX, hotspotY);
    hostStbiImageFree(pixels);

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

    // Clamp the frame delta so a long presentation stall (e.g. ~1 s under
    // Wine) cannot inject a huge simulation step or a giant animation jump.
    // Mirrors FramePacer::beginFrame clamping (max 0.1 s).
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    const bool framebufferValid =
        framebufferWidth > 0 && framebufferHeight > 0;
    if (framebufferValid) {
        width = framebufferWidth;
        height = framebufferHeight;
        if (!fullscreenModeIsStretched(mAppliedVideoMode)) {
            sceneWidth = width;
            sceneHeight = height;
        }
    }
    // A hidden or minimized window can report 0x0. Never turn that into a
    // 1x1 viewport/FBO; keep the last valid presentation size for this frame.
    if (width <= 0 || height <= 0) {
        return dt;
    }
    {
        static std::uint64_t lastSizeLogMs = 0;
        static int lastLoggedWidth = -1;
        static int lastLoggedHeight = -1;
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if ((!framebufferValid || framebufferWidth != lastLoggedWidth ||
             framebufferHeight != lastLoggedHeight) &&
            static_cast<std::uint64_t>(nowMs) - lastSizeLogMs >= 1000) {
            lastSizeLogMs = static_cast<std::uint64_t>(nowMs);
            lastLoggedWidth = framebufferWidth;
            lastLoggedHeight = framebufferHeight;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::Rendering, StructuredLevel::Important,
                "renderer.frame-size", "render",
                "renderer observed framebuffer and presentation size", 0,
                nlohmann::json{
                    {"framebuffer_width", framebufferWidth},
                    {"framebuffer_height", framebufferHeight},
                    {"presentation_width", width},
                    {"presentation_height", height},
                    {"scene_width", sceneWidth},
                    {"scene_height", sceneHeight},
                    {"window_visible", glfwGetWindowAttrib(window, GLFW_VISIBLE)},
                    {"window_iconified", glfwGetWindowAttrib(window, GLFW_ICONIFIED)}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
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
    return window && glfwWindowShouldClose(window);
}

void Renderer::shutdown() {
    destroyCustomCursor();

    if (window) {
        mimita::host::setResidentWindow(nullptr);
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

FullscreenMode Renderer::applyVideoMode(int gameWidth, int gameHeight, FullscreenMode mode)
{
    if (!window) return mAppliedVideoMode;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* desktopMode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (monitor && desktopMode && mAppliedVideoMode == FullscreenMode::Windowed) {
        mDesktopWidth = desktopMode->width;
        mDesktopHeight = desktopMode->height;
        mDesktopRefreshRate = desktopMode->refreshRate;
    }
    const int desktopWidth = mDesktopWidth > 0
        ? mDesktopWidth : (desktopMode ? desktopMode->width : 0);
    const int desktopHeight = mDesktopHeight > 0
        ? mDesktopHeight : (desktopMode ? desktopMode->height : 0);
    const int desktopRefreshRate = mDesktopRefreshRate != GLFW_DONT_CARE
        ? mDesktopRefreshRate : (desktopMode ? desktopMode->refreshRate : GLFW_DONT_CARE);
    if (mode != FullscreenMode::Windowed &&
        (!monitor || desktopWidth <= 0 || desktopHeight <= 0)) {
        printf("[RENDERER] No primary monitor mode; falling back to windowed\n");
        mode = FullscreenMode::Windowed;
    }

    if (mAppliedVideoMode == FullscreenMode::Windowed &&
        mode != FullscreenMode::Windowed) {
        glfwGetWindowPos(window, &mWindowedX, &mWindowedY);
        mHasWindowedPosition = true;
    }

    int monitorX = 0;
    int monitorY = 0;
    if (monitor)
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);

    int outputWidth = gameWidth;
    int outputHeight = gameHeight;
    const bool stretched = fullscreenModeIsStretched(mode);
    const bool fullFrameStretched = fullscreenModeStretchesWholeFrame(mode);
    const int sceneGameWidth = fullFrameStretched ? 1024 : gameWidth;
    const int sceneGameHeight = fullFrameStretched ? 768 : gameHeight;

    switch (mode) {
        case FullscreenMode::Windowed: {
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
            int x = mHasWindowedPosition ? mWindowedX : monitorX + 100;
            int y = mHasWindowedPosition ? mWindowedY : monitorY + 100;
            if (desktopWidth > 0 && desktopHeight > 0) {
                x = std::clamp(x, monitorX, monitorX + std::max(0, desktopWidth - gameWidth));
                y = std::clamp(y, monitorY, monitorY + std::max(0, desktopHeight - gameHeight));
            }
            glfwSetWindowMonitor(window, nullptr, x, y, gameWidth, gameHeight, GLFW_DONT_CARE);
            sceneWidth = gameWidth;
            sceneHeight = gameHeight;
            break;
        }
        case FullscreenMode::Exclusive: {
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
            outputWidth = gameWidth;
            outputHeight = gameHeight;
            glfwSetWindowMonitor(window, monitor, monitorX, monitorY,
                                 outputWidth, outputHeight, GLFW_DONT_CARE);
            sceneWidth = gameWidth;
            sceneHeight = gameHeight;
            break;
        }
        case FullscreenMode::ExclusiveStretched:
        case FullscreenMode::ExclusiveFullFrameStretched: {
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
            outputWidth = desktopWidth;
            outputHeight = desktopHeight;
            glfwSetWindowMonitor(window, monitor, monitorX, monitorY,
                                 outputWidth, outputHeight, desktopRefreshRate);
            sceneWidth = sceneGameWidth;
            sceneHeight = sceneGameHeight;
            break;
        }
        case FullscreenMode::Borderless:
        case FullscreenMode::BorderlessStretched:
        case FullscreenMode::BorderlessFullFrameStretched: {
            glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
            outputWidth = desktopWidth;
            outputHeight = desktopHeight;
            glfwSetWindowMonitor(window, nullptr, monitorX, monitorY,
                                 outputWidth, outputHeight, GLFW_DONT_CARE);
            sceneWidth = stretched ? sceneGameWidth : outputWidth;
            sceneHeight = stretched ? sceneGameHeight : outputHeight;
            break;
        }
    }

    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0) width = outputWidth;
    if (height <= 0) height = outputHeight;
    if (!stretched) {
        sceneWidth = width;
        sceneHeight = height;
    }
    sceneWidth = std::max(sceneWidth, 1);
    sceneHeight = std::max(sceneHeight, 1);
    mAppliedVideoMode = mode;
    printf("[RENDERER] Video mode applied: mode=%d (%s) scene=%dx%d output=%dx%d\n",
           static_cast<int>(mode), fullscreenModeName(mode), sceneWidth,
           sceneHeight, width, height);

    // Some GLFW/driver paths reset the swap interval when the window monitor
    // changes (fullscreen/windowed switch). Always force OFF.
    glfwSwapInterval(0);
    mVSync = false;
    return mode;
}

void Renderer::setVSync(bool on)
{
    if (on) {
        Debug::warn(Debug::Category::Render,
            "[VSYNC] BLOCKED attempt to enable VSync (swap interval forced to 0)\n");
    }
    if (window)
        glfwSwapInterval(0);
    mVSync = false;
    Debug::log(Debug::Category::Render,
        "[VSYNC] swap interval=0 (forced OFF)\n");
}

void Renderer::forceVSyncOff(const char* reason)
{
    mVSync = false;
    if (window)
        glfwSwapInterval(0);
    Debug::log(Debug::Category::Render,
        "[VSYNC] forced OFF: reason=%s\n", reason ? reason : "unknown");
}
