// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\main-menu.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawMainMenu(args)
 *
 * this file DOES:
 * - draw main menu buttons and title
 *
 * this file DOES NOT:
 * - mutate game state directly
 */

#include "main-menu.h"
#include "../ui-system.h"
#include <cstdio>

MainMenuResult drawMainMenu(GLFWwindow* win)
{
    MainMenuResult r{};

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    uiDrawRect({0, 0, (float)w, (float)h}, {0.035f, 0.04f, 0.052f, 1.0f}, "main-menu-background");
    uiDrawText("MiMITA", cx - 88.0f, cy - 160.0f, 1.25f, {0.95f, 0.98f, 1.0f, 1.0f});
    uiDrawText("\"movement is more important than aim\"", cx - 205.0f, cy - 70.0f, 0.46f, {0.72f, 0.82f, 0.9f, 1.0f});

    if (uiButton(win, "PLAY", {cx - 125.0f, cy + 5.0f, 250.0f, 58.0f}, {0.24f,0.82f,0.48f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Play pressed\n");
        r.goPlay = true;
    }

    if (uiButton(win, "SETTINGS", {cx - 125.0f, cy + 82.0f, 250.0f, 58.0f}, {0.86f,0.74f,0.28f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Settings pressed\n");
        r.goSettings = true;
    }

    return r;
}
