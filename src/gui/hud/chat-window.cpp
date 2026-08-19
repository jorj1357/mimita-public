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

#include <chrono>
#include <ctime>

namespace
{
std::string chatUtcNow()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm utc{};
    gmtime_s(&utc, &now);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}
}

// Global state
ChatWindowState gChatWindowState;
UiTickClock gChatUiTickClock;

bool isChatOpen()
{
    return gChatWindowState.open;
}

void noteChatActivity()
{
    gChatWindowState.lastMessageUiTick = gChatUiTickClock.getTick();
    gChatWindowState.backgroundOpacity = CHAT_MAX_OPACITY;
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
    state.backgroundOpacity = CHAT_MAX_OPACITY;
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
    Debug::log(Debug::Category::Chat, "[CHAT INPUT OPEN] key=/\n");
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
    Debug::log(Debug::Category::Chat, "[CHAT INPUT TEXT] len=%zu\n",
               state.textInput.value.size());
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
        Debug::log(Debug::Category::Chat, "[CHAT INPUT SUBMIT] len=%zu\n",
                   outMessage.size());
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
            state.backgroundOpacity = CHAT_MAX_OPACITY;
        else if (elapsed < CHAT_FADE_HOLD_TICKS + CHAT_FADE_DURATION_TICKS)
        {
            uint64_t fadeElapsed = elapsed - CHAT_FADE_HOLD_TICKS;
            float t = (float)fadeElapsed / (float)CHAT_FADE_DURATION_TICKS;
            state.backgroundOpacity = CHAT_MAX_OPACITY * (1.0f - t);
        }
        else
            state.backgroundOpacity = 0.0f;
    }
    else
        state.backgroundOpacity = CHAT_MAX_OPACITY;

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

    // Chat messages area — read from JSON or compute from chatBg
    float msgAreaX_d, msgAreaY_d, msgAreaW_d, msgAreaH_d;
    const GuiElement* chatMsgs = hudLayout.get("chatMessages");
    if (chatMsgs)
    {
        msgAreaX_d = chatMsgs->x;
        msgAreaY_d = chatMsgs->y;
        msgAreaW_d = chatMsgs->w > 0.0f ? chatMsgs->w : winW_d - 8.0f;
        msgAreaH_d = chatMsgs->h > 0.0f ? chatMsgs->h : winH_d - 40.0f;
    }
    else
    {
        float pad_d = 4.0f;
        float inputH_d = 30.0f;
        msgAreaX_d = winX_d + pad_d;
        msgAreaY_d = winY_d + inputH_d + pad_d * 2;
        msgAreaW_d = winW_d - pad_d * 2;
        msgAreaH_d = winH_d - inputH_d - pad_d * 3;
    }

    // Chat input bar — read from JSON or compute from chatBg
    float inputX_d, inputY_d, inputW_d, inputH_d;
    const GuiElement* chatBar = hudLayout.get("chatBar");
    if (chatBar)
    {
        inputX_d = chatBar->x;
        inputY_d = chatBar->y;
        inputW_d = chatBar->w > 0.0f ? chatBar->w : winW_d - 8.0f;
        inputH_d = chatBar->h > 0.0f ? chatBar->h : 28.0f;
    }
    else
    {
        float pad_d = 4.0f;
        inputX_d = winX_d + pad_d;
        inputY_d = winY_d + pad_d;
        inputW_d = winW_d - pad_d * 2;
        inputH_d = 30.0f;
    }

    float winX = uiScaleX(winX_d);
    float winY = uiScaleY(winY_d);
    float winW = uiScaleX(winW_d);
    float winH = uiScaleY(winH_d);

    Debug::logThrottled(Debug::Category::Chat, "chat-debug-layout", 0.25f,
                        "[CHAT DEBUG GUI] utc-layout=%s framebuffer=%dx%d windowDesign=(%.1f,%.1f,%.1f,%.1f) windowPixels=(%.1f,%.1f,%.1f,%.1f) messageDesign=(%.1f,%.1f,%.1f,%.1f) inputDesign=(%.1f,%.1f,%.1f,%.1f) history=%zu open=%d opacity=%.3f\n",
                        chatUtcNow().c_str(), UISys::gFbW, UISys::gFbH,
                        winX_d, winY_d, winW_d, winH_d, winX, winY, winW, winH,
                        msgAreaX_d, msgAreaY_d, msgAreaW_d, msgAreaH_d,
                        inputX_d, inputY_d, inputW_d, inputH_d,
                        history.size(), (int)state.open, alpha);

    // ── Background ────────────────────────────────────────────────────
    uiDrawRect({winX, winY, winW, winH},
               {0.15f, 0.15f, 0.17f, alpha}, "chat-window-bg");
    uiDrawRectOutline({winX, winY, winW, winH},
                      {0.35f, 0.35f, 0.35f, alpha * 0.6f}, "chat-window-border");

    // ── Message area ──────────────────────────────────────────────────
    float msgAreaX = uiScaleX(msgAreaX_d);
    float msgAreaY = uiScaleY(msgAreaY_d);
    float msgAreaW = uiScaleX(msgAreaW_d);
    float msgAreaH = uiScaleY(msgAreaH_d);

    // Estimate content height per message (~18 design-pixels per line)
    float lineH_d = 20.0f;
    float contentH_d = (float)history.size() * lineH_d;

    // Keep the newest line at the bottom until the player scrolls upward.
    float maxScroll = std::max(0.0f, contentH_d - msgAreaH_d);
    if (!state.scrolledUp)
        state.scroll.scrollY = maxScroll;

    // Scroll area for messages
    // Scroll widgets take design-space rectangles/heights and perform their
    // own conversion. Passing screen-space values here double-scaled the
    // scissor rectangle and hid the message glyphs.
    uiBeginScrollArea(win, {msgAreaX_d, msgAreaY_d, msgAreaW_d, msgAreaH_d},
                      contentH_d, state.scroll);

    // Check if user is scrolled up
    state.scrolledUp = state.scroll.scrollY < maxScroll - 1.0f;

    // Draw each message (newest at bottom — draw from bottom up)
    float lineScreenH = uiScaleY(lineH_d);
    float textScale = 0.28f;
    glm::vec4 serverColor = {1.0f, 0.8f, 0.3f, alpha};
    glm::vec4 playerColor = {1.0f, 1.0f, 1.0f, alpha};
    glm::vec4 mutedColor = {0.4f, 0.4f, 0.4f, alpha};

    size_t n = history.size();
    for (size_t i = 0; i < n; ++i)
    {
        const auto& entry = history.get(i);
        char line[512];
        if (entry.senderType == ChatSenderType::Server)
        {
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
                std::snprintf(line, sizeof(line), "%s: %s",
                             entry.senderName.c_str(),
                             entry.text.c_str());
                uiDrawText(line, msgAreaX + uiScaleX(2),
                           msgAreaY + (float)i * lineScreenH,
                           textScale, mutedColor);
                continue;
            }

            // Format: (username): (message)
            std::snprintf(line, sizeof(line), "%s: %s",
                         entry.senderName.c_str(),
                         entry.text.c_str());

            const float lineY = msgAreaY + (float)i * lineScreenH;
            float cursorX = msgAreaX + uiScaleX(2);

            // Draw sender name in white, message in white
            uiDrawText(line, cursorX, lineY, textScale, playerColor);

            std::string renderKey = "chat-debug-message-" +
                                    std::to_string(entry.messageId);
            Debug::logThrottled(Debug::Category::Chat, renderKey.c_str(), 1.0f,
                                "[CHAT DEBUG GUI MESSAGE] utc=%s index=%zu messageId=%llu text=\"%s\" sender=%s senderEntityId=%u serverTick=%llu line=\"%s\" textPixels=(x=%.1f,y=%.1f,w=%.1f,h=%.1f) messageAreaPixels=(x=%.1f,y=%.1f,w=%.1f,h=%.1f) scrollY=%.1f alpha=%.3f\n",
                                chatUtcNow().c_str(), i,
                                (unsigned long long)entry.messageId, entry.text.c_str(),
                                entry.senderName.c_str(), entry.senderEntityId,
                                (unsigned long long)entry.serverTick, line,
                                cursorX, lineY, uiMeasureText(line, textScale),
                                lineScreenH,
                                msgAreaX, msgAreaY, msgAreaW, msgAreaH,
                                state.scroll.scrollY, alpha);
        }
    }

    uiEndScrollArea({msgAreaX_d, msgAreaY_d, msgAreaW_d, msgAreaH_d},
                    contentH_d, state.scroll);

    // ── Input field (only when chat is open) ──────────────────────────
    if (state.open)
    {
        float inputX = uiScaleX(inputX_d);
        float inputY = uiScaleY(inputY_d);
        float inputW = uiScaleX(inputW_d);
        float inputH = uiScaleY(inputH_d);

        uiDrawRect({inputX, inputY, inputW, inputH},
                   {0.0f, 0.0f, 0.0f, alpha}, "chat-input-bg");
        uiDrawRectOutline({inputX, inputY, inputW, inputH},
                          {0.4f, 0.4f, 0.4f, alpha * 0.6f}, "chat-input-border");

        UITextInputOptions opts;
        opts.maxLength = 256;
        opts.selectAllOnFocus = true;
        opts.submitOnEnter = true;
        // uiTextInputRender converts design coordinates to screen coordinates.
        // Passing already-scaled values double-scaled the input bar.
        uiTextInputRender(win, "chat_input",
                         {inputX_d, inputY_d, inputW_d, inputH_d},
                         state.textInput, opts);
    }

    Debug::logThrottled(Debug::Category::Chat, "chat-hud-render", 1.0f,
                        "[CHAT HUD] rendered=%zu opacity=%.2f open=%d\n",
                        history.size(), alpha, (int)state.open);

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
