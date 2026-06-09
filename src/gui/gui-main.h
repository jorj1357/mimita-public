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
#include <string>

struct DuelConfigResult;

void guiMain(GLFWwindow* win, GameState& state);
DuelConfigResult getPendingDuelConfig();
void clearPendingDuelConfig();

struct SandboxMapSelection
{
    bool shouldStart = false;
    std::string mapPath;
};
SandboxMapSelection getPendingSandboxMapSelection();
void clearPendingSandboxMapSelection();
void reportSandboxMapLoadResult(const std::string& message, bool success);

struct MultiplayerConnectInfo
{
    bool shouldConnect = false;
    std::string address = "127.0.0.1:1357";
};
MultiplayerConnectInfo getPendingMultiplayerConnect();
void clearPendingMultiplayerConnect();
