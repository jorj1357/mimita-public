// C:\important\quiet\n\mimita-priv-v7\src\engine\engine-frame.cpp
// feb 10 2026
/**
 * piurpose 
 * frame rendering and buffer swap?
 * NO RENDERING logic
 * it just calls funcitons from the renderer or render file
 * idk , renderer or render 
 */

// #pragma message("COMPILING engine-frame.cpp")

#include "engine/engine.h"

float Engine::beginFrame()
{
    return renderer->beginFrame();
}

void Engine::endFrame()
{
    renderer->endFrame();
}
