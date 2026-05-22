// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\settings-menu.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawSettingsMenu(args)
 *
 * this file DOES:
 * - draw settings submenu only
 *
 * this file DOES NOT:
 * - apply settings yet
 */

#pragma once
#include <GLFW/glfw3.h>

struct SettingsMenuResult
{
    bool goDebug = false;
    bool goBack = false;
};

SettingsMenuResult drawSettingsMenu(GLFWwindow* win);