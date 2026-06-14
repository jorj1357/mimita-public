#include "practice-menu.h"
#include "../gui-back.h"
#include "../gui-layout.h"
#include "../ui-system.h"

PracticeMenuResult drawPracticeMenu(GLFWwindow* win)
{
    PracticeMenuResult r{};

    float fbW = uiScreenW(), fbH = uiScreenH();

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/practice-menu.json");

    uiDrawRect({0, 0, fbW, fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "practice-menu-bg");

    uiDrawText("PLAY  >  PRACTICE", uiScaleX(820.0f), uiScaleY(60.0f), 0.50f,
               {0.55f, 0.78f, 1.0f, 1.0f});
    uiDrawRect({uiScaleX(760.0f), uiScaleY(104.0f), uiScaleX(400.0f), uiScaleY(2.0f)},
               {0.3f, 0.4f, 0.5f, 0.6f}, "practice-menu-separator");

    // SANDBOX
    if (uiButton(win, "SANDBOX",
        layout.getRectDesign("SANDBOX", {835.0f, 511.0f, 250.0f, 58.0f}),
        {0.25f, 0.55f, 0.95f, 1.0f}).clicked)
    {
        r.goSandbox = true;
    }
    uiDrawText("Free play on any map.", uiScaleX(860.0f), uiScaleY(579.0f), 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    if (guiBackButton(win, layout.getRectDesign("backButton", {40.0f, 40.0f, 120.0f, 50.0f})))
        r.goBack = true;

    return r;
}
