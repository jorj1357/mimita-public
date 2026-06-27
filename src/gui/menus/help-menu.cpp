#include "help-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../ui-system.h"
#include <cstdio>

HelpMenuResult drawHelpMenu(GLFWwindow* win)
{
    HelpMenuResult r{};
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/help-menu.json");

    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

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

    return r;
}
