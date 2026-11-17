// 2026-11-14
// Host-dispatched input handlers for the active game generation.

#include "game/game-input-module.h"
#include "input/input-commands.h"
#include "input/input-router.h"
#include "renderer/renderer.h"
extern Renderer* gRenderer;
#include "debug/structured-log.h"

#include <string>
#include <nlohmann/json.hpp>
#include <chrono>

#include <GLFW/glfw3.h>

#include "auth/auth-popup.h"
#include "devtools/terminal.h"
#include "gui/gui-bindings.h"
#include "gui/gui-editor.h"
#include "gui/ui-system-internal.h"
#include "gui/hud/chat-window.h"
#include "gui/menus/sign-in-menu.h"
#include "gui/menus/server-info-menu.h"
#include "gui/menus/online-menu.h"
#include "gui/menus/pause-menu.h"
#include "gui/password-popup.h"
#include "mapeditor/map-editor.h"
#include "avatar/avatar-editor.h"

void requestSendChatMessage(const std::string& message);

namespace mimita::game {

void handleGameInputKey(int key, int scancode, int action, int mods) {
    InputCommandSystem::instance().ingestHostKey(key, scancode, action, mods);
    {
        static std::uint64_t lastKeyLogMs = 0;
        const auto nowMs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        if (action == GLFW_PRESS && nowMs - lastKeyLogMs >= 250) {
            lastKeyLogMs = nowMs;
            StructuredLogger::instance().writeEvent(
                StructuredCategory::General, StructuredLevel::Important,
                "input.host-key", "input", "resident key event reached game DLL", 0,
                nlohmann::json{{"key", key}, {"action", action}, {"mods", mods}},
                __FILE__, __LINE__, __FUNCTION__);
        }
    }
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // The hot DLL has its own static GLFW copy, so frame-level key polling
    // cannot see the resident window. Handle the two global modal keys on the
    // host-dispatched event path.
    if (action == GLFW_PRESS && key == GLFW_KEY_GRAVE_ACCENT) {
        if (!PauseMenu::isOpen()) {
            Terminal::instance().toggle();
            return;
        }
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        GLFWwindow* window = gRenderer ? gRenderer->window : nullptr;
        if (PasswordPopup::isOpen()) {
            if (window) PasswordPopup::handleKey(window, GLFW_KEY_ESCAPE, GLFW_PRESS);
            return;
        }
        if (PauseMenu::isOpen()) {
            if (window) PauseMenu::handleKey(window, GLFW_KEY_ESCAPE, GLFW_PRESS);
            return;
        }
        if (Terminal::instance().isOpen()) {
            Terminal::instance().toggle();
            return;
        }
    }

    authPopupHandleKey(key, action);
    signInMenuHandleKey(key, action);
    if (avatarEditorHandleKey(key, action, mods)) return;
    serverInfoMenuHandleKey(key, action, mods);
    onlineMenuHandleKey(key, action);
    std::string chatMsg;
    const bool consumed = handleChatWindowKey(
        gChatWindowState, key, action, mods, nullptr,
        gChatUiTickClock, chatMsg);
    if (!chatMsg.empty()) requestSendChatMessage(chatMsg);
    if (consumed) return;
    guiBindingsHandleKey(key, action, mods);
    Terminal::instance().handleKey(key, mods);
}

void handleGameInputChar(unsigned int codepoint) {
    authPopupHandleChar(codepoint);
    signInMenuHandleChar(codepoint);
    if (avatarEditorHandleChar(codepoint)) return;
    serverInfoMenuHandleChar(codepoint);
    onlineMenuHandleChar(codepoint);
    handleChatWindowChar(gChatWindowState, codepoint);
    if (MapEditor::instance().handleChar(codepoint)) return;
    guiBindingsHandleChar(codepoint);
    Terminal::instance().handleChar(codepoint);
    GuiEditor::instance().handleChar(codepoint);
}

void handleGameInputScroll(double xOffset, double yOffset) {
    (void)xOffset;
    Terminal::instance().handleScroll(yOffset);
    UISys::gScrollYOffset = yOffset;
}

void handleGameInputDrop(int count, const char** paths) {
    avatarEditorHandleDrop(count, paths);
}

void handleGameInputEvent(const mimita::host::GameInputEventV1& event) {
    if (event.version != 2 ||
        event.structSize < sizeof(mimita::host::GameInputEventV1)) {
        return;
    }
    switch (event.kind) {
        case mimita::host::GameInputKind::Key:
            handleGameInputKey(event.key, event.scancode, event.action, event.mods);
            break;
        case mimita::host::GameInputKind::Char:
            handleGameInputChar(event.codepoint);
            break;
        case mimita::host::GameInputKind::Scroll:
            handleGameInputScroll(event.scrollX, event.scrollY);
            break;
        case mimita::host::GameInputKind::MouseButton:
            if (event.action == GLFW_PRESS) {
                StructuredLogger::instance().writeEvent(
                    StructuredCategory::General, StructuredLevel::Important,
                    "input.host-mouse", "input",
                    "resident mouse-button event reached game DLL", 0,
                    nlohmann::json{{"button", event.key},
                                   {"action", event.action},
                                   {"mods", event.mods}},
                    __FILE__, __LINE__, __FUNCTION__);
            }
            InputRouter::ingestMouseButton(event.key, event.action,
                                           InputRouter::Source::Host);
            break;
        case mimita::host::GameInputKind::Cursor:
            InputRouter::ingestCursor(event.cursorX, event.cursorY,
                                      InputRouter::Source::Host);
            break;
        case mimita::host::GameInputKind::Drop: {
            const char* paths[8] = {};
            const std::uint32_t count = event.pathCount < 8 ? event.pathCount : 8;
            for (std::uint32_t i = 0; i < count; ++i) paths[i] = event.paths[i];
            handleGameInputDrop(static_cast<int>(count), paths);
            break;
        }
    }
}

}  // namespace mimita::game
