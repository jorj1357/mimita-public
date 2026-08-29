// 08 29 2026, 15 30
/* purpose
* Implements the private-server password popup using a JSON layout.
* Follows the same pattern as pause-menu.cpp: load JSON, drawGuiElement per ID.
* The password text input uses UITextInputState via drawGuiElement + GuiBindings.
* Does NOT own gameplay input, connection lifecycle, or server browser data.
* Does NOT duplicate the terminal text input or auth popup code input systems.
*/

#include "gui/password-popup.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>

#include "gui/gui-bindings.h"
#include "gui/gui-element-render.h"
#include "gui/gui-layout.h"
#include "gui/ui-system.h"
#include "gui/ui-text-input.h"

extern std::string gSubmittedBinding;

namespace PasswordPopup {
namespace {

bool gOpen = false;
std::string gRoomCode;
std::string gServerName;
std::string gPassword;
bool gWrongPassword = false;
bool gResultReady = false;

} // namespace

bool isOpen() { return gOpen; }

void open(const std::string& roomCode, const std::string& serverName)
{
    gRoomCode = roomCode;
    gServerName = serverName;
    gPassword.clear();
    gWrongPassword = false;
    gResultReady = false;
    gOpen = true;
    GuiBindings::instance().set("popup.password", "");
    GuiBindings::instance().set("popup.errorText", "");
}

void close()
{
    gOpen = false;
    GuiBindings::instance().clearFocus();
}

void setWrongPassword(bool wrong, const std::string& serverName)
{
    gWrongPassword = wrong;
    if (!serverName.empty())
        gServerName = serverName;
    if (wrong)
        GuiBindings::instance().set("popup.errorText", "Wrong password, try again");
    else
        GuiBindings::instance().set("popup.errorText", "");
}

std::string getRoomCode() { return gRoomCode; }
std::string getPassword() { return gPassword; }

void clearResult()
{
    gPassword.clear();
    gResultReady = false;
}

void handleKey(GLFWwindow* window, int key, int action)
{
    (void)window;
    if (!gOpen || action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) close();
}

void render(GLFWwindow* window)
{
    if (!gOpen) return;

    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/password-popup.json");

    if (const GuiElement* overlay = layout.get("overlay"))
        drawGuiElement(window, *overlay);
    if (const GuiElement* panel = layout.get("panel"))
        drawGuiElement(window, *panel);
    if (const GuiElement* title = layout.get("title"))
        drawGuiElement(window, *title);

    if (const GuiElement* elem = layout.get("serverName"))
    {
        GuiElement modified = *elem;
        modified.text = gServerName;
        drawGuiElement(window, modified);
    }

    if (const GuiElement* label = layout.get("passwordLabel"))
        drawGuiElement(window, *label);
    if (const GuiElement* input = layout.get("passwordInput"))
        drawGuiElement(window, *input);

    if (const GuiElement* elem = layout.get("errorText"))
    {
        GuiElement modified = *elem;
        modified.text = gWrongPassword ? "Wrong password, try again" : "";
        drawGuiElement(window, modified);
    }

    if (drawGuiElement(window, *layout.get("okButton")).clicked)
    {
        gPassword = GuiBindings::instance().get("popup.password");
        gResultReady = true;
        close();
    }
    if (drawGuiElement(window, *layout.get("cancelButton")).clicked)
    {
        close();
    }

    if (gSubmittedBinding == "popup.password")
    {
        gSubmittedBinding.clear();
        gPassword = GuiBindings::instance().get("popup.password");
        gResultReady = true;
        close();
    }
}

} // namespace PasswordPopup
