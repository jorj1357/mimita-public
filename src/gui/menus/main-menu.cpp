// C:\important\quiet\n\mimita-priv-v7\src\gui\menus\main-menu.cpp
// mar 8 2026
/**
 * purpose
 * expose ONE function:
 * drawMainMenu(args)
 *
 * this file DOES:
 * - draw main menu buttons and title
 *
 * this file DOES NOT:
 * - mutate game state directly
 */

#include "main-menu.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "profile/local-profile-system.h"
#include <cstdio>
#include <filesystem>


#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

void openMimitaWebsite()
{
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", "https://mimita.fun", nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

}

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
            // Try PNG, then GIF, then MP4
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

    if (uiButton(win, "SANDBOX",
        layout.getRectCentered("sandboxButton", {cx - 125.0f, cy + 159.0f, 250.0f, 58.0f}, cx, cy),
        {0.25f,0.55f,0.95f,1.0f}).clicked)
        r.startSandbox = true;

    const float accountY = cy + 238.0f;
    if (uiButton(win, "SIGN UP",
        layout.getRectCentered("signUpButton", {cx - 250.0f, accountY, 118.0f, 44.0f}, cx, cy),
        {0.22f,0.5f,0.78f,1.0f}).clicked)
        openMimitaWebsite();
    if (uiButton(win, "SIGN IN",
        layout.getRectCentered("signInButton", {cx - 124.0f, accountY, 118.0f, 44.0f}, cx, cy),
        {0.18f,0.68f,0.42f,1.0f}).clicked)
        r.goSignIn = true;
    if (uiButton(win, "RESET PASSWORD",
        layout.getRectCentered("resetPasswordButton", {cx + 2.0f, accountY, 160.0f, 44.0f}, cx, cy),
        {0.58f,0.44f,0.2f,1.0f}).clicked)
        openMimitaWebsite();
    if (uiButton(win, "HELP",
        layout.getRectCentered("helpButton", {cx + 170.0f, accountY, 80.0f, 44.0f}, cx, cy),
        {0.55f,0.35f,0.75f,1.0f}).clicked)
        r.goHelp = true;

    return r;
}
