// 2026-11-14
// Host-dispatched input handlers for the active game generation.

#include "game/game-input-module.h"

#include <string>

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
#include "mapeditor/map-editor.h"
#include "avatar/avatar-editor.h"

void requestSendChatMessage(const std::string& message);

namespace mimita::game {

void handleGameInputKey(int key, int scancode, int action, int mods) {
    (void)scancode;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    authPopupHandleKey(key, action);
    signInMenuHandleKey(key, action);
    if (avatarEditorHandleKey(key, action, mods)) return;
    serverInfoMenuHandleKey(key, action, mods);
    onlineMenuHandleKey(key, action);
    std::string chatMsg;
    const bool consumed = handleChatWindowKey(
        gChatWindowState, key, action, mods, glfwGetCurrentContext(),
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
    if (event.version != 1 ||
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
