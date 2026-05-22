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
    printf("[PLAY MENU] begin\n");

    PlayMenuResult r{};

    guiLabel("Play", 860, 220);

    if (guiButton(win, "Sandbox", 800, 360, 320, 70, {0.2f,0.7f,1.0f,1.0f}))
    {
        printf("[PLAY MENU] Sandbox pressed\n");
        r.startSandbox = true;
    }

    if (guiButton(win, "Time Trials", 800, 450, 320, 70, {0.7f,0.5f,1.0f,1.0f}))
    {
        printf("[PLAY MENU] Time Trials pressed\n");
        r.startTimeTrials = true;
    }

    if (guiButton(win, "Practice", 800, 540, 320, 70, {0.7f,0.7f,0.7f,1.0f}))
    {
        printf("[PLAY MENU] Practice pressed\n");
        r.startPractice = true;
    }

    if (guiBackButton(win))
    {
        printf("[PLAY MENU] Back pressed\n");
        r.goBack = true;
    }

    printf("[PLAY MENU] end\n");
    return r;
}