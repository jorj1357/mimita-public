#include "main-menu.h"
#include "account-panel.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-coord.h"
#include "auth/auth-system.h"

#include <cstdio>
#include <filesystem>
#include <shellapi.h>

namespace {

void openBrowser(const char* url)
{
    printf("[MAIN MENU] opening %s\n", url);
    HINSTANCE h = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
        printf("[MAIN MENU] ShellExecuteA failed: result=%lld\n", (long long)(INT_PTR)h);
}

struct MenuButton {
    const char* id;
    const char* text;
    float x, y, w, h;
    glm::vec4 bg;
};

}

MainMenuResult drawMainMenu(GLFWwindow* win)
{
    MainMenuResult r{};

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/main-menu.json");

    const char* bgPath = "assets/ui/backgrounds/main-menu-bg.png";
    bool bgLoaded = false;
    {
        std::string testPath = bgPath;
        size_t dot = testPath.rfind('.');
        if (dot != std::string::npos) {
            std::string base = testPath.substr(0, dot);
            for (const char* ext : {".png", ".jpg", ".jpeg", ".gif", ".mp4"}) {
                std::string full = base + ext;
                if (std::filesystem::exists(full)) {
                    uiDrawMedia(full.c_str(), {(float)0, (float)0, (float)fbW, (float)fbH});
                    bgLoaded = true;
                    break;
                }
            }
        }
    }
    if (!bgLoaded)
        uiDrawRect({0, 0, (float)fbW, (float)fbH}, {0.035f, 0.04f, 0.052f, 1.0f}, "main-menu-background");

    // Cover image at the top
    {
        float coverH = fbH * 0.35f;
        float coverY = 0;
        float coverW = (float)fbW;
        uiDrawImage("assets/uitextures/mimita cover in game v1.png",
                    {0, coverY, coverW, coverH});
    }

    // Render title/subtitle from layout (text elements only)
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;
        if (elem->type == "text" || elem->type == "label")
            drawGuiElement(win, *elem);
    }

    AccountPanelAction account = drawAccountPanel(win);

    // Render buttons AFTER account panel (same timing as Sign In/Sign Up)
    MenuButton buttons[] = {
        {"playButton", "PLAY",    270, 520, 300, 62, {0.24f, 0.82f, 0.48f, 1.0f}},
        {"settingsButton", "SETTINGS", 270, 600, 300, 62, {0.86f, 0.74f, 0.28f, 1.0f}},
        {"replaysButton", "REPLAYS", 270, 680, 300, 62, {0.55f, 0.35f, 0.75f, 1.0f}},
        {"avatarButton", "AVATAR", 270, 760, 300, 62, {0.30f, 0.60f, 0.50f, 1.0f}},
        {"exitButton", "EXIT",    270, 840, 300, 62, {0.65f, 0.20f, 0.20f, 1.0f}},
    };

    for (const MenuButton& btn : buttons)
    {
        UIButtonState s = uiButton(win, btn.text,
            {btn.x, btn.y, btn.w, btn.h}, btn.bg, btn.id);
        if (!s.clicked) continue;

        printf("[MAIN MENU] %s pressed\n", btn.id);
        if (btn.id == std::string("playButton")) r.goPlay = true;
        else if (btn.id == std::string("settingsButton")) r.goSettings = true;
        else if (btn.id == std::string("replaysButton")) r.goReplays = true;
        else if (btn.id == std::string("avatarButton")) r.goAvatarCreator = true;
        else if (btn.id == std::string("exitButton")) r.goExit = true;
    }
    if (account.logIn)
    {
        openBrowser("https://www.mimita.fun/signin");
        AuthSystem::instance().startLinkFlow();
    }
    else if (account.signUp)
    {
        openBrowser("https://www.mimita.fun/signup");
        AuthSystem::instance().startLinkFlow();
    }

    return r;
}
