#include "main-menu.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
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

    // Render all layout elements using the unified renderer
    // profileLine is handled separately because its text is dynamic (includes username)
    for (const std::string& id : layout.elementIds())
    {
        if (id == "profileLine") continue;

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

    // Render profile line with dynamic username text
    const GuiElement* profileEl = layout.get("profileLine");
    if (profileEl)
    {
        const std::string profileText =
            "Logged in as: " + LocalProfileSystem::instance().currentUsername();
        float sx = uiScaleX(profileEl->x);
        float sy = uiScaleY(profileEl->y);
        float scale = profileEl->fontSize > 0.0f ? profileEl->fontSize : 0.38f;
        glm::vec4 color = profileEl->getTextColorVec();
        uiDrawText(profileText.c_str(), sx, sy, scale, color);
    }

    return r;
}
