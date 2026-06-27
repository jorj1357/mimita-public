#include "main-menu.h"
#include "account-panel.h"
#include "menu-avatar-preview.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "auth/auth-system.h"
#include "avatar/avatar.h"
#include "entities/player.h"
#include "entities/player.h"

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

}

MainMenuResult drawMainMenu(GLFWwindow* win)
{
    MainMenuResult r{};
    AuthSystem& auth = AuthSystem::instance();

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

    // Cover image from layout (editable in main-menu.json)
    {
        const GuiElement* cover = layout.get("coverImage");
        if (cover && cover->visible && !cover->backgroundImage.empty())
        {
            float screenW = (float)fbW;
            float screenH = (float)fbH;
            float scaleX = screenW / 1920.0f;
            float scaleY = screenH / 1080.0f;
            float ix = cover->x * scaleX;
            float iy = cover->y * scaleY;
            float iw = cover->w * scaleX;
            float ih = cover->h * scaleY;
            uiDrawImage(cover->backgroundImage.c_str(), {ix, iy, iw, ih});
        }
    }

    // ── Avatar preview (right side, configurable) ────────────────
    MenuAvatarPreview& avatarPreview = MenuAvatarPreview::instance();
    avatarPreview.pollHotReload();
    avatarPreview.update(0.016f, glm::vec3(0.0f, 1.0f, 0.0f));
    avatarPreview.draw(fbW, fbH);

    // Apply current avatar to preview player if it changed
    {
        Player* previewPlayer = avatarPreview.player();
        if (previewPlayer && previewPlayer->modelLoaded)
        {
            AvatarSystem& av = AvatarSystem::instance();
            static std::string lastAvatar;
            std::string current = av.hasAvatar() ? av.currentName() : "";
            if (!current.empty() && current != lastAvatar)
            {
                av.applyToPlayer(*previewPlayer);
                lastAvatar = current;
                printf("[MAIN MENU] Applied avatar: %s\n", current.c_str());
            }
        }
    }

    // Account panel (right side)
    AccountPanelAction account = drawAccountPanel(win);

    // Buttons from layout (positions/sizes/colors editable in main-menu.json)
    const char* btnIds[] = {"playButton", "settingsButton", "replaysButton", "avatarButton", "exitButton", nullptr};
    float sX = (float)fbW / 1920.0f;
    float sY = (float)fbH / 1080.0f;
    for (int i = 0; btnIds[i]; ++i)
    {
        const GuiElement* elem = layout.get(btnIds[i]);
        if (!elem || !elem->visible) continue;

        float bx = elem->x * sX;
        float by = elem->y * sY;
        float bw = elem->w * sX;
        float bh = elem->h * sY;

        UIButtonState s = uiButton(win, elem->text.c_str(),
            {bx, by, bw, bh},
            elem->getBackgroundColorVec(), elem->id.c_str());
        if (!s.clicked) continue;

        printf("[MAIN MENU] %s pressed\n", elem->id.c_str());
        if (elem->id == "playButton") r.goPlay = true;
        else if (elem->id == "settingsButton") r.goSettings = true;
        else if (elem->id == "replaysButton") r.goReplays = true;
        else if (elem->id == "avatarButton") r.goAvatarCreator = true;
        else if (elem->id == "exitButton") r.goExit = true;
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
    else if (account.switchAccount || r.switchAccount)
    {
        r.switchAccount = true;
        AuthSystem::instance().logout();
        openBrowser("https://www.mimita.fun/signin");
        AuthSystem::instance().startLinkFlow();
    }
    else if (account.logOut || r.logOut)
    {
        r.logOut = true;
        AuthSystem::instance().logout();
    }

    // ── Account actions for authenticated users ─────────────────────────────
    if (AuthSystem::instance().state() == AuthState::Authenticated)
    {
        const GuiElement* accountSection = layout.get("accountSection");
        if (accountSection && accountSection->visible)
        {
            float sx = accountSection->x;
            float sy = accountSection->y;
            float sw = accountSection->w;
            float rx = sx * sX;
            float ry = sy * sY;
            float rw = sw * sX;

            float labelY = ry + 10.0f * sY;
            uiDrawText("Continue as:", rx + 20.0f * sX, labelY, 0.25f,
                       {0.6f, 0.65f, 0.75f, 0.8f});

            float nameY = labelY + 30.0f * sY;
            float nameSize = 0.40f;
            std::string display = AuthSystem::instance().displayName();
            float nameW = uiMeasureText(display.c_str(), nameSize);
            uiDrawText(display.c_str(), rx + (rw - nameW) * 0.5f, nameY, nameSize,
                       {0.9f, 0.95f, 1.0f, 1.0f});

            float mmrY = nameY + 35.0f * sY;
            char mmrBuf[64];
            snprintf(mmrBuf, sizeof(mmrBuf), "MMR: %d", AuthSystem::instance().user().stats.currentMmr);
            float mmrW = uiMeasureText(mmrBuf, 0.22f);
            uiDrawText(mmrBuf, rx + (rw - mmrW) * 0.5f, mmrY, 0.22f,
                       {0.4f, 0.7f, 0.9f, 0.8f});

            float playBtnY = mmrY + 50.0f * sY;
            float playBtnW = rw - 40.0f * sX;
            float playBtnH = 40.0f * sY;
            if (uiButton(win, "Play",
                         {rx + 20.0f * sX, playBtnY, playBtnW, playBtnH},
                         {0.2f, 0.6f, 0.3f, 1.0f}, "account-play").clicked)
            {
                r.goPlay = true;
            }

            float switchBtnY = playBtnY + playBtnH + 8.0f * sY;
            if (uiButton(win, "Switch Account",
                         {rx + 20.0f * sX, switchBtnY, playBtnW, playBtnH * 0.8f},
                         {0.25f, 0.3f, 0.45f, 1.0f}, "account-switch").clicked)
            {
                r.switchAccount = true;
            }

            float logoutBtnY = switchBtnY + playBtnH * 0.8f + 8.0f * sY;
            if (uiButton(win, "Logout",
                         {rx + 20.0f * sX, logoutBtnY, playBtnW, playBtnH * 0.8f},
                         {0.4f, 0.15f, 0.15f, 1.0f}, "account-logout").clicked)
            {
                r.logOut = true;
            }
        }
    }

    return r;
}
