#include "play-menu.h"
#include "../gui-back.h"
#include "../gui-layout.h"
#include "../ui-system.h"

PlayMenuResult drawPlayMenu(GLFWwindow* win)
{
    PlayMenuResult r{};

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/play-menu.json");

    float fbW = uiScreenW(), fbH = uiScreenH();
    uiDrawRect({0, 0, fbW, fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "play-menu-bg");

    // Breadcrumb header (text positions use uiScaleX/Y to convert from design→fb)
    uiDrawText("PLAY", uiScaleX(918.0f), uiScaleY(60.0f), 0.72f, {0.55f, 0.78f, 1.0f, 1.0f});
    uiDrawRect({uiScaleX(760.0f), uiScaleY(104.0f), uiScaleX(400.0f), uiScaleY(2.0f)},
               {0.3f, 0.4f, 0.5f, 0.6f}, "play-menu-separator");

    // DUELS - largest, most prominent button (design coordinates)
    if (uiButton(win, "DUELS",
        layout.getRectDesign("DUELS", {800.0f, 430.0f, 320.0f, 58.0f}),
        {0.9f, 0.28f, 0.12f, 1.0f}).clicked)
    {
        r.goDuels = true;
    }
    uiDrawText("Fast 1v1 and PvE combat.", uiScaleX(825.0f), uiScaleY(498.0f), 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    // BOMB TAG
    if (uiButton(win, "BOMB TAG",
        layout.getRectDesign("BOMB TAG", {800.0f, 545.0f, 320.0f, 58.0f}),
        {0.9f, 0.5f, 0.1f, 1.0f}).clicked)
    {
        r.goBombTag = true;
    }
    uiDrawText("Hot potato with explosions.", uiScaleX(825.0f), uiScaleY(613.0f), 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    // ONLINE / COMMUNITY SERVERS
    if (uiButton(win, "ONLINE / COMMUNITY SERVERS",
        layout.getRectDesign("ONLINE / COMMUNITY SERVERS", {800.0f, 660.0f, 320.0f, 58.0f}),
        {0.22f, 0.5f, 0.78f, 1.0f}).clicked)
    {
        r.goOnline = true;
    }
    uiDrawText("Play with other people.", uiScaleX(825.0f), uiScaleY(728.0f), 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    // PRACTICE
    if (uiButton(win, "PRACTICE",
        layout.getRectDesign("PRACTICE", {800.0f, 768.0f, 320.0f, 58.0f}),
        {0.25f, 0.65f, 0.45f, 1.0f}).clicked)
    {
        r.goPractice = true;
    }
    uiDrawText("Sandbox and training modes.", uiScaleX(825.0f), uiScaleY(836.0f), 0.32f,
               {0.72f, 0.78f, 0.88f, 1.0f});

    if (guiBackButton(win, layout.getRectDesign("backButton", {40.0f, 40.0f, 120.0f, 50.0f})))
        r.goBack = true;

    return r;
}
