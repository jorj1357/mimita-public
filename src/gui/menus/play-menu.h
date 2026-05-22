// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\play-menu.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawPlayMenu(args)
 *
 * this file DOES:
 * - draw play submenu only
 *
 * this file DOES NOT:
 * - draw settings/debug
 */

#pragma once
#include <GLFW/glfw3.h>

struct PlayMenuResult
{
    bool startSandbox = false;
    bool startTimeTrials = false;
    bool startPractice = false;
    bool goBack = false;
};

PlayMenuResult drawPlayMenu(GLFWwindow* win);