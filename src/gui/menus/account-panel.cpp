#include "gui/menus/account-panel.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "auth/auth-system.h"

#include <cstdio>

AccountPanelAction drawAccountPanel(GLFWwindow* window)
{
    AuthSystem& auth = AuthSystem::instance();
    AccountPanelAction result;
    float fbW = uiScreenW();
    float fbH = uiScreenH();
    float scaleX = fbW / 1920.0f;
    float scaleY = fbH / 1080.0f;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/account-panel.json");

    auto lx = [&](const GuiElement* e) { return e ? e->x * scaleX : 0.0f; };
    auto ly = [&](const GuiElement* e) { return e ? e->y * scaleY : 0.0f; };
    auto lw = [&](const GuiElement* e) { return e ? e->w * scaleX : 0.0f; };
    auto lh = [&](const GuiElement* e) { return e ? e->h * scaleY : 0.0f; };
    auto lcx = [&](const GuiElement* e) { return lx(e) + lw(e) * 0.5f; };

    const GuiElement* bgEl = layout.get("accountPanelBg");
    if (bgEl && bgEl->visible)
    {
        uiDrawRect({lx(bgEl), ly(bgEl), lw(bgEl), lh(bgEl)},
                   {0.025f, 0.03f, 0.04f, 0.4f}, "account-panel-bg");
        uiDrawRectOutline({lx(bgEl), ly(bgEl), lw(bgEl), lh(bgEl)},
                          {0.3f, 0.4f, 0.55f, 0.15f}, "account-panel-border");
    }

    // Username / No Account text
    if (auth.state() == AuthState::Authenticated)
    {
        const GuiElement* ue = layout.get("usernameText");
        if (ue && ue->visible)
        {
            const char* name = auth.user().username.c_str();
            float nameW = uiMeasureText(name, ue->fontSize);
            uiDrawText(name, lcx(ue) - nameW * 0.5f, ly(ue), ue->fontSize, ue->getTextColorVec());
        }
    }
    else
    {
        const GuiElement* nae = layout.get("noAccountText");
        if (nae && nae->visible)
        {
            float tw = uiMeasureText("No Account Detected", nae->fontSize);
            uiDrawText("No Account Detected", lcx(nae) - tw * 0.5f, ly(nae), nae->fontSize, nae->getTextColorVec());
        }
        const GuiElement* he = layout.get("hintText");
        if (he && he->visible)
        {
            float hw = uiMeasureText("Accounts are optional", he->fontSize);
            uiDrawText("Accounts are optional", lcx(he) - hw * 0.5f, ly(he), he->fontSize, he->getTextColorVec());
        }
    }

    // Sign In / Sign Up buttons or Authenticated actions
    {
        const GuiElement* siEl = layout.get("signInButton");
        float btnW = siEl ? lw(siEl) : 200.0f * scaleX;
        float btnH = siEl ? lh(siEl) : 44.0f * scaleY;

        float btnAreaY = ly(bgEl) + lh(bgEl) - btnH * 4 - 40.0f * scaleY;

        if (auth.state() == AuthState::Authenticated)
        {
            // Show MMR
            char mmrBuf[32];
            snprintf(mmrBuf, sizeof(mmrBuf), "MMR: %d", auth.user().stats.currentMmr);
            float mmrW = uiMeasureText(mmrBuf, 0.25f);
            uiDrawText(mmrBuf, lcx(siEl) - mmrW * 0.5f, btnAreaY, 0.25f,
                       {0.4f, 0.7f, 0.9f, 0.8f});

            // Stats line
            btnAreaY += 25.0f * scaleY;
            char statsBuf[64];
            snprintf(statsBuf, sizeof(statsBuf), "W:%d L:%d K:%d D:%d",
                     auth.user().stats.wins, auth.user().stats.losses,
                     auth.user().stats.kills, auth.user().stats.deaths);
            float statsW = uiMeasureText(statsBuf, 0.20f);
            uiDrawText(statsBuf, lcx(siEl) - statsW * 0.5f, btnAreaY, 0.20f,
                       {0.6f, 0.65f, 0.75f, 0.7f});

            // Switch Account button
            btnAreaY += 30.0f * scaleY;
            if (uiButton(window, "Switch Account",
                         {lcx(siEl) - btnW * 0.5f, btnAreaY, btnW, btnH * 0.8f},
                         {0.25f, 0.3f, 0.45f, 1.0f}, "account-switch").clicked)
            {
                result.switchAccount = true;
            }

            // Logout button
            btnAreaY += btnH * 0.8f + 8.0f * scaleY;
            if (uiButton(window, "Logout",
                         {lcx(siEl) - btnW * 0.5f, btnAreaY, btnW, btnH * 0.8f},
                         {0.4f, 0.15f, 0.15f, 1.0f}, "account-logout").clicked)
            {
                result.logOut = true;
            }
        }
        else
        {
            if (siEl && siEl->visible)
            {
                float siX = lcx(siEl) - btnW * 0.5f;
                if (uiButton(window, "Sign In", {siX, btnAreaY, btnW, btnH},
                             siEl->getBackgroundColorVec(), "account-login").clicked)
                {
                    printf("[ACCOUNT] Sign In clicked\n");
                    result.logIn = true;
                }
            }

            const GuiElement* suEl = layout.get("signUpButton");
            if (suEl && suEl->visible)
            {
                float btn2Y = btnAreaY + btnH + 10.0f * scaleY;
                float suX = lcx(suEl) - btnW * 0.5f;
                if (uiButton(window, "Sign Up", {suX, btn2Y, btnW, btnH},
                             suEl->getBackgroundColorVec(), "account-signup").clicked)
                {
                    printf("[ACCOUNT] Sign Up clicked\n");
                    result.signUp = true;
                }
            }
        }
    }

    return result;
}
