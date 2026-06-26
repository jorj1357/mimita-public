#include "gui/menus/account-panel.h"
#include "gui/ui-system.h"
#include "entities/player.h"
#include "render/render-player.h"
#include "camera.h"
#include "auth/auth-system.h"
#include "physics/config.h"

#include <cstdio>
#include <algorithm>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static Player* gPreviewPlayer = nullptr;
static float gPreviewAngle = 0.0f;

static Player* getPreviewPlayer()
{
    if (!gPreviewPlayer)
    {
        printf("[ACCOUNT PANEL] creating preview player...\n");
        gPreviewPlayer = new Player();
        printf("[ACCOUNT PANEL] preview player: modelLoaded=%d meshVerts=%zu\n",
               (int)gPreviewPlayer->modelLoaded,
               gPreviewPlayer->renderMesh.verts.size());
        printf("[ACCOUNT PANEL] preview player pos=(%.2f,%.2f,%.2f)\n",
               gPreviewPlayer->pos.x,
               gPreviewPlayer->pos.y,
               gPreviewPlayer->pos.z);
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

    // --- Username / No Account Detected at top ---
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

    // --- 3D character preview in the middle ---
    int fbWpx = 0, fbHpx = 0;
    glfwGetFramebufferSize(window, &fbWpx, &fbHpx);
    float scaleX = (float)fbWpx / fbW;
    float scaleY = (float)fbHpx / fbH;

    float previewTop = uiScaleY(90.0f);
    float previewAreaH = fbH * 0.40f;
    if (previewAreaH > panelW * 1.2f)
        previewAreaH = panelW * 1.2f;
    float previewBottom = previewTop + previewAreaH;

    int ppx = (int)(panelX * scaleX);
    int ppy = (int)((fbH - previewBottom) * scaleY);
    int ppw = (int)(panelW * scaleX);
    int pph = (int)(previewAreaH * scaleY);

    glm::vec3 targetPos = preview ? preview->pos : glm::vec3(0.0f, 0.0f, 1.5f);

    {
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_SCISSOR_TEST);
        glScissor(ppx, ppy, ppw, pph);
        glClearColor(0.04f, 0.045f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (preview && preview->modelLoaded)
        {
            gPreviewAngle += 0.015f;
            if (gPreviewAngle > 360.0f)
                gPreviewAngle -= 360.0f;

            Camera previewCam;
            float rad = glm::radians(gPreviewAngle);
            float dist = 7.0f;
            float height = 2.5f;
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

            renderPlayer(*preview, previewCam);
        }
        glDisable(GL_SCISSOR_TEST);
    }

    // --- Health bar BELOW preview ---
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

    // --- Buttons at bottom ---
    float btnY = previewBottom + previewAreaH > fbH * 0.5f
        ? previewBottom + uiScaleY(65.0f)
        : fbH * 0.5f + uiScaleY(20.0f);
    float btnW = uiScaleX(260.0f);
    float btnH = uiScaleY(44.0f);

    if (auth.state() != AuthState::Authenticated)
    {
        if (uiButton(window, "Log In",
                     {panelCenterX - btnW * 0.5f, btnY, btnW, btnH},
                     {0.2f, 0.5f, 0.85f, 1.0f}, "account-login").clicked)
        {
            printf("[ACCOUNT PANEL] Log In clicked\n");
            result.logIn = true;
        }

        float btn2Y = btnY + btnH + uiScaleY(10.0f);
        if (uiButton(window, "Sign Up",
                     {panelCenterX - btnW * 0.5f, btn2Y, btnW, btnH},
                     {0.2f, 0.5f, 0.25f, 1.0f}, "account-signup").clicked)
        {
            printf("[ACCOUNT PANEL] Sign Up clicked\n");
            result.signUp = true;
        }

        float btn3Y = btn2Y + btnH + uiScaleY(6.0f);
        if (uiButton(window, "Continue Offline",
                     {panelCenterX - btnW * 0.5f, btn3Y, btnW, uiScaleY(36.0f)},
                     {0.3f, 0.3f, 0.35f, 1.0f}, "account-offline").clicked)
        {
            printf("[ACCOUNT PANEL] Continue Offline clicked\n");
            result.continueOffline = true;
        }
    }

    return result;
}
