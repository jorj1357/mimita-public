#include "duel-config-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
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

    // Render all layout elements
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        // Skip dynamic text labels — rendered manually below
        if (id == "header") continue;

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "npcCountMinus") sNumNpcs = std::max(1, sNumNpcs - 1);
        else if (id == "npcCountPlus") sNumNpcs = std::min(20, sNumNpcs + 1);
        else if (id == "killsMinus") sKillsToWin = std::max(1, sKillsToWin - 1);
        else if (id == "killsPlus") sKillsToWin = std::min(99, sKillsToWin + 1);
        else if (id == "timeMinus") sDuelLength = std::max(30, sDuelLength - 1);
        else if (id == "timePlus") sDuelLength = std::min(3600, sDuelLength + 1);
        else if (id == "diffMinus") sNpcDifficulty = std::max(1.0f, sNpcDifficulty - 1.0f);
        else if (id == "diffPlus") sNpcDifficulty = std::min(10.0f, sNpcDifficulty + 1.0f);
        else if (id == "startDuelButton") r.startDuel = true;
        else if (id == "backButton") r.goBack = true;
    }

    // Static header
    const GuiElement* headerEl = layout.get("header");
    if (headerEl)
        drawGuiElement(win, *headerEl);

    // Dynamic labels
    auto drawLabel = [&](const char* fmt, int val, float y) {
        char text[128];
        snprintf(text, sizeof(text), fmt, val);
        uiDrawText(text, uiScaleX(720.0f), uiScaleY(y), 0.38f, {1, 1, 1, 1});
    };
    drawLabel("NPC Count: %d", sNumNpcs, 260.0f);
    drawLabel("Kills to Win: %d", sKillsToWin, 320.0f);
    drawLabel("Time Limit (sec): %d", sDuelLength, 380.0f);

    {
        char diffText[64];
        snprintf(diffText, sizeof(diffText), "Difficulty: %.0f/10", sNpcDifficulty);
        uiDrawText(diffText, uiScaleX(720.0f), uiScaleY(440.0f), 0.38f, {1, 1, 1, 1});
    }

    return r;
}
