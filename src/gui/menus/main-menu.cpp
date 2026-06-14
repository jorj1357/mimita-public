#include "main-menu.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "profile/local-profile-system.h"
#include <cstdio>
#include <filesystem>

MainMenuResult drawMainMenu(GLFWwindow* win)
{
    MainMenuResult r{};

    int w = 0, h = 0;
    glfwGetFramebufferSize(win, &w, &h);
    float cx = w * 0.5f;
    float cy = h * 0.5f;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/main-menu.json");

    // Try media background (MP4 or GIF), fall back to solid color
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
                    uiDrawMedia(full.c_str(), {(float)0, (float)0, (float)w, (float)h});
                    bgLoaded = true;
                    break;
                }
            }
        }
    }
    if (!bgLoaded)
        uiDrawRect({0, 0, (float)w, (float)h}, {0.035f, 0.04f, 0.052f, 1.0f}, "main-menu-background");
    uiDrawText("MiMITA", cx - 88.0f, cy - 160.0f, 1.25f, {0.95f, 0.98f, 1.0f, 1.0f});
    uiDrawText("\"movement is more important than aim\"", cx - 205.0f, cy - 70.0f, 0.46f, {0.72f, 0.82f, 0.9f, 1.0f});
    const std::string profileLine =
        "Logged in as: " + LocalProfileSystem::instance().currentUsername();
    uiDrawText(profileLine.c_str(), cx - 145.0f, cy - 112.0f, 0.38f,
               {0.45f, 1.0f, 0.62f, 1.0f});

    if (uiButton(win, "PLAY",
        layout.getRectCentered("playButton", {cx - 125.0f, cy + 5.0f, 250.0f, 58.0f}, cx, cy),
        {0.24f,0.82f,0.48f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Play pressed\n");
        r.goPlay = true;
    }

    if (uiButton(win, "SETTINGS",
        layout.getRectCentered("settingsButton", {cx - 125.0f, cy + 82.0f, 250.0f, 58.0f}, cx, cy),
        {0.86f,0.74f,0.28f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Settings pressed\n");
        r.goSettings = true;
    }

    if (uiButton(win, "REPLAYS",
        layout.getRectCentered("replaysButton", {cx - 125.0f, cy + 159.0f, 250.0f, 58.0f}, cx, cy),
        {0.55f,0.35f,0.75f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Replays pressed\n");
        r.goReplays = true;
    }

    if (uiButton(win, "EXIT",
        layout.getRectCentered("exitButton", {cx - 125.0f, cy + 236.0f, 250.0f, 58.0f}, cx, cy),
        {0.65f,0.2f,0.2f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Exit pressed\n");
        r.goExit = true;
    }

    return r;
}
