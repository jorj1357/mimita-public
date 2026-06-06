// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\play-menu.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawPlayMenu(args)
 *
 * this file DOES:
 * - draw sandbox / time trials / practice / back
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

    guiLabel("Community Servers (placeholder)", 720, 220);

    if (guiButton(win, "Dummy Server: localhost", 760, 360, 400, 70, {0.2f,0.7f,1.0f,1.0f}))
    {
        printf("[PLAY MENU] Dummy server selected\n");
        r.startSandbox = true;
    }

    if (guiBackButton(win))
    {
        printf("[PLAY MENU] Back pressed\n");
        r.goBack = true;
    }

    return r;
}
