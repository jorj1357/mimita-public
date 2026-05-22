// C:\important\quiet\n\mimita-priv-v7\src\engine\engine.h
// feb 10 2026
// purpose
/**
 * header for the src/engine folder
 * engine frame im not sure what it does 
 * and engine init idk that either
 * but both importnat  
 */

#pragma once

#include "renderer/renderer.h"
#include "camera.h"

struct Engine {
    Renderer* renderer = nullptr;
    Camera* activeCamera = nullptr;

    void init(int w, int h, const char* title);
    bool running() const;

    float beginFrame();
    void endFrame();

    GLFWwindow* window() const;

    void bindCamera(Camera* cam);
};
