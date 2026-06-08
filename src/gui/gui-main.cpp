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
#include "ui-system.h"
#include <cstdio>

enum GuiMenuState
{
    GUI_MENU_MAIN,
    GUI_MENU_SETTINGS,
    GUI_MENU_SERVERS,
    GUI_MENU_DUEL_CONFIG
};

static GuiMenuState gGuiMenuState = GUI_MENU_MAIN;

static DuelConfigResult gPendingDuelConfig{};

DuelConfigResult getPendingDuelConfig() { return gPendingDuelConfig; }
void clearPendingDuelConfig() { gPendingDuelConfig = DuelConfigResult{}; }

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
            if (r.startSandbox)
                state = GAME_PLAYING;
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
                gGuiMenuState = GUI_MENU_SERVERS;
            break;
        }
    }

    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();
}
