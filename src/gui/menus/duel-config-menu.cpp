#include "duel-config-menu.h"
#include "../gui-button.h"
#include "../gui-back.h"
#include "../gui-label.h"
#include "../ui-system.h"
#include <cstdio>
#include <algorithm>

static int sNumNpcs = 3;
static int sKillsToWin = 5;
static int sDuelLength = 300;
static float sNpcDifficulty = 5.0f;

static void drawIntSlider(GLFWwindow* win, const char* label, float x, float y,
                           int& value, int minVal, int maxVal)
{
    char text[128];
    snprintf(text, sizeof(text), "%s: %d", label, value);
    uiDrawText(text, x, y, 0.38f, {1, 1, 1, 1});

    float btnW = 50.0f;
    float btnH = 40.0f;
    if (guiButton(win, "-", x + 350, y - 5, btnW, btnH, {0.5f, 0.15f, 0.15f, 1.0f}))
        value = std::max(minVal, value - 1);
    if (guiButton(win, "+", x + 410, y - 5, btnW, btnH, {0.15f, 0.5f, 0.15f, 1.0f}))
        value = std::min(maxVal, value + 1);
}

DuelConfigResult drawDuelConfigMenu(GLFWwindow* win)
{
    DuelConfigResult r{};
    r.numNpcs = sNumNpcs;
    r.killsToWin = sKillsToWin;
    r.duelLengthSeconds = sDuelLength;
    r.npcDifficulty = sNpcDifficulty;

    guiLabel("Duel Mode Settings", 720, 160);

    drawIntSlider(win, "NPC Count", 720, 260, sNumNpcs, 1, 20);
    drawIntSlider(win, "Kills to Win", 720, 320, sKillsToWin, 1, 99);
    drawIntSlider(win, "Time Limit (sec)", 720, 380, sDuelLength, 30, 3600);

    {
        char diffText[64];
        snprintf(diffText, sizeof(diffText), "Difficulty: %.0f/10", sNpcDifficulty);
        uiDrawText(diffText, 720, 440, 0.38f, {1, 1, 1, 1});
        float btnW = 50.0f, btnH = 40.0f;
        if (guiButton(win, "-", 1070, 435, btnW, btnH, {0.5f, 0.15f, 0.15f, 1.0f}))
            sNpcDifficulty = std::max(1.0f, sNpcDifficulty - 1.0f);
        if (guiButton(win, "+", 1130, 435, btnW, btnH, {0.15f, 0.5f, 0.15f, 1.0f}))
            sNpcDifficulty = std::min(10.0f, sNpcDifficulty + 1.0f);
    }

    if (guiButton(win, "START DUEL", 820, 540, 300, 80, {0.9f, 0.25f, 0.1f, 1.0f}))
    {
        r.startDuel = true;
    }

    if (guiBackButton(win))
    {
        r.goBack = true;
    }

    return r;
}
