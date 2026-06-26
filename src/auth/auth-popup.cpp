#include "auth/auth-popup.h"

#include "gui/ui-system.h"

#include <cstdio>
#include <shellapi.h>

AuthPopupAction drawAuthPopup(GLFWwindow* window)
{
    float fbW = uiScreenW();
    float fbH = uiScreenH();
    AuthPopupAction result = AuthPopupAction::None;

    uiDrawRect({0, 0, fbW, fbH}, {0.0f, 0.0f, 0.0f, 0.55f}, "auth-dim");

    float popupW = uiScaleX(700.0f);
    float popupH = uiScaleY(520.0f);
    float popupX = (fbW - popupW) * 0.5f;
    float popupY = (fbH - popupH) * 0.5f;

    uiDrawRect({popupX, popupY, popupW, popupH},
               {0.06f, 0.07f, 0.09f, 0.97f}, "auth-popup");
    uiDrawRectOutline({popupX, popupY, popupW, popupH},
                      {0.4f, 0.6f, 0.9f, 0.8f}, "auth-popup-border");

    float titleY = popupY + uiScaleY(60.0f);
    uiDrawText("No Mimita Account Detected",
               popupX + uiScaleX(40.0f), titleY, 0.55f,
               {0.85f, 0.9f, 1.0f, 1.0f});

    float descY = titleY + uiScaleY(100.0f);
    const char* desc = "Signing in lets you keep your username, cosmetics,\nachievements, stats, and future progression across computers.";
    uiDrawText(desc, popupX + uiScaleX(40.0f), descY, 0.30f,
               {0.7f, 0.75f, 0.85f, 1.0f});

    float noteY = descY + uiScaleY(120.0f);
    uiDrawText("Accounts are optional, but recommended.",
               popupX + uiScaleX(40.0f), noteY, 0.28f,
               {0.6f, 0.65f, 0.75f, 1.0f});

    float btnY = popupY + popupH - uiScaleY(130.0f);
    float btnW = uiScaleX(280.0f);
    float btnH = uiScaleY(54.0f);
    float centerX = popupX + (popupW - btnW) * 0.5f;

    if (uiButton(window, "Log In",
                 {centerX, btnY, btnW, btnH},
                 {0.2f, 0.45f, 0.8f, 1.0f}, "auth-login").clicked)
    {
        printf("[AUTH] popup: Log In clicked\n");
        result = AuthPopupAction::LogIn;
    }

    if (uiButton(window, "Create Account",
                 {centerX, btnY + btnH + uiScaleY(16.0f), btnW, btnH},
                 {0.18f, 0.4f, 0.22f, 1.0f}, "auth-create").clicked)
    {
        printf("[AUTH] popup: Create Account clicked\n");
        result = AuthPopupAction::CreateAccount;
    }

    float skipBtnY = btnY + (btnH + uiScaleY(16.0f)) * 2 + uiScaleY(8.0f);
    if (uiButton(window, "Continue Offline",
                 {centerX, skipBtnY, btnW, uiScaleY(44.0f)},
                 {0.25f, 0.25f, 0.3f, 1.0f}, "auth-skip").clicked)
    {
        printf("[AUTH] popup: Continue Offline clicked\n");
        result = AuthPopupAction::ContinueOffline;
    }

    return result;
}
