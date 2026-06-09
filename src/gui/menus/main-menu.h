// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\main-menu.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawMainMenu(args)
 *
 * this file DOES:
 * - draw main menu only
 *
 * this file DOES NOT:
 * - draw play submenu
 * - draw settings submenu
 */

#pragma once
#include <GLFW/glfw3.h>

struct MainMenuResult
{
    bool goPlay = false;
    bool goSettings = false;
    bool goSignIn = false;
    bool startSandbox = false;
    bool goHelp = false;
};

MainMenuResult drawMainMenu(GLFWwindow* win);
