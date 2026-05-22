// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-main.h
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * guiMain(args)
 *
 * this file DOES:
 * - orchestrate gui menu flow
 *
 * this file DOES NOT:
 * - implement button drawing
 * - implement individual menus
 */

#pragma once
#include <GLFW/glfw3.h>
#include "game/game-state.h"

void guiMain(GLFWwindow* win, GameState& state);