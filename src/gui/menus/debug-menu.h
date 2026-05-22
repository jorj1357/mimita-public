// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\debug-menu.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawDebugMenu(args)
 *
 * this file DOES:
 * - draw debug submenu only
 *
 * this file DOES NOT:
 * - toggle real engine debug values yet
 */

#pragma once
#include <GLFW/glfw3.h>

struct DebugMenuResult
{
    bool toggleDebugMovement = false;
    bool toggleDebugVisuals = false;
    bool goBack = false;
};

DebugMenuResult drawDebugMenu(GLFWwindow* win);