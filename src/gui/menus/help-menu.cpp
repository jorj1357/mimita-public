// 08 31 2026, 19 35
/* purpose
* Draws the JSON-defined help screen shared by the main and pause menus.
* Keeps help text scrollable and hot-reloadable through GuiLayoutManager.
* Routes only the back action; command behavior belongs to terminal systems.
* Does NOT own command registration or gameplay/network state.
* Does NOT duplicate help content in C++.
*/

#include "help-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../ui-system.h"
#include "../gui-coord.h"
#include <cstdio>

namespace {
UIScrollState gHelpScroll;
}

HelpMenuResult drawHelpMenu(GLFWwindow* win)
{
    HelpMenuResult r{};
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/help-menu.json");

    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        if (id == "helpBg" || id == "helpTitle" || id == "backButton") {
            UIButtonState s = drawGuiElement(win, *elem);
            if (s.clicked && id == "backButton") r.goBack = true;
        }
    }

    const UIRect scrollArea = {560.0f, 110.0f, 820.0f, 900.0f};
    uiBeginScrollArea(win, scrollArea, 1250.0f, gHelpScroll);
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible || id == "helpBg" || id == "helpTitle" || id == "backButton") continue;

        if (elem->type == "text" || elem->type == "label")
        {
            drawGuiElement(win, *elem);
            continue;
        }

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "backButton")
            r.goBack = true;
    }
    uiEndScrollArea(scrollArea, 1250.0f, gHelpScroll);

    return r;
}
