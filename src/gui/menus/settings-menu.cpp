// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\settings-menu.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawSettingsMenu(args)
 *
 * this file DOES:
 * - show binds/debug/settings placeholders
 *
 * this file DOES NOT:
 * - edit real config yet
 */

#include "settings-menu.h"
#include "../ui-system.h"
#include <cstdio>

SettingsMenuResult drawSettingsMenu(GLFWwindow* win)
{
    SettingsMenuResult r{};
    static float volume = 75.0f;
    static float fov = 100.0f;
    static bool fullscreen = false;
    static bool physicsDebug = true;
    static bool renderDebug = true;

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    float cx = w * 0.5f;

    uiDrawRect({0, 0, (float)w, (float)h}, {0.025f, 0.03f, 0.04f, 1.0f}, "settings-background");
    uiDrawText("SETTINGS / DEBUG UI TEST", cx - 300.0f, 52.0f, 2.6f, {0.95f, 0.98f, 1.0f, 1.0f});
    uiDrawText("VISIBLE WIDGET TYPES: LABELS BUTTONS SLIDERS CHECKBOXES IMAGE PLACEHOLDERS SCROLL REGION", 46.0f, 112.0f, 1.3f, {0.62f,0.82f,1.0f,1});

    uiDrawRect({42, 150, (float)w - 84.0f, (float)h - 235.0f}, {0.08f, 0.09f, 0.12f, 0.92f}, "scrollable-region-placeholder");
    uiDrawRectOutline({42, 150, (float)w - 84.0f, (float)h - 235.0f}, {0.3f, 0.8f, 1.0f, 1.0f}, "scrollable-region-outline");
    uiDrawText("[SCROLLABLE REGION PLACEHOLDER]", 62.0f, 170.0f, 1.6f, {0.95f,0.8f,0.3f,1});

    uiSlider(win, "VOLUME", {80, 255, 360, 28}, &volume, 0.0f, 100.0f);
    uiSlider(win, "FOV", {80, 345, 360, 28}, &fov, 60.0f, 140.0f);
    uiCheckbox(win, "FULLSCREEN PLACEHOLDER", {80, 420, 96, 42}, &fullscreen);
    uiCheckbox(win, "PHYSICS DEBUG OVERLAY", {80, 490, 96, 42}, &physicsDebug);
    uiCheckbox(win, "RENDER DEBUG OVERLAY", {80, 560, 96, 42}, &renderDebug);

    uiPlaceholderImageButton(win, "missing-texture-test", {520, 245, 150, 115});
    uiPlaceholderImageButton(win, "icon-button-test", {700, 245, 150, 115});

    if (uiButton(win, "TEST BUTTON", {520, 410, 250, 58}, {0.42f, 0.72f, 0.95f, 1.0f}).clicked)
        printf("[SETTINGS MENU] Test button pressed\n");

    uiDrawWarning("[MISSING FONT/TEXTURE WARNINGS ARE DRAWN HERE ON PURPOSE]", 520, 510);
    uiDrawText("HOTKEYS F1 PHYSICS | F2 UI | F3 RENDER | F4 COLLISION | F5 WIREFRAME | F6 NORMALS | F7 BOUNDS", 62, (float)h - 60.0f, 1.3f, {0.8f, 1.0f, 0.8f, 1});

    if (uiButton(win, "BACK", {42, (float)h - 72.0f, 150, 48}, {0.75f,0.25f,0.25f,1.0f}).clicked)
    {
        printf("[SETTINGS MENU] Back pressed\n");
        r.goBack = true;
    }

    return r;
}
