#include "main-menu.h"
#include "account-panel.h"
#include "menu-avatar-preview.h"
#include "../ui-system.h"
#include "../gui-layout.h"
#include "../gui-element-render.h"
#include "../gui-coord.h"
#include "auth/auth-system.h"
#include "auth/auth-controller.h"
#include "auth/auth-popup.h"
#include "avatar/avatar.h"
#include "entities/player.h"
#include "entities/player.h"

#include <cstdio>
#include <filesystem>
#include <shellapi.h>
#include "debug/debug-log.h"
#include "world/texture-store.h"

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
            printf("[MAIN MENU] cover rect=(%.0f,%.0f,%.0f,%.0f) fb=(%d,%d)\n", ix, iy, iw, ih, fbW, fbH);
            // Draw bright red rect first to confirm UI rendering works here
            uiDrawRect({ix, iy, iw, ih}, {1.0f, 0.0f, 0.0f, 1.0f}, "cover-test");
            uiDrawImage(cover->backgroundImage.c_str(), {ix, iy, iw, ih});
            uiDrawRectOutline({ix, iy, iw, ih}, {0.0f, 1.0f, 0.0f, 1.0f}, "cover-bounds");
        } else {
            printf("[MAIN MENU] cover missing: cover=%p visible=%d bg='%s'\n",
                   (void*)cover, cover ? (int)cover->visible : -1,
                   cover ? cover->backgroundImage.c_str() : "null");
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
    const char* btnIds[] = {"playButton", "settingsButton", "replaysButton", "avatarButton", "enterSignInCodeButton", "exitButton", nullptr};
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
        else if (elem->id == "enterSignInCodeButton") r.enterSignInCode = true;
        else if (elem->id == "exitButton") r.goExit = true;
    }

    if (account.logIn)
    {
        printf("[MAIN MENU] go to in-game login screen\n");
        r.goLogin = true;
    }
    else if (account.signUp)
    {
        openBrowser("https://www.mimita.fun/signup");
    }
    else if (account.switchAccount || r.switchAccount)
    {
        r.switchAccount = true;
        AuthSystem::instance().logout();
        openBrowser("https://www.mimita.fun/clientsignin");
    }
    else if (account.logOut || r.logOut)
    {
        r.logOut = true;
        AuthSystem::instance().logout();
    }

    // ── Account actions for authenticated users ─────────────────────────────
    bool mainMenuSignedIn = (AuthSystem::instance().state() == AuthState::Authenticated ||
                             AuthController::instance().runtime().state == AuthState::SignedIn);
    if (mainMenuSignedIn)
    {
        const GuiElement* accountSection = layout.get("accountSection");
        if (accountSection && accountSection->visible)
        {
            // Draw static labels from layout
            const GuiElement* contLabel = layout.get("accountContinueLabel");
            if (contLabel) drawGuiElement(win, *contLabel);

            // Draw buttons from layout
            const char* actBtnIds[] = {"accountPlayButton", "accountSwitchButton", "accountLogoutButton"};
            for (int i = 0; i < 3; ++i) {
                const GuiElement* be = layout.get(actBtnIds[i]);
                if (!be || !be->visible) continue;
                UIButtonState bs = drawGuiElement(win, *be);
                if (!bs.clicked) continue;
                if (i == 0) r.goPlay = true;
                else if (i == 1) r.switchAccount = true;
                else r.logOut = true;
            }

            // Dynamic username (centered, from layout label position if available)
            {
                GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
                std::string display = AuthSystem::instance().displayName();
                Debug::logThrottled(Debug::Category::Gui, "menu-username", 3.0f,
                    "username being displayed=%s source=AuthSystem.displayName()\n",
                    display.c_str());
                float nameSize = 0.40f;
                float nameW = uiMeasureText(display.c_str(), nameSize);
                float cx = cs.designToScreenX(accountSection->x + accountSection->w * 0.5f);
                float cy = cs.designToScreenY(150.0f);
                uiDrawText(display.c_str(), cx - nameW * 0.5f, cy, nameSize,
                           {0.9f, 0.95f, 1.0f, 1.0f});

                // MMR
                char mmrBuf[64];
                snprintf(mmrBuf, sizeof(mmrBuf), "MMR: %d",
                         AuthSystem::instance().user().stats.currentMmr);
                float mmrW = uiMeasureText(mmrBuf, 0.28f);
                uiDrawText(mmrBuf, cx - mmrW * 0.5f, cy + 30.0f, 0.28f,
                           {0.4f, 0.7f, 0.9f, 0.8f});
            }
        }
    }

    return r;
}
