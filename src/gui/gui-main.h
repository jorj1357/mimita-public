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

enum GuiMenuState
{
    GUI_MENU_MAIN,
    GUI_MENU_SETTINGS,
    GUI_MENU_SANDBOX_MAPS,
    GUI_MENU_SERVERS,
    GUI_MENU_DUEL_CONFIG,
    GUI_MENU_SERVER_INFO,
    GUI_MENU_SIGN_IN,
    GUI_MENU_HELP,
    GUI_MENU_PLAY,
    GUI_MENU_PRACTICE,
    GUI_MENU_REPLAY,
    GUI_MENU_AVATAR_CREATOR,
    GUI_MENU_BOMB_TAG_CONFIG,
    GUI_MENU_COMPETITIVE,
    GUI_MENU_COMPETITIVE_RESULT,
    GUI_MENU_AUTH,
    GUI_MENU_LOGIN,
};

extern GuiMenuState gGuiMenuState;

void guiMain(GLFWwindow* win, GameState& state);
DuelConfigResult getPendingDuelConfig();
void clearPendingDuelConfig();

#include "game/bomb-tag-config.h"
extern BombTagConfigResult gPendingBombTagConfig;
BombTagConfigResult getPendingBombTagConfig();
void clearPendingBombTagConfig();

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
    std::string joinToken;
    uint16_t port = 1357;
    std::string roomCode;
};
MultiplayerConnectInfo getPendingMultiplayerConnect();
void clearPendingMultiplayerConnect();

struct ExternalServerProcessStatus
{
    bool launched = false;
    bool running = false;
    uint32_t processId = 0;
    uint16_t port = 1357;
    uint64_t uptimeMs = 0;
    std::string roomCode;
    std::string serverName;
    std::string mapName;
};

ExternalServerProcessStatus getExternalServerProcessStatus();

namespace MimitaNet {
struct ListenServerState;
}
MimitaNet::ListenServerState* getListenServerState();
