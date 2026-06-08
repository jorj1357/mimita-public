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
#include <string>

struct PlayMenuResult
{
    bool startSandbox = false;
    bool startTimeTrials = false;
    bool startPractice = false;
    bool startDuel = false;
    bool startServer = false;
    bool stopServer = false;
    bool connectToServer = false;
    std::string connectAddress;
    bool goBack = false;
};

PlayMenuResult drawPlayMenu(GLFWwindow* win);
void playMenuSetActive(bool active);
void playMenuHandleChar(unsigned int codepoint);
void playMenuHandleKey(int key, int action);
