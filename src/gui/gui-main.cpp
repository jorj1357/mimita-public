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
#include "ui-system.h"
#include <cstdio>

enum GuiMenuState
{
    GUI_MENU_MAIN,
    GUI_MENU_SETTINGS
};

static GuiMenuState gGuiMenuState = GUI_MENU_MAIN;

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
                printf("[GUI MAIN] PLAY -> GAME_PLAYING\n");
                state = GAME_PLAYING;
            }
            else if (r.goSettings)
            {
                gGuiMenuState = GUI_MENU_SETTINGS;
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
    }

    uiRenderFrameDebugOverlay(win, "MENU", false);
    uiEndFrame();
}
