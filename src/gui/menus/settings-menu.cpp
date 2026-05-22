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
#include "../gui-button.h"
#include "../gui-back.h"
#include "../gui-label.h"
#include <cstdio>

SettingsMenuResult drawSettingsMenu(GLFWwindow* win)
{
    printf("[SETTINGS MENU] begin\n");

    SettingsMenuResult r{};

    guiLabel("Settings", 830, 220);
    guiLabel("WASD", 820, 330);
    guiLabel("Jump", 820, 380);
    guiLabel("Dash", 820, 430);
    guiLabel("Ground Return", 820, 480);

    if (guiButton(win, "Debug", 800, 580, 320, 70, {0.9f,0.5f,0.2f,1.0f}))
    {
        printf("[SETTINGS MENU] Debug pressed\n");
        r.goDebug = true;
    }

    if (guiBackButton(win))
    {
        printf("[SETTINGS MENU] Back pressed\n");
        r.goBack = true;
    }

    printf("[SETTINGS MENU] end\n");
    return r;
}