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
#include "../gui-button.h"
#include "../gui-label.h"
#include <cstdio>

MainMenuResult drawMainMenu(GLFWwindow* win)
{
    printf("[MAIN MENU] begin\n");

    MainMenuResult r{};

    guiLabel("Mimita", 820, 220);

    if (guiButton(win, "Play", 800, 400, 320, 80, {0.3f,0.8f,0.4f,1.0f}))
    {
        printf("[MAIN MENU] Play pressed\n");
        r.goPlay = true;
    }

    if (guiButton(win, "Settings", 800, 520, 320, 80, {0.8f,0.8f,0.2f,1.0f}))
    {
        printf("[MAIN MENU] Settings pressed\n");
        r.goSettings = true;
    }

    printf("[MAIN MENU] end\n");
    return r;
}