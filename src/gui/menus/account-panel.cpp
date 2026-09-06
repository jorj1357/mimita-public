#include "gui/menus/account-panel.h"
#include "gui/ui-system.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/gui-coord.h"
#include "auth/auth-system.h"
#include "auth/auth-controller.h"
#include "vip/vip-name-render.h"

#include <cstdio>

static bool isSignedIn()
{
    if (AuthSystem::instance().state() == AuthState::Authenticated)
        return true;
    if (AuthController::instance().runtime().state == AuthState::SignedIn)
        return true;
    return false;
}

static std::string getDisplayUsername()
{
    AuthSystem& auth = AuthSystem::instance();
    if (auth.state() == AuthState::Authenticated && !auth.user().username.empty())
        return auth.user().username;
    if (!AuthController::instance().runtime().username.empty())
        return AuthController::instance().runtime().username;
    return auth.displayName();
}

AccountPanelAction drawAccountPanel(GLFWwindow* window)
{
    AuthSystem& auth = AuthSystem::instance();
    AccountPanelAction result;
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/account-panel.json");

    // Draw background from JSON
    const GuiElement* bgEl = layout.get("accountPanelBg");
    if (bgEl && bgEl->visible)
        drawGuiElement(window, *bgEl);

    // Username / No Account text from JSON
    if (isSignedIn())
    {
        const GuiElement* ue = layout.get("usernameText");
        if (ue && ue->visible)
        {
            VipNameDrawOptions nameOpts;
            nameOpts.scale = ue->fontSize > 0.0f ? ue->fontSize : 0.32f;
            nameOpts.alpha = 1.0f;
            nameOpts.phase = 0.0f;
            nameOpts.drawBadge = false;
            nameOpts.detail = &auth.user().vipStyleDetail;
            vipDrawStyledNameCentered(getDisplayUsername(), auth.user().vipAppearance,
                                      uiScaleX(ue->x + ue->w * 0.5f), uiScaleY(ue->y), nameOpts);
        }
    }
    else
    {
        const GuiElement* nae = layout.get("noAccountText");
        if (nae && nae->visible) drawGuiElement(window, *nae);
        const GuiElement* he = layout.get("hintText");
        if (he && he->visible) drawGuiElement(window, *he);
    }

    // Sign In / Sign Up buttons or Authenticated actions
    {
        float btnAreaY = bgEl ? (cs.designToScreenY(bgEl->y + bgEl->h) - 4 * 44 - 40) : 500.0f;
        float btnAreaDesignY = cs.screenToDesignY(btnAreaY);

        if (isSignedIn())
        {
            int mmr = 0;
            int wins = 0, losses = 0, kills = 0, deaths = 0;

            if (auth.state() == AuthState::Authenticated)
            {
                mmr = auth.user().stats.currentMmr;
                wins = auth.user().stats.wins;
                losses = auth.user().stats.losses;
                kills = auth.user().stats.kills;
                deaths = auth.user().stats.deaths;
            }

            // MMR text (dynamic)
            const GuiElement* mmrEl = layout.get("mmrText");
            if (mmrEl && mmrEl->visible)
            {
                char mmrBuf[32];
                snprintf(mmrBuf, sizeof(mmrBuf), "MMR: %d", mmr);
                GuiElement dyn = *mmrEl;
                dyn.text = mmrBuf;
                dyn.y = btnAreaDesignY;
                drawGuiElement(window, dyn);
            }

            // Stats text (dynamic)
            const GuiElement* stEl = layout.get("statsText");
            if (stEl && stEl->visible)
            {
                char statsBuf[64];
                snprintf(statsBuf, sizeof(statsBuf), "W:%d L:%d K:%d D:%d",
                         wins, losses, kills, deaths);
                GuiElement dyn = *stEl;
                dyn.text = statsBuf;
                dyn.y = btnAreaDesignY + 25.0f;
                drawGuiElement(window, dyn);
            }

            // Switch Account button
            const GuiElement* swEl = layout.get("accountSwitchButton");
            if (swEl && swEl->visible)
            {
                GuiElement dyn = *swEl;
                dyn.y = btnAreaDesignY + 50.0f;
                UIButtonState s = drawGuiElement(window, dyn);
                if (s.clicked) result.switchAccount = true;
            }

            // Logout button
            const GuiElement* loEl = layout.get("accountLogoutButton");
            if (loEl && loEl->visible)
            {
                GuiElement dyn = *loEl;
                dyn.y = btnAreaDesignY + 50.0f + 44.0f;
                UIButtonState s = drawGuiElement(window, dyn);
                if (s.clicked) result.logOut = true;
            }
        }
        else
        {
            const GuiElement* siEl = layout.get("signInButton");
            if (siEl && siEl->visible)
            {
                GuiElement dyn = *siEl;
                dyn.y = btnAreaDesignY;
                if (drawGuiElement(window, dyn).clicked)
                {
                    printf("[ACCOUNT] Sign In clicked\n");
                    result.logIn = true;
                }
            }

            const GuiElement* suEl = layout.get("signUpButton");
            if (suEl && suEl->visible)
            {
                GuiElement dyn = *suEl;
                dyn.y = btnAreaDesignY + 54.0f;
                if (drawGuiElement(window, dyn).clicked)
                {
                    printf("[ACCOUNT] Sign Up clicked\n");
                    result.signUp = true;
                }
            }
        }
    }

    return result;
}
