#include "duel-config-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-coord.h"
#include "../ui-system.h"
#include <cstdio>
#include <algorithm>
#include <functional>

static int sNumNpcs = 3;
static int sKillsToWin = 5;
static int sDuelLength = 300;
static float sNpcDifficulty = 5.0f;

// Render elements in consistent Z-order: panels first, then text, then buttons.
// This prevents opaque backgrounds from covering interactive elements when
// unordered_map iteration order places them after buttons.
static void renderElementsInOrder(GLFWwindow* win, GuiLayout& layout,
                                  std::function<void(const std::string&)> onButtonClick)
{
    // Pass 1: panels (background)
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;
        if (elem->type == "panel")
            drawGuiElement(win, *elem);
    }

    // Pass 2: text and labels
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;
        if (elem->type == "text" || elem->type == "label")
            drawGuiElement(win, *elem);
    }

    // Pass 3: buttons (clickable)
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;
        if (elem->type == "button" || elem->type == "checkbox")
        {
            UIButtonState s = drawGuiElement(win, *elem);
            if (s.clicked && onButtonClick)
                onButtonClick(id);
        }
    }
}

DuelConfigResult drawDuelConfigMenu(GLFWwindow* win)
{
    DuelConfigResult r{};
    r.numNpcs = sNumNpcs;
    r.killsToWin = sKillsToWin;
    r.duelLengthSeconds = sDuelLength;
    r.npcDifficulty = sNpcDifficulty;

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/duel-config-menu.json");

    // Render all elements in correct Z-order
    renderElementsInOrder(win, layout, [&](const std::string& id) {
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
    });

    // Dynamic value labels (use layout positions + dynamic text)
    auto drawValue = [&](const char* id, const char* fmt, auto val) {
        const GuiElement* e = layout.get(id);
        if (!e) return;
        char text[128];
        snprintf(text, sizeof(text), fmt, val);
        uiDrawText(text, cs.designToScreenX(e->x + e->w + 20),
                   cs.designToScreenY(e->y), 0.38f, {1, 1, 1, 1});
    };
    drawValue("npcCountLabel", "%d", sNumNpcs);
    drawValue("killsLabel", "%d", sKillsToWin);
    drawValue("timeLabel", "%d", sDuelLength);
    drawValue("diffLabel", "%.0f/10", sNpcDifficulty);

    return r;
}
