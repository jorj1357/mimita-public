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
#include "avatar/cosmetic-system.h"
#include "config/player-visuals-config.h"
#include "debug/debug-log.h"
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
    bool isLocal,
    int localTeam)
{
    Player& p = const_cast<Player&>(player);
    AvatarSystem& av = AvatarSystem::instance();

    // NPCs carry their own per-life avatar identity. Bind it before queuing
    // the shared async model load; this keeps avatar work out of spawn/update.
    if (!p.avatarName().empty() &&
        (!p.avatarInstance || p.avatarInstance->name != p.avatarName()))
        av.applyAvatarToPlayer(p, p.avatarName());

    // Kick async load once per player
    if (!p.modelLoaded && !p.mLazyLoadRequested &&
        (av.hasAvatar() || p.avatarInstance)) {
        p.mLazyLoadRequested = true;
        if (!p.avatarInstance)
            av.requestModelLoad(p);
    }
    p.finalizeModelIfReady();
    av.finalizeAtlasIfReady(p);

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

    // Remote dead bodies are still drawn while their fall-over death animation
    // plays (frozen in place, rotating over totalTicks); only once it completes
    // (deathAnim.active false) is the body hidden instantly. The local player's
    // own body is never drawn, dead or alive.
    if (player.dead && isLocal) {
        if (logDraw)
            printf("[DRAW PLAYER] entityId=%u isLocal=%d submitted=0 reason=local-dead worldPos=(%.2f,%.2f,%.2f)\n",
                   networkEntityId, (int)isLocal,
                   player.pos.x, player.pos.y, player.pos.z);
        if (logDraw)
            lastLogMs[networkEntityId] = nowMs;
        return;
    }
    if (player.dead && !player.deathAnim.active) {
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

    const PlayerVisualsData& visuals = PlayerVisualsConfig::instance().data();
    const PlayerOutlineSettings* outline = &visuals.self;
    if (!isLocal)
        outline = (localTeam >= 0 && player.matchTeam >= 0 && localTeam == player.matchTeam)
            ? &visuals.teammate : &visuals.enemy;
    if (outline->enabled && outline->thickness > 0.0f && outline->alpha != 0.0f &&
        !(player.dead && outline->disappearOnDeath)) {
        const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blendWas = glIsEnabled(GL_BLEND);
        GLint depthFuncWas = GL_LESS;
        glGetIntegerv(GL_DEPTH_FUNC, &depthFuncWas);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (outline->visibleThroughWalls) glDisable(GL_DEPTH_TEST); else glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        const glm::vec4 color(outline->color.r / 255.0f, outline->color.g / 255.0f,
                              outline->color.b / 255.0f, outline->alpha);
        player.renderCurrentPose(gRenderer->shaderProgram, view, proj, true, hideHead,
                                 true, outline->thickness, color);
        glDepthMask(GL_TRUE);
        glDepthFunc(depthFuncWas);
        if (depthWas) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (blendWas) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        Debug::logThrottled(Debug::Category::Render, "player_visuals_outline", 1.0f,
            "[PLAYER VISUALS] outline entity=%u category=%s throughWalls=%d thickness=%.2f",
            networkEntityId, isLocal ? "self" : (localTeam >= 0 && player.matchTeam == localTeam ? "teammate" : "enemy"),
            outline->visibleThroughWalls ? 1 : 0, outline->thickness);
    }

    // Render attached cosmetic meshes (hats etc.) on top of the body.
    CosmeticSystem::instance().renderCosmetics(player);

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
    renderPlayerInternal(player, cam, 0, true, -1);
}

void renderNetworkPlayer(
    const Player& player,
    const Camera& cam,
    uint32_t networkEntityId,
    bool isLocal,
    int localTeam)
{
    renderPlayerInternal(player, cam, networkEntityId, isLocal, localTeam);
}
