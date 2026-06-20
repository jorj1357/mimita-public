#include "play-menu.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../ui-system.h"
#include <cstdio>

PlayMenuResult drawPlayMenu(GLFWwindow* win)
{
    PlayMenuResult r{};

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/play-menu.json");

    float fbW = uiScreenW(), fbH = uiScreenH();
    uiDrawRect({0, 0, fbW, fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "play-menu-bg");

    // Separator line
    uiDrawRect({uiScaleX(760.0f), uiScaleY(104.0f), uiScaleX(400.0f), uiScaleY(2.0f)},
               {0.3f, 0.4f, 0.5f, 0.6f}, "play-menu-separator");

    // Render all layout elements
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "duelsButton")
        {
            printf("[PLAY MENU] Duels\n");
            r.goDuels = true;
        }
        else if (id == "bombTagButton")
        {
            printf("[PLAY MENU] Bomb Tag\n");
            r.goBombTag = true;
        }
        else if (id == "onlineButton")
        {
            printf("[PLAY MENU] Online\n");
            r.goOnline = true;
        }
        else if (id == "practiceButton")
        {
            printf("[PLAY MENU] Practice\n");
            r.goPractice = true;
        }
        else if (id == "backButton")
        {
            r.goBack = true;
        }
    }

    return r;
}
