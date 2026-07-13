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
#include "terminal/terminal-state.h"
#include <chrono>
#include <cstdio>
#include <unordered_map>
#include <glad/glad.h>

extern Renderer* gRenderer;

namespace {

void renderPlayerInternal(
    const Player& player,
    const Camera& cam,
    uint32_t networkEntityId,
    bool isLocal)
{
    static std::unordered_map<uint32_t, uint64_t> lastLogMs;
    const uint64_t nowMs = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool logDraw =
        networkEntityId != 0 && nowMs - lastLogMs[networkEntityId] >= 1000;

    if (!gRenderer) {
        printf("[RENDER] renderer missing\n");
        if (logDraw)
            printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=0 reason=no-renderer\n",
                   networkEntityId, (int)isLocal);
        return;
    }

    if (player.dead) {
        if (logDraw)
            printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=0 reason=dead worldPos=(%.2f,%.2f,%.2f)\n",
                   networkEntityId, (int)isLocal,
                   player.pos.x, player.pos.y, player.pos.z);
        if (logDraw)
            lastLogMs[networkEntityId] = nowMs;
        return;
    }

    // printf("[RENDER] shaderProgram=%u\n", gRenderer->shaderProgram);

    glm::mat4 view = cam.getView();
    glm::mat4 proj = cam.getProj((float)gRenderer->width, (float)gRenderer->height);

    if (logDraw)
    {
        printf("[REMOTE PLAYER RENDER] entityId=%u modelLoaded=%d meshCount=%zu visible=1 "
               "position=(%.2f,%.2f,%.2f)\n",
               networkEntityId, (int)player.modelLoaded,
               player.physicalBody.partMeshes.size(),
               player.pos.x, player.pos.y, player.pos.z);
        printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=1 mesh=%s "
               "bodyParts=%zu fallbackBatches=%zu worldPos=(%.2f,%.2f,%.2f)\n",
               networkEntityId, (int)isLocal,
               player.modelLoaded ? "player-glb" : "fallback-capsule",
               player.physicalBody.partMeshes.size(),
               player.renderMesh.batches.size(),
               player.pos.x, player.pos.y, player.pos.z);
        lastLogMs[networkEntityId] = nowMs;
    }

    // Apply ghost rendering for server_showghost
    if (player.renderGhost)
    {
        GLuint prog = gRenderer->shaderProgram;
        GLint colorLoc = glGetUniformLocation(prog, "uColor");
        if (colorLoc >= 0)
        {
            // Semi-transparent dark teal tint for ghost
            glUniform4f(colorLoc, 0.0f, 0.5f, 0.5f, 0.4f);
        }
    }

    bool hideHead = isLocal && !cam.thirdPerson;
    player.render(
        gRenderer->shaderProgram,
        view,
        proj,
        hideHead
    );

    // Reset ghost color
    if (player.renderGhost)
    {
        GLuint prog = gRenderer->shaderProgram;
        GLint colorLoc = glGetUniformLocation(prog, "uColor");
        if (colorLoc >= 0)
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
    }
}

} // namespace

void renderPlayer(const Player& player, const Camera& cam)
{
    renderPlayerInternal(player, cam, 0, true);
}

void renderNetworkPlayer(
    const Player& player,
    const Camera& cam,
    uint32_t networkEntityId,
    bool isLocal)
{
    renderPlayerInternal(player, cam, networkEntityId, isLocal);
}
