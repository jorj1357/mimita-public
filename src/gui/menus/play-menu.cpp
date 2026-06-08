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
#include "../ui-system.h"
#include <cstdio>

PlayMenuResult drawPlayMenu(GLFWwindow* win)
{
    PlayMenuResult r{};

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    float cx = w * 0.5f;

    uiDrawRect({0, 0, (float)w, (float)h}, {0.035f, 0.04f, 0.052f, 1.0f}, "play-menu-bg");

    guiLabel("Multiplayer", cx - 80.0f, 140.0f);

    if (guiButton(win, "Start Server", cx - 200.0f, 240.0f, 400.0f, 70.0f, {0.2f,0.8f,0.3f,1.0f}))
    {
        r.startServer = true;
    }

    if (guiButton(win, "Connect to Server", cx - 200.0f, 330.0f, 400.0f, 70.0f, {0.2f,0.7f,1.0f,1.0f}))
    {
        r.connectToServer = true;
    }

    if (guiButton(win, "Duel Mode", cx - 200.0f, 420.0f, 400.0f, 70.0f, {0.9f,0.3f,0.1f,1.0f}))
    {
        r.startDuel = true;
    }

    if (guiBackButton(win))
    {
        r.goBack = true;
    }

    return r;
}
