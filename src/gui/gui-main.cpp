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
#include <cstdio>

enum GuiMenuState
{
    GUI_MENU_MAIN,
    GUI_MENU_PLAY,
    GUI_MENU_SETTINGS,
    GUI_MENU_DEBUG
};

static GuiMenuState gGuiMenuState = GUI_MENU_MAIN;

void guiMain(GLFWwindow* win, GameState& state)
{
    printf("[GUI MAIN] begin\n");
    printf("[GUI MAIN] menu-state=%d game-state=%d\n", (int)gGuiMenuState, (int)state);

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
            break;
        }

        case GUI_MENU_PLAY:
        {
            PlayMenuResult r = drawPlayMenu(win);

            if (r.startSandbox)
            {
                printf("[GUI MAIN] start sandbox -> GAME_PLAYING\n");
                state = GAME_PLAYING;
            }
            else if (r.startTimeTrials)
            {
                printf("[GUI MAIN] start time trials -> GAME_PLAYING\n");
                state = GAME_PLAYING;
            }
            else if (r.startPractice)
            {
                printf("[GUI MAIN] start practice -> GAME_PLAYING\n");
                state = GAME_PLAYING;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_SETTINGS:
        {
            SettingsMenuResult r = drawSettingsMenu(win);

            if (r.goDebug)
            {
                gGuiMenuState = GUI_MENU_DEBUG;
            }
            else if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_MAIN;
            }
            break;
        }

        case GUI_MENU_DEBUG:
        {
            DebugMenuResult r = drawDebugMenu(win);

            if (r.toggleDebugMovement)
            {
                printf("[GUI MAIN] TODO toggle debug movement\n");
            }

            if (r.toggleDebugVisuals)
            {
                printf("[GUI MAIN] TODO toggle debug visuals\n");
            }

            if (r.goBack)
            {
                gGuiMenuState = GUI_MENU_SETTINGS;
            }
            break;
        }
    }

    printf("[GUI MAIN] end\n");
}