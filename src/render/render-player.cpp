// C:\important\quiet\n\mimita-priv-v7\src\render\render-player.cpp
// feb 10 2026
/**
 * purpose
 * rned erplauer
 * render plauer
 * wrapper
 * small wrapper so that the game faster i 
 * think
 */

#include "render-player.h"
#include "entities/player.h"
#include "camera.h"
#include "renderer/renderer.h"
#include <cstdio>

extern Renderer* gRenderer;

void renderPlayer(const Player& player, const Camera& cam)
{
    if (!gRenderer) {
        printf("[RENDER] renderer missing\n");
        return;
    }

    printf("[RENDER] shaderProgram=%u\n", gRenderer->shaderProgram);

    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->width, (float)gRenderer->height);

    player.render(
        gRenderer->shaderProgram,
        view,
        proj
    );
}