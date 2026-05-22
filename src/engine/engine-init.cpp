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
#include "renderer/renderer.h"

// ---- GLOBAL ENGINE STATE ----
// THIS IS THE ONLY PLACE TO DEFINE IT 
// idk what to do here bruh mar 6 2026 
Renderer* gRenderer = nullptr;

void Engine::init(int w, int h, const char* title)
{
    static Renderer r(w, h, title);
    renderer = &r;
    gRenderer = &r;

    if (!renderer->window) {
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
    return renderer && !renderer->shouldClose();
}
