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
#include "../gui-back.h"
#include "../gui-button.h"
#include "../gui-label.h"
#include "../gui-layout.h"
#include "../ui-system.h"
#include <cstdio>

DebugMenuResult drawDebugMenu(GLFWwindow* win)
{
    printf("[DEBUG MENU] begin\n");

    DebugMenuResult r{};

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/debug-menu.json");

    guiLabel("Debug", 860, 220);

    {
        UIRect mr = layout.getRect("Toggle Debug Movement", {760.0f, 360.0f, 400.0f, 70.0f});
        if (guiButton(win, "Toggle Debug Movement", mr.x, mr.y, mr.w, mr.h, {0.7f,0.3f,0.9f,1.0f}))
        {
            printf("[DEBUG MENU] Toggle Debug Movement pressed\n");
            r.toggleDebugMovement = true;
        }
    }

    {
        UIRect vr = layout.getRect("Toggle Debug Visuals", {760.0f, 450.0f, 400.0f, 70.0f});
        if (guiButton(win, "Toggle Debug Visuals", vr.x, vr.y, vr.w, vr.h, {0.3f,0.9f,0.9f,1.0f}))
        {
            printf("[DEBUG MENU] Toggle Debug Visuals pressed\n");
            r.toggleDebugVisuals = true;
        }
    }

    if (guiBackButton(win, layout.getRect("backButton", {40.0f, 40.0f, 120.0f, 50.0f})))
    {
        printf("[DEBUG MENU] Back pressed\n");
        r.goBack = true;
    }

    printf("[DEBUG MENU] end\n");
    return r;
}