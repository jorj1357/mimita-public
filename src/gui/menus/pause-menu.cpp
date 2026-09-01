// 09 01 2026, 00 00
/* purpose
* Implements the in-game ESC modal with resume, settings, Discord, invite, leave, and quick controls.
* Uses config/gui/pause-menu.json for every visual rectangle, label, color, and row cadence.
* Keeps player input disabled while the modal or embedded settings screen is visible.
* Does NOT draw the 3D world, own the settings controls, or duplicate duel queue behavior.
* Does NOT disconnect until the player explicitly confirms Leave Room.
* Does NOT require the active mode to be a duel before showing quick controls.
*/

#include "gui/menus/pause-menu.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include <shellapi.h>

#include "debug/debug-log.h"
#include "duel/duel-history.h"
#include "duel/duel-queue.h"
#include "game/game-state.h"
#include "game/duel.h"
#include "gui/gui-element-render.h"
#include "gui/gui-layout.h"
#include "gui/gui-main.h"
#include "gui/menus/settings-menu.h"
#include "gui/menus/help-menu.h"
#include "gui/ui-system.h"
#include "input/input-commands.h"
#include "input/mouse-lock.h"
#include "network/multiplayer-context.h"
#include "notifications/notifications.h"
#include "terminal/terminal-state.h"

extern DuelManager gDuelManager;

namespace PauseMenu {
namespace {

enum class View { Main, ConfirmLeave, Settings, Help };
bool gOpen = false;
View gView = View::Main;

void restoreGameplayCursor(GLFWwindow* window)
{
    glfwSetInputMode(window, GLFW_CURSOR,
        MouseLock::locked() ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

void drawText(const GuiElement* element, const std::string& text, float yOffset = 0.0f)
{
    if (!element || !element->visible) return;
    const float scale = element->fontSize > 0.0f ? element->fontSize : 0.30f;
    uiDrawText(text.c_str(), uiScaleX(element->x), uiScaleY(element->y + yOffset),
               scale, element->getTextColorVec());
}

std::string lastSeenText(uint64_t unixMs)
{
    const uint64_t now = MimitaNet::nowMs();
    const uint64_t seconds = now > unixMs ? (now - unixMs) / 1000 : 0;
    if (seconds < 60) return "Last seen: " + std::to_string(seconds) + "s ago";
    if (seconds < 3600) return "Last seen: " + std::to_string(seconds / 60) + "m ago";
    if (seconds < 86400) return "Last seen: " + std::to_string(seconds / 3600) + "h ago";
    return "Last seen: " + std::to_string(seconds / 86400) + "d ago";
}

void drawQuickControls(GLFWwindow* window, GuiLayout& layout)
{
    const GuiElement* panel = layout.get("recentPanel");
    const GuiElement* header = layout.get("recentHeader");
    const GuiElement* empty = layout.get("recentEmpty");
    const GuiElement* username = layout.get("recentUsername");
    const GuiElement* room = layout.get("recentRoom");
    if (!panel || !username || !room) return;

    drawGuiElement(window, *panel);
    drawText(header, header ? header->text : "QUICK CONTROLS");

    // Draw keybind entries
    struct QuickControl {
        const char* name;
        const char* key;
    };
    static const QuickControl controls[] = {
        {"Mouse lock/unlock", "L"},
        {"Save last 15 sec replay", "P"},
        {"Open latest replay", "J"},
        {"Invite friend", "I"},
    };

    const int count = (int)(sizeof(controls) / sizeof(controls[0]));
    for (int i = 0; i < count; ++i) {
        const float offset = username->h * static_cast<float>(i);
        drawText(username, controls[i].name, offset);
        drawText(room, controls[i].key, offset);
    }
}

void leaveRoom(GLFWwindow* window)
{
    const bool duelQueueActive = DuelQueue::instance().isActive();
    const bool activeDuel = duelQueueActive &&
        (DuelQueue::instance().state() == DuelQueueState::InDuel ||
         DuelQueue::instance().state() == DuelQueueState::MatchEnd);

    if (activeDuel) {
        DuelQueue::instance().returnToQueue();
        gOpen = false;
        gView = View::Main;
        InputCommandSystem::instance().setKeyboardEnabled(true);
        restoreGameplayCursor(window);
        return;
    }
    if (duelQueueActive) {
        DuelQueue::instance().stopQueue();
        gGuiMenuState = GUI_MENU_MAIN;
    } else {
        if (gDuelManager.phase() != DuelPhase::Off)
            gDuelManager.stopDuel();
        MimitaNet::mpShutdown(MP_CONTEXT);
        gGuiMenuState = GUI_MENU_SERVERS;
    }
    GAME_STATE = GAME_MENU;
    gOpen = false;
    gView = View::Main;
    InputCommandSystem::instance().setKeyboardEnabled(true);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

} // namespace

bool isOpen() { return gOpen; }

void toggle(GLFWwindow* window)
{
    if (gOpen) { close(window); return; }
    if (GAME_STATE != GAME_PLAYING) return;
    gOpen = true;
    gView = View::Main;
    InputCommandSystem::instance().setKeyboardEnabled(false);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void close(GLFWwindow* window)
{
    gOpen = false;
    gView = View::Main;
    InputCommandSystem::instance().setKeyboardEnabled(true);
    restoreGameplayCursor(window);
}

void handleKey(GLFWwindow* window, int key, int action)
{
    if (!gOpen || action != GLFW_PRESS) return;
    if (key != GLFW_KEY_ESCAPE) return;
    if (gView == View::ConfirmLeave) gView = View::Main;
    else if (gView == View::Settings) gView = View::Main;
    else close(window);
}

void render(GLFWwindow* window)
{
    if (!gOpen) return;
    GuiLayout& layout = GuiLayoutManager::instance().getLayout("config/gui/pause-menu.json");
    if (const GuiElement* overlay = layout.get("overlay")) drawGuiElement(window, *overlay);

    if (gView == View::Settings) {
        SettingsMenuResult result = drawSettingsMenu(window);
        if (result.goBack) gView = View::Main;
        return;
    }

    if (gView == View::Help) {
        HelpMenuResult result = drawHelpMenu(window);
        if (result.goBack) gView = View::Main;
        return;
    }

    if (gView == View::ConfirmLeave) {
        drawGuiElement(window, *layout.get("confirmPanel"));
        drawText(layout.get("confirmText"), layout.get("confirmText")->text);
        if (drawGuiElement(window, *layout.get("confirmYes")).clicked) leaveRoom(window);
        if (drawGuiElement(window, *layout.get("confirmNo")).clicked) gView = View::Main;
        return;
    }

    for (const char* id : {"menuPanel", "menuTitle"}) {
        if (const GuiElement* element = layout.get(id)) drawGuiElement(window, *element);
    }
    if (const GuiElement* resume = layout.get("resumeButton"); resume && drawGuiElement(window, *resume).clicked) close(window);
    if (const GuiElement* settings = layout.get("settingsButton"); settings && drawGuiElement(window, *settings).clicked) gView = View::Settings;
    if (const GuiElement* help = layout.get("helpButton"); help && drawGuiElement(window, *help).clicked) gView = View::Help;
    if (const GuiElement* discord = layout.get("discordButton"); discord && drawGuiElement(window, *discord).clicked) {
        const GuiElement* url = layout.get("discordUrl");
        const std::string value = url && !url->text.empty()
            ? url->text : "https://discord.gg/sY8QHbfG9D";
        ShellExecuteA(nullptr, "open", value.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        Debug::log(Debug::Category::Gui, "[DISCORD] opened invite link\n");
    }
    if (const GuiElement* invite = layout.get("inviteButton"); invite && drawGuiElement(window, *invite).clicked) {
        const GuiElement* url = layout.get("inviteUrl");
        const std::string value = url && !url->text.empty() ? url->text : "https://www.mimita.fun/download";
        glfwSetClipboardString(window, value.c_str());
        NotificationSystem::instance().push("Code copied!", "Code: " + value, 180, {});
    }
    if (const GuiElement* leave = layout.get("leaveButton"); leave && drawGuiElement(window, *leave).clicked) gView = View::ConfirmLeave;
    drawQuickControls(window, layout);
}

} // namespace PauseMenu
