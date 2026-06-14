#include "gui-main.h"
#include "menus/main-menu.h"
#include "menus/play-menu.h"
#include "menus/online-menu.h"
#include "menus/practice-menu.h"
#include "menus/settings-menu.h"
#include "menus/debug-menu.h"
#include "menus/duel-config-menu.h"
#include "menus/server-info-menu.h"
#include "menus/sign-in-menu.h"
#include "menus/sandbox-map-menu.h"
#include "menus/help-menu.h"
#include "ui-system.h"
#include "gui-editor.h"
#include "gui-layout.h"
#include <cstdio>

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
    GUI_MENU_PRACTICE
};

static GuiMenuState gGuiMenuState = GUI_MENU_MAIN;

static const char* layoutFileForMenu(GuiMenuState state)
{
    switch (state) {
        case GUI_MENU_MAIN:         return "config/gui/main-menu.json";
        case GUI_MENU_SERVERS:      return "config/gui/play-menu.json";
        case GUI_MENU_SETTINGS:     return "config/gui/settings-menu.json";
        case GUI_MENU_DUEL_CONFIG:  return "config/gui/duel-config-menu.json";
        case GUI_MENU_SANDBOX_MAPS: return "config/gui/sandbox-map-menu.json";
        case GUI_MENU_SIGN_IN:      return "config/gui/sign-in-menu.json";
        case GUI_MENU_HELP:         return "config/gui/help-menu.json";
        case GUI_MENU_SERVER_INFO:  return "config/gui/server-info-menu.json";
    }
    return "config/gui/main-menu.json";
}

static DuelConfigResult gPendingDuelConfig{};
static bool gServerRunning = false;
static char gServerAddress[64] = "127.0.0.1:1357";
static MultiplayerConnectInfo gPendingConnect{};
static SandboxMapSelection gPendingSandboxMap{};

DuelConfigResult getPendingDuelConfig() { return gPendingDuelConfig; }
void clearPendingDuelConfig() { gPendingDuelConfig = DuelConfigResult{}; }

MultiplayerConnectInfo getPendingMultiplayerConnect() { return gPendingConnect; }
void clearPendingMultiplayerConnect() { gPendingConnect = {}; }
SandboxMapSelection getPendingSandboxMapSelection() { return gPendingSandboxMap; }
void clearPendingSandboxMapSelection() { gPendingSandboxMap = {}; }
void reportSandboxMapLoadResult(const std::string& message, bool success)
{
    sandboxMapMenuSetLoadResult(message, success);
}

void guiMain(GLFWwindow* win, GameState& state)
{
    GuiLayoutManager::instance().pollReload();

    uiBeginFrame(win, "menu");

    // Tell the GUI editor which layout file is active for this menu
    GuiEditor::instance().setActiveLayout(layoutFileForMenu(gGuiMenuState));

    switch (gGuiMenuState)
    {
        case GUI_MENU_MAIN:
        {
            MainMenuResult r = drawMainMenu(win);

            if (r.goPlay)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            else if (r.goSettings)
            {
                gGuiMenuState = GUI_MENU_SETTINGS;
            }
            else if (r.goReplays)
            {
                printf("[MAIN MENU] Replays menu not yet implemented\n");
            }
            else if (r.goExit)
            {
                glfwSetWindowShouldClose(win, GLFW_TRUE);
            }
            break;
        }

        case GUI_MENU_PLAY:
        {
            PlayMenuResult r = drawPlayMenu(win);

            if (r.goDuels)
            {
                gGuiMenuState = GUI_MENU_DUEL_CONFIG;
            }
            else if (r.goOnline)
            {
                onlineMenuSetActive(true);
                gGuiMenuState = GUI_MENU_SERVERS;
            }
            else if (r.goPractice)
            {
                gGuiMenuState = GUI_MENU_PRACTICE;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_PRACTICE:
        {
            PracticeMenuResult r = drawPracticeMenu(win);

            if (r.goSandbox)
            {
                sandboxMapMenuSetActive(true);
                gGuiMenuState = GUI_MENU_SANDBOX_MAPS;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_SANDBOX_MAPS:
        {
            SandboxMapMenuResult r = drawSandboxMapMenu(win);
            if (r.startSandbox)
            {
                gPendingSandboxMap.shouldStart = true;
                gPendingSandboxMap.mapPath = r.mapPath;
                sandboxMapMenuSetActive(false);
                state = GAME_PLAYING;
            }
            else if (r.goBack)
            {
                sandboxMapMenuSetActive(false);
                gGuiMenuState = GUI_MENU_PRACTICE;
            }
            break;
        }

        case GUI_MENU_SETTINGS:
        {
            SettingsMenuResult r = drawSettingsMenu(win);

            if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_SERVERS:
        {
            OnlineMenuResult r = drawOnlineMenu(win);
            if (r.connectToServer)
            {
                gPendingConnect.shouldConnect = true;
                gPendingConnect.address = r.connectAddress;
                onlineMenuSetActive(false);
                state = GAME_PLAYING;
            }
            else if (r.goBack)
            {
                onlineMenuSetActive(false);
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_DUEL_CONFIG:
        {
            DuelConfigResult r = drawDuelConfigMenu(win);
            if (r.startDuel) {
                gPendingDuelConfig = r;
                state = GAME_PLAYING;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_PLAY;
            }
            break;
        }

        case GUI_MENU_SERVER_INFO:
        {
            ServerInfoResult r = drawServerInfoMenu(win, gServerAddress, gServerRunning);
            if (r.startServer)
                gServerRunning = true;
            if (r.connect)
            {
                serverInfoMenuSetActive(false);
                gPendingConnect.shouldConnect = true;
                gPendingConnect.address = gServerAddress;
                state = GAME_PLAYING;
            }
            else if (r.goBack)
                gGuiMenuState = GUI_MENU_SERVERS;
            break;
        }

        case GUI_MENU_SIGN_IN:
        {
            SignInMenuResult r = drawSignInMenu(win);
            if (r.signedIn || r.goBack)
            {
                signInMenuSetActive(false);
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_HELP:
        {
            HelpMenuResult r = drawHelpMenu(win);
            if (r.goBack)
                gGuiMenuState = GUI_MENU_MAIN;
            break;
        }
    }

    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();

    GuiEditor::instance().update(win);
}
