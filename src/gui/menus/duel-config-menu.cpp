#include "duel-config-menu.h"
#include "../gui-back.h"
#include "../gui-button.h"
#include "../gui-label.h"
#include "../gui-layout.h"
#include "../ui-system.h"
#include <cstdio>
#include <algorithm>

static int sNumNpcs = 3;
static int sKillsToWin = 5;
static int sDuelLength = 300;
static float sNpcDifficulty = 5.0f;

DuelConfigResult drawDuelConfigMenu(GLFWwindow* win)
{
    DuelConfigResult r{};
    r.numNpcs = sNumNpcs;
    r.killsToWin = sKillsToWin;
    r.duelLengthSeconds = sDuelLength;
    r.npcDifficulty = sNpcDifficulty;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/duel-config-menu.json");

    guiLabel("Duel Mode Settings", uiScaleX(720.0f), uiScaleY(160.0f));

    // NPC Count
    {
        char text[128];
        snprintf(text, sizeof(text), "NPC Count: %d", sNumNpcs);
        uiDrawText(text, uiScaleX(720.0f), uiScaleY(260.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("NPC Count -", {1070.0f, 255.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f}))
            sNumNpcs = std::max(1, sNumNpcs - 1);
        UIRect pr = layout.getRectDesign("NPC Count +", {1130.0f, 255.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f}))
            sNumNpcs = std::min(20, sNumNpcs + 1);
    }

    // Kills to Win
    {
        char text[128];
        snprintf(text, sizeof(text), "Kills to Win: %d", sKillsToWin);
        uiDrawText(text, uiScaleX(720.0f), uiScaleY(320.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("Kills to Win -", {1070.0f, 315.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f}))
            sKillsToWin = std::max(1, sKillsToWin - 1);
        UIRect pr = layout.getRectDesign("Kills to Win +", {1130.0f, 315.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f}))
            sKillsToWin = std::min(99, sKillsToWin + 1);
    }

    // Time Limit
    {
        char text[128];
        snprintf(text, sizeof(text), "Time Limit (sec): %d", sDuelLength);
        uiDrawText(text, uiScaleX(720.0f), uiScaleY(380.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("Time Limit -", {1070.0f, 375.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f}))
            sDuelLength = std::max(30, sDuelLength - 1);
        UIRect pr = layout.getRectDesign("Time Limit +", {1130.0f, 375.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f}))
            sDuelLength = std::min(3600, sDuelLength + 1);
    }

    // Difficulty
    {
        char diffText[64];
        snprintf(diffText, sizeof(diffText), "Difficulty: %.0f/10", sNpcDifficulty);
        uiDrawText(diffText, uiScaleX(720.0f), uiScaleY(440.0f), 0.38f, {1, 1, 1, 1});
        UIRect mr = layout.getRectDesign("Difficulty -", {1070.0f, 435.0f, 50.0f, 40.0f});
        if (guiButton(win, "-", mr.x, mr.y, mr.w, mr.h, {0.5f, 0.15f, 0.15f, 1.0f}))
            sNpcDifficulty = std::max(1.0f, sNpcDifficulty - 1.0f);
        UIRect pr = layout.getRectDesign("Difficulty +", {1130.0f, 435.0f, 50.0f, 40.0f});
        if (guiButton(win, "+", pr.x, pr.y, pr.w, pr.h, {0.15f, 0.5f, 0.15f, 1.0f}))
            sNpcDifficulty = std::min(10.0f, sNpcDifficulty + 1.0f);
    }

    {
        UIRect sd = layout.getRectDesign("START DUEL", {820.0f, 540.0f, 300.0f, 80.0f});
        if (guiButton(win, "START DUEL", sd.x, sd.y, sd.w, sd.h, {0.9f, 0.25f, 0.1f, 1.0f}))
            r.startDuel = true;
    }

    if (guiBackButton(win, layout.getRectDesign("backButton", {40.0f, 40.0f, 120.0f, 50.0f})))
        r.goBack = true;

    return r;
}
