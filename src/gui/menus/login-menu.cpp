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

namespace {

bool gActive = false;

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
        for (const std::string& id : layout.elementIds())
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

    for (const std::string& id : layout.elementIds())
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
        else if (id == "createAccountNote")
        {
            drawGuiElement(window, *elem);
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
