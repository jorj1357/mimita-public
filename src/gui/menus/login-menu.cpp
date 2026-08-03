// 08 02 2026, 18 30
/* purpose
* Draws the login menu GUI and drives sign-in, back, sign-up, and
* forgot-password actions from the layout-defined elements.
* Syncs the auth form bindings with the AuthController.
* DOES NOT perform network authentication itself (see auth-controller).
* DOES NOT render the main menu, account panel, or sign-in-code dialog.
* DOES NOT handle website password-reset logic (that lives on the website).
*/

#include "gui/menus/login-menu.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/ui-system.h"
#include "gui/gui-coord.h"
#include "gui/gui-bindings.h"
#include "auth/auth-controller.h"
#include "auth/auth-system.h"
#include "debug/debug-log.h"

#include <cstdio>
#include <shellapi.h>
#include <algorithm>

namespace {

bool gActive = false;

std::vector<std::string> elementIdsByLayer(const GuiLayout& layout)
{
    std::vector<std::pair<int, std::string>> sorted;
    for (const std::string& id : layout.elementIds())
    {
        const GuiElement* e = layout.get(id);
        if (e) sorted.push_back({e->layer, id});
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
            return a.first < b.first;
        });
    std::vector<std::string> ids;
    for (const auto& pair : sorted) ids.push_back(pair.second);
    return ids;
}

void openBrowser(const char* url)
{
    Debug::log(Debug::Category::Gui, "login menu opening %s\n", url);
    HINSTANCE h = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
        Debug::warn(Debug::Category::Gui, "login menu ShellExecuteA failed for %s (result=%lld)\n", url, (long long)(INT_PTR)h);
}

void syncBindingsToForm()
{
    GuiBindings& b = GuiBindings::instance();
    AuthController& ctrl = AuthController::instance();
    ctrl.form().identifier = b.get("auth.form.identifier");
    ctrl.form().password = b.get("auth.form.password");
    ctrl.form().rememberMe = b.get("auth.form.remember_me") == "true" || b.get("auth.form.remember_me") == "1";
}

void syncRuntimeToBindings()
{
    GuiBindings& b = GuiBindings::instance();
    const AuthRuntime& r = AuthController::instance().runtime();
    b.set("auth.runtime.status_text", r.statusText);
    b.set("auth.runtime.error_message", r.errorMessage);
    b.set("auth.runtime.state", std::to_string((int)r.state));
}

}

void loginMenuSetActive(bool active)
{
    gActive = active;
    if (active)
    {
        AuthController::instance().runtime().state = AuthState::ReadyToSignIn;
        AuthController::instance().runtime().statusText.clear();
        AuthController::instance().runtime().errorCode.clear();
        AuthController::instance().runtime().errorMessage.clear();
        syncRuntimeToBindings();
    }
}

LoginMenuResult drawLoginMenu(GLFWwindow* window)
{
    LoginMenuResult result{};
    AuthController& ctrl = AuthController::instance();
    AuthRuntime& rt = ctrl.runtime();

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/login-menu.json");

    if (rt.state == AuthState::SigningIn || rt.state == AuthState::LoadingAccount)
    {
        for (const std::string& id : elementIdsByLayer(layout))
        {
            const GuiElement* elem = layout.get(id);
            if (!elem || !elem->visible) continue;
            if (id == "statusText" || id == "errorText")
            {
                GuiElement dyn = *elem;
                if (id == "statusText" && !rt.statusText.empty())
                {
                    dyn.text = rt.statusText;
                    dyn.textColor = {0.5f, 0.85f, 0.5f, 1.0f};
                    drawGuiElement(window, dyn);
                }
                else if (id == "errorText")
                {
                    dyn.visible = false;
                }
            }
            else
            {
                drawGuiElement(window, *elem);
            }
        }
        return result;
    }

    if (rt.state == AuthState::SignedIn)
    {
        result.signedIn = true;
        return result;
    }

    syncBindingsToForm();

    for (const std::string& id : elementIdsByLayer(layout))
    {
        const GuiElement* elem = layout.get(id);
        if (!elem || !elem->visible) continue;

        if (id == "signInButton")
        {
            UIButtonState s = drawGuiElement(window, *elem);
            if (s.clicked)
            {
                syncBindingsToForm();
                Debug::log(Debug::Category::Gui, "login sign in clicked\n");
                ctrl.signIn();
                syncRuntimeToBindings();
            }
        }
        else if (id == "cancelButton")
        {
            UIButtonState s = drawGuiElement(window, *elem);
            if (s.clicked)
            {
                ctrl.cancel();
                syncRuntimeToBindings();
                result.goBack = true;
            }
        }
        else if (id == "signUpButton")
        {
            UIButtonState s = drawGuiElement(window, *elem);
            if (s.clicked)
            {
                openBrowser("https://mimita.fun/signup");
                result.createAccount = true;
            }
        }
        else if (id == "forgotPasswordButton")
        {
            UIButtonState s = drawGuiElement(window, *elem);
            if (s.clicked)
            {
                openBrowser("https://mimita.fun/forgot-password");
            }
        }
        else if (id == "statusText" && !rt.statusText.empty())
        {
            GuiElement dyn = *elem;
            dyn.text = rt.statusText;
            drawGuiElement(window, dyn);
        }
        else if (id == "errorText" && !rt.errorMessage.empty())
        {
            GuiElement dyn = *elem;
            dyn.text = rt.errorMessage;
            drawGuiElement(window, dyn);
        }
        else
        {
            drawGuiElement(window, *elem);
        }
    }

    syncRuntimeToBindings();

    if (rt.state == AuthState::SignedIn)
    {
        result.signedIn = true;
    }

    return result;
}
