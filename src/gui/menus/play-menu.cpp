#include "play-menu.h"
#include "../gui-back.h"
#include "../gui-layout.h"
#include "../ui-system.h"

PlayMenuResult drawPlayMenu(GLFWwindow* win)
{
    PlayMenuResult r{};

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/play-menu.json");

    uiDrawRect({0, 0, (float)w, (float)h}, {0.035f, 0.04f, 0.052f, 1.0f}, "play-menu-bg");

    // Breadcrumb header
    uiDrawText("PLAY", cx - 42.0f, 60.0f, 0.72f, {0.55f, 0.78f, 1.0f, 1.0f});
    uiDrawRect({cx - 200.0f, 104.0f, 400.0f, 2.0f},
               {0.3f, 0.4f, 0.5f, 0.6f}, "play-menu-separator");

    // DUELS - largest, most prominent button
    if (uiButton(win, "DUELS",
        layout.getRectCentered("DUELS", {cx - 160.0f, cy - 70.0f, 320.0f, 68.0f}, cx, cy),
        {0.9f, 0.28f, 0.12f, 1.0f}).clicked)
    {
        r.goDuels = true;
    }
    uiDrawText("Fast 1v1 and PvE combat.", cx - 135.0f, cy + 10.0f, 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    // ONLINE / COMMUNITY SERVERS
    if (uiButton(win, "ONLINE / COMMUNITY SERVERS",
        layout.getRectCentered("ONLINE / COMMUNITY SERVERS", {cx - 160.0f, cy + 75.0f, 320.0f, 58.0f}, cx, cy),
        {0.22f, 0.5f, 0.78f, 1.0f}).clicked)
    {
        r.goOnline = true;
    }
    uiDrawText("Play with other people.", cx - 135.0f, cy + 143.0f, 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    // PRACTICE
    if (uiButton(win, "PRACTICE",
        layout.getRectCentered("PRACTICE", {cx - 160.0f, cy + 183.0f, 320.0f, 58.0f}, cx, cy),
        {0.25f, 0.65f, 0.45f, 1.0f}).clicked)
    {
        r.goPractice = true;
    }
    uiDrawText("Sandbox and training modes.", cx - 135.0f, cy + 251.0f, 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    if (guiBackButton(win))
        r.goBack = true;

    return r;
}
