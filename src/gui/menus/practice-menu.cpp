#include "practice-menu.h"
#include "../gui-back.h"
#include "../gui-layout.h"
#include "../ui-system.h"

PracticeMenuResult drawPracticeMenu(GLFWwindow* win)
{
    PracticeMenuResult r{};

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/practice-menu.json");

    uiDrawRect({0, 0, (float)w, (float)h}, {0.035f, 0.04f, 0.052f, 1.0f}, "practice-menu-bg");

    // Breadcrumb header
    uiDrawText("PLAY  >  PRACTICE", cx - 140.0f, 60.0f, 0.50f,
               {0.55f, 0.78f, 1.0f, 1.0f});
    uiDrawRect({cx - 200.0f, 104.0f, 400.0f, 2.0f},
               {0.3f, 0.4f, 0.5f, 0.6f}, "practice-menu-separator");

    // SANDBOX
    if (uiButton(win, "SANDBOX",
        layout.getRectCentered("SANDBOX", {cx - 125.0f, cy - 29.0f, 250.0f, 58.0f}, cx, cy),
        {0.25f, 0.55f, 0.95f, 1.0f}).clicked)
    {
        r.goSandbox = true;
    }
    uiDrawText("Free play on any map.", cx - 100.0f, cy + 39.0f, 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    // Future: Training Range, NPC Practice

    if (guiBackButton(win, layout.getRect("backButton", {40.0f, 40.0f, 120.0f, 50.0f})))
        r.goBack = true;

    return r;
}
