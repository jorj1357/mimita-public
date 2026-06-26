#include "main-menu.h"
#include "account-panel.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "auth/auth-system.h"

#include <cstdio>
#include <filesystem>
#include <shellapi.h>

MainMenuResult drawMainMenu(GLFWwindow* win)
{
    MainMenuResult r{};

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

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

    // Left-side layout buttons
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        UIButtonState s = drawGuiElement(win, *elem);
        if (!s.clicked) continue;

        if (id == "playButton")
        {
            printf("[MAIN MENU] Play pressed\n");
            r.goPlay = true;
        }
        else if (id == "settingsButton")
        {
            printf("[MAIN MENU] Settings pressed\n");
            r.goSettings = true;
        }
        else if (id == "replaysButton")
        {
            printf("[MAIN MENU] Replays pressed\n");
            r.goReplays = true;
        }
        else if (id == "avatarButton")
        {
            printf("[MAIN MENU] Avatar Creator pressed\n");
            r.goAvatarCreator = true;
        }
        else if (id == "exitButton")
        {
            printf("[MAIN MENU] Exit pressed\n");
            r.goExit = true;
        }
    }

    // Right-side account panel
    AccountPanelAction account = drawAccountPanel(win);
    if (account.logIn)
    {
        printf("[MAIN MENU] opening https://www.mimita.fun/login\n");
        HINSTANCE h = ShellExecuteA(nullptr, "open",
            "https://www.mimita.fun/login",
            nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)h <= 32)
            printf("[MAIN MENU] ShellExecuteA failed: result=%lld\n", (long long)(INT_PTR)h);
        AuthSystem::instance().startLinkFlow();
    }
    else if (account.signUp)
    {
        printf("[MAIN MENU] opening https://www.mimita.fun/signup\n");
        HINSTANCE h = ShellExecuteA(nullptr, "open",
            "https://www.mimita.fun/signup",
            nullptr, nullptr, SW_SHOWNORMAL);
        if ((INT_PTR)h <= 32)
            printf("[MAIN MENU] ShellExecuteA failed: result=%lld\n", (long long)(INT_PTR)h);
        AuthSystem::instance().startLinkFlow();
    }
    else if (account.continueOffline)
    {
        printf("[MAIN MENU] Continue Offline\n");
        AuthSystem::instance().skipLogin();
    }

    return r;
}
