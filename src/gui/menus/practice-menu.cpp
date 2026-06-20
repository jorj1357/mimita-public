#include "practice-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../ui-system.h"

PracticeMenuResult drawPracticeMenu(GLFWwindow* win)
{
    PracticeMenuResult r{};

    float fbW = uiScreenW(), fbH = uiScreenH();

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/practice-menu.json");

    uiDrawRect({0, 0, fbW, fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "practice-menu-bg");

    // Separator line
    uiDrawRect({uiScaleX(760.0f), uiScaleY(104.0f), uiScaleX(400.0f), uiScaleY(2.0f)},
               {0.3f, 0.4f, 0.5f, 0.6f}, "practice-menu-separator");

    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "sandboxButton")
        {
            r.goSandbox = true;
        }
        else if (id == "backButton")
        {
            r.goBack = true;
        }
    }

    return r;
}
