// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\play-menu.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawPlayMenu(args)
 *
 * this file DOES:
 * - draw sandbox / time trials / practice / duel / back
 *
 * this file DOES NOT:
 * - mutate global state directly
 */

#include "play-menu.h"
#include "../gui-button.h"
#include "../gui-back.h"
#include "../gui-label.h"
#include <cstdio>

PlayMenuResult drawPlayMenu(GLFWwindow* win)
{
    PlayMenuResult r{};

    guiLabel("Community Servers", 720, 180);

    if (guiButton(win, "Dummy Server: localhost", 760, 300, 400, 70, {0.2f,0.7f,1.0f,1.0f}))
    {
        printf("[PLAY MENU] Dummy server selected\n");
        r.startSandbox = true;
    }

    if (guiButton(win, "Duel Mode", 760, 390, 400, 70, {0.9f,0.3f,0.1f,1.0f}))
    {
        printf("[PLAY MENU] Duel Mode selected\n");
        r.startDuel = true;
    }

    if (guiBackButton(win))
    {
        printf("[PLAY MENU] Back pressed\n");
        r.goBack = true;
    }

    return r;
}
