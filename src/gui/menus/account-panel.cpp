#include "gui/menus/account-panel.h"
#include "gui/ui-system.h"
#include "entities/player.h"
#include "render/render-player.h"
#include "camera.h"
#include "auth/auth-system.h"
#include "physics/config.h"
#include "renderer/renderer.h"
#include "render/lighting-config.h"

#include <cstdio>
#include <algorithm>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static Player* gPreviewPlayer = nullptr;
static float gPreviewAngle = 0.0f;

extern Renderer* gRenderer;

namespace {

void setPlayerUniforms(GLuint shader)
{
    const auto& cfg = LightingConfig::instance();

    glUniform1i(glGetUniformLocation(shader, "uUseColor"), 0);
    glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    glUniform1i(glGetUniformLocation(shader, "uDebugView"), 0);
    glUniform1i(glGetUniformLocation(shader, "uShadowsEnabled"), 0);

    glm::vec3 ld = cfg.lightDir();
    glUniform3f(glGetUniformLocation(shader, "uLightDir"),
                ld.x, ld.y, ld.z);
    glUniform1f(glGetUniformLocation(shader, "uAmbientStrength"),
                cfg.ambientStrength());
    glUniform1f(glGetUniformLocation(shader, "uDiffuseStrength"),
                cfg.diffuseStrength());
    glUniform1f(glGetUniformLocation(shader, "uEdgeDarkness"),
                cfg.edgeDarkness());
    glUniform1f(glGetUniformLocation(shader, "uEdgeWidth"),
                cfg.edgeWidth());
    glUniform1f(glGetUniformLocation(shader, "uAODarkness"),
                cfg.aoDarkness());
    glUniform1f(glGetUniformLocation(shader, "uAOContrast"),
                cfg.aoContrast());
    glUniform1f(glGetUniformLocation(shader, "uTextureContrast"),
                cfg.textureContrast());
    glUniform1f(glGetUniformLocation(shader, "uTextureBrightness"),
                cfg.textureBrightness());
}

}

static Player* getPreviewPlayer()
{
    if (!gPreviewPlayer)
    {
        printf("[PREVIEW] creating preview player...\n");
        gPreviewPlayer = new Player();
        printf("[PREVIEW] modelLoaded=%d meshVerts=%zu bodyParts=%zu partMeshes=%zu\n",
               (int)gPreviewPlayer->modelLoaded,
               gPreviewPlayer->renderMesh.verts.size(),
               gPreviewPlayer->physicalBody.parts.size(),
               gPreviewPlayer->physicalBody.partMeshes.size());
        printf("[PREVIEW] pos=(%.2f,%.2f,%.2f) dead=%d maxHp=%d\n",
               gPreviewPlayer->pos.x,
               gPreviewPlayer->pos.y,
               gPreviewPlayer->pos.z,
               (int)gPreviewPlayer->dead,
               gPreviewPlayer->maxHp);
    }
    return gPreviewPlayer;
}

AccountPanelAction drawAccountPanel(GLFWwindow* window)
{
    AuthSystem& auth = AuthSystem::instance();
    AccountPanelAction result;
    float fbW = uiScreenW();
    float fbH = uiScreenH();

    float panelX = uiScaleX(1100.0f);
    float panelW = fbW - panelX - uiScaleX(60.0f);
    float panelCenterX = panelX + panelW * 0.5f;

    uiDrawRect({panelX, 0, panelW, fbH},
               {0.025f, 0.03f, 0.04f, 0.4f}, "account-panel-bg");
    uiDrawRectOutline({panelX, 0, panelW, fbH},
                      {0.3f, 0.4f, 0.55f, 0.15f}, "account-panel-border");

    Player* preview = getPreviewPlayer();
    glm::vec3 targetPos = preview ? preview->pos : glm::vec3(0.0f, 0.0f, 1.5f);

    if (auth.state() == AuthState::Authenticated)
    {
        const char* name = auth.user().username.c_str();
        float nameW = uiMeasureText(name, 0.45f);
        uiDrawText(name, panelCenterX - nameW * 0.5f,
                   uiScaleY(30.0f), 0.45f,
                   {0.85f, 0.9f, 1.0f, 1.0f});
    }
    else
    {
        float titleW = uiMeasureText("No Account Detected", 0.36f);
        uiDrawText("No Account Detected",
                   panelCenterX - titleW * 0.5f, uiScaleY(30.0f), 0.36f,
                   {0.75f, 0.8f, 0.9f, 1.0f});
        float hintW = uiMeasureText("Accounts are optional", 0.24f);
        uiDrawText("Accounts are optional",
                   panelCenterX - hintW * 0.5f, uiScaleY(60.0f), 0.24f,
                   {0.55f, 0.6f, 0.7f, 1.0f});
    }

    int fbWpx = 0, fbHpx = 0;
    glfwGetFramebufferSize(window, &fbWpx, &fbHpx);

    float previewTop = uiScaleY(90.0f);
    float previewAreaH = fbH * 0.40f;
    if (previewAreaH > panelW * 1.2f)
        previewAreaH = panelW * 1.2f;
    float previewBottom = previewTop + previewAreaH;

    if (preview && preview->modelLoaded)
    {
        gPreviewAngle += 0.045f;
        if (gPreviewAngle > 360.0f)
            gPreviewAngle -= 360.0f;

        Camera previewCam;
        previewCam.fov = 50.0f;
        float rad = glm::radians(gPreviewAngle);
        float dist = 8.0f;
        float height = 2.0f;
        previewCam.pos = glm::vec3(
            targetPos.x + std::cos(rad) * dist,
            targetPos.y + std::sin(rad) * dist,
            targetPos.z + height);
        previewCam.front = glm::normalize(
            targetPos + glm::vec3(0.0f, 0.0f, 1.5f) - previewCam.pos);
        previewCam.right = glm::normalize(
            glm::cross(previewCam.front, glm::vec3(0.0f, 0.0f, 1.0f)));
        previewCam.up = glm::normalize(
            glm::cross(previewCam.right, previewCam.front));

        GLint prevViewport[4];
        glGetIntegerv(GL_VIEWPORT, prevViewport);
        GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);

        float sx = (float)fbWpx / fbW;
        float sy = (float)fbHpx / fbH;
        int vx = (int)(panelX * sx);
        int vy = (int)((fbH - previewBottom) * sy);
        int vw = (int)(panelW * sx);
        int vh = (int)(previewAreaH * sy);
        glViewport(vx, vy, vw, vh);

        glEnable(GL_DEPTH_TEST);
        glClear(GL_DEPTH_BUFFER_BIT);
        glClearColor(0.04f, 0.045f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        setPlayerUniforms(gRenderer->shaderProgram);
        renderPlayer(*preview, previewCam);

        if (!depthWasEnabled)
            glDisable(GL_DEPTH_TEST);
        glViewport(prevViewport[0], prevViewport[1],
                   prevViewport[2], prevViewport[3]);
    }

    float hpBarY = previewBottom + uiScaleY(12.0f);
    float hpBarW = uiScaleX(150.0f);
    float hpBarH = uiScaleY(16.0f);
    float hpBarX = panelCenterX - hpBarW * 0.5f;

    if (preview && preview->modelLoaded)
    {
        float ratio = preview->maxHp > 0
            ? std::clamp((float)preview->currentHp / (float)preview->maxHp, 0.0f, 1.0f)
            : 1.0f;
        uiDrawRect({hpBarX, hpBarY, hpBarW, hpBarH},
                   {0.55f, 0.03f, 0.03f, 0.85f}, "preview-hp-bg");
        uiDrawRect({hpBarX, hpBarY, hpBarW * ratio, hpBarH},
                   {0.05f, 0.8f, 0.15f, 0.85f}, "preview-hp-fg");

        char hpText[32];
        snprintf(hpText, sizeof(hpText), "%d HP", preview->currentHp);
        float hpW = uiMeasureText(hpText, 0.28f);
        uiDrawText(hpText, panelCenterX - hpW * 0.5f,
                   hpBarY + hpBarH + uiScaleY(4.0f), 0.28f,
                   {1.0f, 1.0f, 1.0f, 0.85f});
    }

    float btnY = previewBottom + previewAreaH > fbH * 0.5f
        ? previewBottom + uiScaleY(65.0f)
        : fbH * 0.5f + uiScaleY(20.0f);
    float btnW = uiScaleX(260.0f);
    float btnH = uiScaleY(44.0f);

    if (auth.state() != AuthState::Authenticated)
    {
        if (uiButton(window, "Sign In",
                     {panelCenterX - btnW * 0.5f, btnY, btnW, btnH},
                     {0.2f, 0.5f, 0.85f, 1.0f}, "account-login").clicked)
        {
            printf("[PREVIEW] Sign In clicked\n");
            result.logIn = true;
        }

        float btn2Y = btnY + btnH + uiScaleY(10.0f);
        if (uiButton(window, "Sign Up",
                     {panelCenterX - btnW * 0.5f, btn2Y, btnW, btnH},
                     {0.2f, 0.5f, 0.25f, 1.0f}, "account-signup").clicked)
        {
            printf("[PREVIEW] Sign Up clicked\n");
            result.signUp = true;
        }
    }

    return result;
}
