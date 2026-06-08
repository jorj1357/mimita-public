// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\settings-menu.cpp
// may 23 2026
/**
 * purpose
 * cleaner responsive-ish settings menu
 * using ui helpers instead of hardcoded chaos
 */

#include "settings-menu.h"
#include "../ui-system.h"
#include "camera.h"
#include "config/player-settings.h"
#include "renderer/renderer.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

extern Renderer* gRenderer;

static const char* kResolutions[] = {
    "800x600",
    "1024x768",
    "1280x960",
    "1920x1080"
};
static const int kNumResolutions = 4;

static const char* kGraphicsPresets[] = {
    "Minimal",
    "Low",
    "Medium",
    "High",
    "Max"
};
static const int kNumGraphicsPresets = 5;

static void applyResolution(const char* resStr)
{
    if (!gRenderer || !gRenderer->window) return;
    int w = 0, h = 0;
    if (sscanf(resStr, "%dx%d", &w, &h) != 2) return;
    if (w <= 0 || h <= 0) return;
    glfwSetWindowSize(gRenderer->window, w, h);
    gRenderer->width = w;
    gRenderer->height = h;
    printf("[SETTINGS] Resolution applied: %dx%d\n", w, h);
}

static int uiOptionDropdown(GLFWwindow* win, const char* label,
                            float x, float& y, float w, float h, float gap,
                            const char** options, int numOptions, int currentIndex)
{
    uiDrawText(label, x, y, 0.38f, {0.8f, 0.9f, 1.0f, 1.0f});
    y += gap * 0.5f;

    int result = -1;
    for (int i = 0; i < numOptions; ++i)
    {
        float optY = y;
        UIRect r = {x, optY, w, h};
        glm::vec4 color = (i == currentIndex)
            ? glm::vec4{0.3f, 0.7f, 1.0f, 1.0f}
            : glm::vec4{0.15f, 0.2f, 0.3f, 1.0f};
        uiDrawRect(r, color, label);
        if (i == currentIndex)
            uiDrawRectOutline(r, {0.5f, 0.9f, 1.0f, 1.0f}, label);
        uiDrawText(options[i], x + uiScaleX(8), optY + uiScaleY(4), 0.32f, {1.0f, 1.0f, 1.0f, 1.0f});
        if (uiButton(win, "", r, color).clicked)
            result = i;
        y += h + gap * 0.3f;
    }
    return result;
}

SettingsMenuResult drawSettingsMenu(GLFWwindow* win)
{
    SettingsMenuResult r{};

    PlayerSettings& settings = GetPlayerSettings();

    static bool fullscreen = false;
    static bool physicsDebug = true;
    static bool renderDebug = true;

    int w = 0;
    int h = 0;

    glfwGetFramebufferSize(win, &w, &h);

    float sw = (float)w;
    float sh = (float)h;

    //--------------------------------------------------
    // BACKGROUND
    //--------------------------------------------------

    uiDrawRect(
        {0,0,sw,sh},
        {0.025f,0.03f,0.04f,1.0f},
        "settings-background"
    );

    //--------------------------------------------------
    // TITLE
    //--------------------------------------------------

    uiDrawText(
        "SETTINGS / DEBUG UI TEST",
        sw * 0.5f - uiScaleX(240),
        uiScaleY(52),
        0.72f,
        {0.95f,0.98f,1.0f,1.0f}
    );

    uiDrawText(
        "LABELS | BUTTONS | SLIDERS | CHECKBOXES | IMAGE BUTTONS",
        sw * 0.5f - uiScaleX(360),
        uiScaleY(112),
        0.33f,
        {0.62f,0.82f,1.0f,1.0f}
    );

    //--------------------------------------------------
    // MAIN PANEL
    //--------------------------------------------------

    UIRect panel = {
        uiScaleX(42),
        uiScaleY(150),
        sw - uiScaleX(84),
        sh - uiScaleY(235)
    };

    uiDrawRect(
        panel,
        {0.08f,0.09f,0.12f,0.92f},
        "settings-panel"
    );

    uiDrawRectOutline(
        panel,
        {0.3f,0.8f,1.0f,1.0f},
        "settings-panel-outline"
    );

    //--------------------------------------------------
    // LEFT COLUMN
    //--------------------------------------------------

    float leftX = uiScaleX(80);

    float y = uiScaleY(240);

    float sliderW = uiScaleX(360);

    float gap = uiScaleY(36);

    if (uiSlider(
        win,
        "MASTER VOLUME",
        uiRow(leftX, y, sliderW, uiScaleY(28), gap),
        &settings.masterVolume,
        0.0f,
        1.0f
    )) SavePlayerSettings();

    if (uiSlider(
        win,
        "FOV",
        uiRow(leftX, y, sliderW, uiScaleY(28), gap),
        &settings.fov,
        60.0f,
        140.0f
    )) {
        CAMERA_FOV = settings.fov;
        SavePlayerSettings();
    }

    if (uiSlider(
        win,
        "SENSITIVITY",
        uiRow(leftX, y, sliderW, uiScaleY(28), gap),
        &settings.sensitivity,
        0.01f,
        1.0f
    )) {
        CAMERA_SENS = settings.sensitivity;
        SavePlayerSettings();
    }

    y += uiScaleY(12);

    uiCheckbox(
        win,
        "FULLSCREEN PLACEHOLDER",
        uiRow(leftX, y, uiScaleX(96), uiScaleY(42), gap),
        &fullscreen
    );

    uiCheckbox(
        win,
        "PHYSICS DEBUG OVERLAY",
        uiRow(leftX, y, uiScaleX(96), uiScaleY(42), gap),
        &physicsDebug
    );

    uiCheckbox(
        win,
        "RENDER DEBUG OVERLAY",
        uiRow(leftX, y, uiScaleX(96), uiScaleY(42), gap),
        &renderDebug
    );

    y += uiScaleY(16);

    {
        int idx = -1;
        for (int i = 0; i < kNumResolutions; ++i)
            if (settings.resolution == kResolutions[i]) { idx = i; break; }
        if (idx < 0) idx = 0;
        int next = uiOptionDropdown(win, "RESOLUTION",
            leftX, y, uiScaleX(200), uiScaleY(26), uiScaleY(20),
            kResolutions, kNumResolutions, idx);
        if (next >= 0 && next != idx)
        {
            settings.resolution = kResolutions[next];
            applyResolution(settings.resolution.c_str());
            SavePlayerSettings();
        }
    }

    {
        int idx = -1;
        for (int i = 0; i < kNumGraphicsPresets; ++i)
            if (settings.graphicsPreset == kGraphicsPresets[i]) { idx = i; break; }
        if (idx < 0) idx = 2;
        int next = uiOptionDropdown(win, "GRAPHICS PRESET",
            leftX, y, uiScaleX(200), uiScaleY(26), uiScaleY(20),
            kGraphicsPresets, kNumGraphicsPresets, idx);
        if (next >= 0 && next != idx)
        {
            settings.graphicsPreset = kGraphicsPresets[next];
            SavePlayerSettings();
        }
    }

    //--------------------------------------------------
    // RIGHT COLUMN
    //--------------------------------------------------

    float rightX = sw * 0.58f;

    float rightY = uiScaleY(240);

    uiPlaceholderImageButton(
        win,
        "missing-texture-test",
        {
            rightX,
            rightY,
            uiScaleX(150),
            uiScaleY(115)
        }
    );

    uiPlaceholderImageButton(
        win,
        "icon-button-test",
        {
            rightX + uiScaleX(190),
            rightY,
            uiScaleX(150),
            uiScaleY(115)
        }
    );

    //--------------------------------------------------
    // TEST BUTTON
    //--------------------------------------------------

    if (
        uiButton(
            win,
            "TEST BUTTON",
            uiCentered(
                uiScaleX(250),
                uiScaleY(58),
                sh * 0.62f
            ),
            {0.42f,0.72f,0.95f,1.0f}
        ).clicked
    )
    {
        printf("[SETTINGS MENU] Test button pressed\n");
    }

    //--------------------------------------------------
    // WARNING
    //--------------------------------------------------

    uiDrawWarning(
        "[MISSING FONT/TEXTURE WARNINGS ARE DRAWN HERE ON PURPOSE]",
        sw * 0.5f - uiScaleX(260),
        sh * 0.77f
    );

    //--------------------------------------------------
    // HOTKEYS
    //--------------------------------------------------

    uiDrawText(
        "F1 PHYSICS | F2 UI | F3 RENDER | F4 COLLISION | F5 WIREFRAME",
        uiScaleX(62),
        sh - uiScaleY(60),
        0.32f,
        {0.8f,1.0f,0.8f,1.0f}
    );

    //--------------------------------------------------
    // BACK BUTTON
    //--------------------------------------------------

    if (
        uiButton(
            win,
            "BACK",
            uiCentered(
                uiScaleX(180),
                uiScaleY(48),
                sh - uiScaleY(82)
            ),
            {0.75f,0.25f,0.25f,1.0f}
        ).clicked
    )
    {
        printf("[SETTINGS MENU] Back pressed\n");

        r.goBack = true;
    }

    return r;
}
