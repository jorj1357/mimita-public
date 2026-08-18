// 07 30 2026, 13 26
/* purpose
* Implements the 2D chat window renderer at top-left of the screen.
* Uses the existing UI system for scrolling, text input, and text rendering.
* Fade timing uses UiTickClock ticks to stay consistent across frame rates.
* Layout is driven by config/gui/hud.json (chatBg element).
* Does NOT own ChatHistory, network state, or 3D bubble rendering.
* Does NOT handle the "/" key or mouse lock — those are wired externally.
*/
#include "chat-window.h"

// Global state
ChatWindowState gChatWindowState;
UiTickClock gChatUiTickClock;

bool isChatOpen()
{
    return gChatWindowState.open;
}
#include "chat-history.h"
#include "gui/gui-coord.h"
#include "gui/gui-layout.h"
#include "gui/gui-element-render.h"
#include "gui/ui-system-internal.h"
#include "debug/debug-log.h"
#include "terminal/terminal-state.h"
#include "vip/vip-name-render.h"
#include "input/input-commands.h"
#include "input/mouse-lock.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

bool gChatWindowVisible = true;

void initChatWindowState(ChatWindowState& state)
{
    state.open = false;
    state.mouseUnlocked = false;
    state.lastMessageUiTick = 0;
    state.backgroundOpacity = 0.0f;
    state.hovered = false;
    state.scrolledUp = false;
    state.newMessageCount = 0;

    state.textInput.value.clear();
    state.textInput.cursorPos = 0;
    state.textInput.selectionStart = -1;
    state.textInput.focused = false;
    state.textInput.submitOnEnter = true;
    state.textInput.selectAllOnFocus = true;

    state.scroll.scrollY = 0.0f;
    state.scroll.dragging = false;
}

void openChatWindow(ChatWindowState& state)
{
    state.open = true;
    state.backgroundOpacity = 1.0f;
    state.textInput.focused = true;
    state.textInput.selectAllOnFocus = true;
    state.textInput.lastActivityMs = 0;
    state.scrolledUp = false;
    state.newMessageCount = 0;
    state.scroll.scrollY = 0.0f;
    state.textInput.value.clear();
    state.textInput.cursorPos = 0;
    InputCommandSystem::instance().setKeyboardEnabled(false);
    GLFWwindow* win = glfwGetCurrentContext();
    if (win && glfwGetInputMode(win, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void closeChatWindow(ChatWindowState& state)
{
    state.open = false;
    state.textInput.focused = false;
    state.textInput.value.clear();
    state.textInput.cursorPos = 0;
    state.hovered = false;
    InputCommandSystem::instance().setKeyboardEnabled(true);
    GLFWwindow* win = glfwGetCurrentContext();
    if (win)
        MouseLock::set(win, true);
}

void setChatMouseUnlocked(ChatWindowState& state, bool unlocked)
{
    state.mouseUnlocked = unlocked;
}

void handleChatWindowChar(ChatWindowState& state, unsigned int codepoint)
{
    if (!state.open || !state.textInput.focused)
        return;
    UITextInputOptions opts;
    opts.maxLength = 256;
    opts.submitOnEnter = true;
    uiTextInputHandleChar(state.textInput, codepoint, opts);
}

bool handleChatWindowKey(ChatWindowState& state, int key, int action, int mods,
                         GLFWwindow* win, const UiTickClock& clock,
                         std::string& outMessage)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return false;

    // Toggle chat with /
    if (key == GLFW_KEY_SLASH && !state.open)
    {
        openChatWindow(state);
        return true;
    }

    if (!state.open)
        return false;

    // Escape closes chat without sending
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        closeChatWindow(state);
        return true;
    }

    // Let text input handle navigation/editing
    UITextInputOptions opts;
    opts.maxLength = 256;
    opts.submitOnEnter = true;
    bool consumed = uiTextInputHandleKey(win, state.textInput, key, action, mods, opts);

    // Enter with submitOnEnter: send the message
    if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
        action == GLFW_PRESS && consumed)
    {
        outMessage = state.textInput.value;
        // Strip leading / (char callback inserts it when / opens chat)
        if (!outMessage.empty() && outMessage[0] == '/')
            outMessage.erase(0, 1);
        state.textInput.value.clear();
        state.textInput.cursorPos = 0;
        closeChatWindow(state);
        return true;
    }

    return consumed;
}

void renderChatWindow(ChatWindowState& state, GLFWwindow* win,
                      const ChatHistory& history, const UiTickClock& clock,
                      float screenW, float screenH)
{
    if (!gChatWindowVisible && !state.open)
        return;

    // ── Fade computation ──────────────────────────────────────────────
    if (!state.open && !state.hovered)
    {
        uint64_t elapsed = clock.getElapsedTicks(state.lastMessageUiTick);
        if (elapsed < CHAT_FADE_HOLD_TICKS)
            state.backgroundOpacity = 1.0f;
        else if (elapsed < CHAT_FADE_HOLD_TICKS + CHAT_FADE_DURATION_TICKS)
        {
            uint64_t fadeElapsed = elapsed - CHAT_FADE_HOLD_TICKS;
            float t = (float)fadeElapsed / (float)CHAT_FADE_DURATION_TICKS;
            state.backgroundOpacity = 1.0f * (1.0f - t);
        }
        else
            state.backgroundOpacity = 0.0f;
    }
    else
        state.backgroundOpacity = 1.0f;

    if (state.backgroundOpacity < 0.005f)
        return;

    float alpha = state.backgroundOpacity;

    // ── Layout ────────────────────────────────────────────────────────
    // Read from config/gui/hud.json if available, otherwise use defaults
    float winW_d = 480.0f;
    float winH_d = 270.0f;
    float winX_d = 12.0f;
    float winY_d = 12.0f;

    GuiLayout& hudLayout = GuiLayoutManager::instance().getLayout("config/gui/hud.json");
    const GuiElement* chatBg = hudLayout.get("chatBg");
    if (chatBg)
    {
        winX_d = chatBg->x;
        winY_d = chatBg->y;
        if (chatBg->w > 0.0f) winW_d = chatBg->w;
        if (chatBg->h > 0.0f) winH_d = chatBg->h;
    }

    float winX = uiScaleX(winX_d);
    float winY = uiScaleY(winY_d);
    float winW = uiScaleX(winW_d);
    float winH = uiScaleY(winH_d);

    // ── Background ────────────────────────────────────────────────────
    uiDrawRect({winX, winY, winW, winH},
               {0.15f, 0.15f, 0.17f, alpha}, "chat-window-bg");
    uiDrawRectOutline({winX, winY, winW, winH},
                      {0.35f, 0.35f, 0.35f, alpha * 0.6f}, "chat-window-border");

    // ── Message area (above input field) ──────────────────────────────
    float inputH_d = 30.0f;
    float pad_d = 4.0f;
    float msgAreaX_d = winX_d + pad_d;
    float msgAreaY_d = winY_d + inputH_d + pad_d * 2;
    float msgAreaW_d = winW_d - pad_d * 2;
    float msgAreaH_d = winH_d - inputH_d - pad_d * 3;

    float msgAreaX = uiScaleX(msgAreaX_d);
    float msgAreaY = uiScaleY(msgAreaY_d);
    float msgAreaW = uiScaleX(msgAreaW_d);
    float msgAreaH = uiScaleY(msgAreaH_d);

    // Estimate content height per message (~18 design-pixels per line)
    float lineH_d = 20.0f;
    float contentH_d = (float)history.size() * lineH_d;
    float contentH = uiScaleY(contentH_d);

    // Scroll area for messages
    uiBeginScrollArea(win, {msgAreaX, msgAreaY, msgAreaW, msgAreaH}, contentH, state.scroll);

    // Check if user is scrolled up
    float maxScroll = std::max(0.0f, contentH - msgAreaH);
    state.scrolledUp = state.scroll.scrollY < maxScroll - 1.0f;

    // Draw each message (oldest to newest, scroll handles positioning)
    float lineScreenH = uiScaleY(lineH_d);
    float textScale = 0.28f;
    glm::vec4 serverColor = {1.0f, 0.8f, 0.3f, alpha};
    glm::vec4 playerColor = {1.0f, 1.0f, 1.0f, alpha};
    glm::vec4 tickColor = {0.5f, 0.5f, 0.5f, alpha};
    glm::vec4 mutedColor = {0.4f, 0.4f, 0.4f, alpha};

    size_t n = history.size();
    for (size_t i = 0; i < n; ++i)
    {
        const auto& entry = history.get(i);
        // Format: "tick senderName: message"
        char line[512];
        if (entry.senderType == ChatSenderType::Server)
        {
            // Server/system messages: "[system] message"
            std::snprintf(line, sizeof(line), "[system] %s", entry.text.c_str());
            glm::vec4 col = serverColor;
            if (entry.muted) col = mutedColor;
            uiDrawText(line, msgAreaX + uiScaleX(2),
                       msgAreaY + (float)i * lineScreenH,
                       textScale, col);
        }
        else
        {
            if (entry.muted)
            {
                std::snprintf(line, sizeof(line), "%llu %s: %s",
                             (unsigned long long)entry.serverTick,
                             entry.senderName.c_str(),
                             entry.text.c_str());
                uiDrawText(line, msgAreaX + uiScaleX(2),
                           msgAreaY + (float)i * lineScreenH,
                           textScale, mutedColor);
                continue;
            }

            char tickBuf[48];
            std::snprintf(tickBuf, sizeof(tickBuf), "%llu ",
                         (unsigned long long)entry.serverTick);
            const float lineY = msgAreaY + (float)i * lineScreenH;
            float cursorX = msgAreaX + uiScaleX(2);
            uiDrawText(tickBuf, cursorX, lineY, textScale, tickColor);
            cursorX += uiMeasureText(tickBuf, textScale);

            VipNameDrawOptions nameOptions;
            nameOptions.scale = textScale;
            nameOptions.alpha = alpha;
            nameOptions.phase = 0.0f;
            nameOptions.detail = &entry.senderVipStyleDetail;
            vipDrawStyledName(entry.senderName, entry.senderVipAppearance,
                              cursorX, lineY, nameOptions);
            cursorX += vipMeasureStyledName(entry.senderName,
                                            entry.senderVipAppearance,
                                            nameOptions);
            uiDrawText(": ", cursorX, lineY, textScale, playerColor);
            cursorX += uiMeasureText(": ", textScale);
            uiDrawText(entry.text.c_str(), cursorX, lineY, textScale, playerColor);
        }
    }

    uiEndScrollArea({msgAreaX, msgAreaY, msgAreaW, msgAreaH}, contentH, state.scroll);

    // ── Input field (only when chat is open) ──────────────────────────
    if (state.open)
    {
        float inputX = uiScaleX(winX_d + pad_d);
        float inputY = uiScaleY(winY_d + pad_d);
        float inputW = uiScaleX(winW_d - pad_d * 2);
        float inputH = uiScaleY(inputH_d);

        uiDrawRect({inputX, inputY, inputW, inputH},
                   {0.0f, 0.0f, 0.0f, alpha}, "chat-input-bg");
        uiDrawRectOutline({inputX, inputY, inputW, inputH},
                          {0.4f, 0.4f, 0.4f, alpha * 0.6f}, "chat-input-border");

        UITextInputOptions opts;
        opts.maxLength = 256;
        opts.selectAllOnFocus = true;
        opts.submitOnEnter = true;
        uiTextInputRender(win, "chat_input",
                         {inputX, inputY, inputW, inputH},
                         state.textInput, opts);
    }

    // ── "N new messages" indicator when scrolled up ───────────────────
    if (state.scrolledUp && state.newMessageCount > 0 && !state.open)
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%llu new message%s",
                     (unsigned long long)state.newMessageCount,
                     state.newMessageCount == 1 ? "" : "s");
        float indW = uiMeasureText(buf, 0.28f);
        float indX = winX + (winW - indW) * 0.5f;
        float indY = winY + winH - uiScaleY(24.0f);
        uiDrawText(buf, indX, indY, 0.28f, {1.0f, 1.0f, 0.3f, alpha});
    }
}
