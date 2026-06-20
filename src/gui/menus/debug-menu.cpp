#include "debug-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../ui-system.h"
#include <cstdio>

DebugMenuResult drawDebugMenu(GLFWwindow* win)
{
    printf("[DEBUG MENU] begin\n");
    DebugMenuResult r{};

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/debug-menu.json");

    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        printf("[DEBUG MENU] clicked: %s\n", id.c_str());

        if (id == "toggleMovementButton")
        {
            r.toggleDebugMovement = true;
        }
        else if (id == "toggleVisualsButton")
        {
            r.toggleDebugVisuals = true;
        }
        else if (id == "backButton")
        {
            r.goBack = true;
        }
    }

    printf("[DEBUG MENU] end\n");
    return r;
}
