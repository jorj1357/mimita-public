// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\debug-menu.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawDebugMenu(args)
 *
 * this file DOES:
 * - emit debug menu button presses
 *
 * this file DOES NOT:
 * - apply actual debug flags yet
 */

#include "debug-menu.h"
#include "../gui-button.h"
#include "../gui-back.h"
#include "../gui-label.h"
#include <cstdio>

DebugMenuResult drawDebugMenu(GLFWwindow* win)
{
    printf("[DEBUG MENU] begin\n");

    DebugMenuResult r{};

    guiLabel("Debug", 860, 220);

    if (guiButton(win, "Toggle Debug Movement", 760, 360, 400, 70, {0.7f,0.3f,0.9f,1.0f}))
    {
        printf("[DEBUG MENU] Toggle Debug Movement pressed\n");
        r.toggleDebugMovement = true;
    }

    if (guiButton(win, "Toggle Debug Visuals", 760, 450, 400, 70, {0.3f,0.9f,0.9f,1.0f}))
    {
        printf("[DEBUG MENU] Toggle Debug Visuals pressed\n");
        r.toggleDebugVisuals = true;
    }

    if (guiBackButton(win))
    {
        printf("[DEBUG MENU] Back pressed\n");
        r.goBack = true;
    }

    printf("[DEBUG MENU] end\n");
    return r;
}