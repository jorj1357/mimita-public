// C:\important\quiet\n\mimita-priv-v7\src\gui\gui-main.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * guiMain(args)
 *
 * this file DOES:
 * - route between main/play/settings/debug menus
 *
 * this file DOES NOT:
 * - draw raw gui primitives itself
 */

#include "gui-main.h"
#include "menus/main-menu.h"
#include "menus/play-menu.h"
#include "menus/settings-menu.h"
#include "menus/debug-menu.h"
#include "menus/duel-config-menu.h"
#include "menus/server-info-menu.h"
#include "menus/sign-in-menu.h"
#include "ui-system.h"
#include <cstdio>

enum GuiMenuState
{
    GUI_MENU_MAIN,
    GUI_MENU_SETTINGS,
    GUI_MENU_SERVERS,
    GUI_MENU_DUEL_CONFIG,
    GUI_MENU_SERVER_INFO,
    GUI_MENU_SIGN_IN
};

static GuiMenuState gGuiMenuState = GUI_MENU_MAIN;

static DuelConfigResult gPendingDuelConfig{};
static bool gServerRunning = false;
static char gServerAddress[64] = "127.0.0.1:1357";
static MultiplayerConnectInfo gPendingConnect{};

DuelConfigResult getPendingDuelConfig() { return gPendingDuelConfig; }
void clearPendingDuelConfig() { gPendingDuelConfig = DuelConfigResult{}; }

MultiplayerConnectInfo getPendingMultiplayerConnect() { return gPendingConnect; }
void clearPendingMultiplayerConnect() { gPendingConnect = {}; }

void guiMain(GLFWwindow* win, GameState& state)
{
    uiBeginFrame(win, "menu");

    switch (gGuiMenuState)
    {
        case GUI_MENU_MAIN:
        {
            MainMenuResult r = drawMainMenu(win);

            if (r.goPlay)
            {
                gGuiMenuState = GUI_MENU_SERVERS;
            }
            else if (r.goSettings)
            {
                gGuiMenuState = GUI_MENU_SETTINGS;
            }
            else if (r.goSignIn)
            {
                signInMenuSetActive(true);
                gGuiMenuState = GUI_MENU_SIGN_IN;
            }
            else if (r.startSandbox)
            {
                state = GAME_PLAYING;
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
            PlayMenuResult r = drawPlayMenu(win);
            if (r.startServer || r.joinByIp) {
                serverInfoMenuSetActive(true);
                gGuiMenuState = GUI_MENU_SERVER_INFO;
            }
            else if (r.connectToServer)
            {
                gPendingConnect.shouldConnect = true;
                gPendingConnect.address = gServerAddress;
                state = GAME_PLAYING;
            }
            else if (r.startDuel)
                gGuiMenuState = GUI_MENU_DUEL_CONFIG;
            else if (r.goBack)
                gGuiMenuState = GUI_MENU_MAIN;
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
                serverInfoMenuSetActive(false);
                gGuiMenuState = GUI_MENU_SERVERS;
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
    }

    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();
}
