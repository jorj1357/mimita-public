#include "main-menu.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "profile/local-profile-system.h"
#include <cstdio>
#include <filesystem>

MainMenuResult drawMainMenu(GLFWwindow* win)
{
    MainMenuResult r{};

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(win, &fbW, &fbH);

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/main-menu.json");

    // Background covers full screen (in framebuffer coordinates)
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

    // Text positions: convert from design (1920x1080) to framebuffer using uiScaleX/Y
    uiDrawText("MiMITA", uiScaleX(872.0f), uiScaleY(380.0f), 1.25f, {0.95f, 0.98f, 1.0f, 1.0f});
    uiDrawText("\"movement is more important than aim\"", uiScaleX(755.0f), uiScaleY(470.0f), 0.46f, {0.72f, 0.82f, 0.9f, 1.0f});
    const std::string profileLine =
        "Logged in as: " + LocalProfileSystem::instance().currentUsername();
    uiDrawText(profileLine.c_str(), uiScaleX(815.0f), uiScaleY(428.0f), 0.38f,
               {0.45f, 1.0f, 0.62f, 1.0f});

    // Buttons: design coordinates are converted to framebuffer inside uiButton()
    if (uiButton(win, "PLAY",
        layout.getRectDesign("playButton", {835.0f, 545.0f, 250.0f, 58.0f}),
        {0.24f,0.82f,0.48f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Play pressed\n");
        r.goPlay = true;
    }

    if (uiButton(win, "SETTINGS",
        layout.getRectDesign("settingsButton", {835.0f, 622.0f, 250.0f, 58.0f}),
        {0.86f,0.74f,0.28f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Settings pressed\n");
        r.goSettings = true;
    }

    if (uiButton(win, "REPLAYS",
        layout.getRectDesign("replaysButton", {835.0f, 699.0f, 250.0f, 58.0f}),
        {0.55f,0.35f,0.75f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Replays pressed\n");
        r.goReplays = true;
    }

    if (uiButton(win, "EXIT",
        layout.getRectDesign("exitButton", {835.0f, 776.0f, 250.0f, 58.0f}),
        {0.65f,0.2f,0.2f,1.0f}).clicked)
    {
        printf("[MAIN MENU] Exit pressed\n");
        r.goExit = true;
    }

    return r;
}
