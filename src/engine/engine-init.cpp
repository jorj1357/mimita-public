// C:\important\quiet\n\mimita-priv-v7\src\engine\engine-init.cpp
// feb 10 2026
/**
 * purpose
 * im not sure
 * add in waht it does
 * but this initializes the engine and calls opengl andn like 
 * idk 
 * i reallt dont know bruh 
 */

// #pragma message("COMPILING engine-init.cpp")

#include <cstring>
#include "engine/engine.h"
#include <cstdio>
#include "game/render-device-bridge-module.h"
#include "renderer/renderer.h"

#ifdef MIMITA_GAME_DLL
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <windows.h>
namespace {
void* loadWindowsGlProc(const char* name) {
    if (!name) return nullptr;
    if (void* proc = reinterpret_cast<void*>(wglGetProcAddress(name)))
        return proc;
    static HMODULE opengl32 = LoadLibraryA("opengl32.dll");
    if (opengl32) {
        if (void* proc = reinterpret_cast<void*>(GetProcAddress(opengl32, name)))
            return proc;
    }
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}
}  // namespace
#endif

// ---- GLOBAL ENGINE STATE ----
// THIS IS THE ONLY PLACE TO DEFINE IT 
// idk what to do here bruh mar 6 2026 
Renderer* gRenderer = nullptr;

void Engine::init(int w, int h, const char* title)
{
#ifdef MIMITA_GAME_DLL
    renderer = mimita::game::acquireHostRenderer(w, h, title);
#else
    static Renderer residentRenderer(w, h, title);
    renderer = &residentRenderer;
#endif
    gRenderer = renderer;

#ifdef MIMITA_GAME_DLL
    // Transitional raw-GL compatibility bridge. The game DLL still contains
    // legacy raw GL call sites during the CAP_RENDER migration; load this
    // generation's dispatch table against the resident context. Remove this
    // block once CAP_RENDER covers the remaining paths.
    if (renderer && renderer->window) {
        glfwMakeContextCurrent(renderer->window);
        const int gladOk =
            gladLoadGLLoader(reinterpret_cast<GLADloadproc>(loadWindowsGlProc));
        printf("[GL BRIDGE] loaded=%d bindTexture=%p drawArrays=%p\n",
               gladOk, reinterpret_cast<void*>(glad_glBindTexture),
               reinterpret_cast<void*>(glad_glDrawArrays));
    }
#endif

    if (!renderer || !renderer->window) {
        printf("[ENGINE] Renderer failed to init\n");
        return;
    }

    printf("[ENGINE] Init OK (%dx%d)\n", w, h);
    printf("[ENGINE] OpenGL ready\n");
}

void Engine::bindCamera(Camera* cam)
{
    activeCamera = cam;
    printf("[ENGINE] Camera bound\n");
}

GLFWwindow* Engine::window() const
{
    return renderer ? renderer->window : nullptr;
}

bool Engine::running() const
{
    return renderer && renderer->window && !renderer->shouldClose();
}

void Engine::shutdown()
{
    if (!renderer)
        return;

    Renderer* shuttingDown = renderer;
    renderer->shutdown();
    renderer = nullptr;
    if (gRenderer == shuttingDown)
        gRenderer = nullptr;
}
