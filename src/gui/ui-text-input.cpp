#include "gui/ui-text-input.h"
#include "gui/ui-system.h"
#include "gui/ui-system-internal.h"
#include "gui/gui-coord.h"
#include "gui/font-stuff/font-loader.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <GLFW/glfw3.h>
#include <glad/glad.h>

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

// ── Internal: clamp cursor and selection after every mutation ─────────

static void clampCursor(UITextInputState& state)
{
    int len = (int)state.value.size();
    state.cursorPos = std::clamp(state.cursorPos, 0, len);
    if (state.selectionStart >= 0)
        state.selectionStart = std::clamp(state.selectionStart, 0, len);
}

// ── Selection helpers ────────────────────────────────────────────────

bool uiTextInputHasSelection(const UITextInputState& state)
{
    return state.selectionStart >= 0 && state.selectionStart != state.cursorPos;
}

void uiTextInputClearSelection(UITextInputState& state)
{
    state.selectionStart = -1;
}

std::string uiTextInputSelectedText(const UITextInputState& state)
{
    if (!uiTextInputHasSelection(state)) return {};
    int a = std::min(state.selectionStart, state.cursorPos);
    int b = std::max(state.selectionStart, state.cursorPos);
    return state.value.substr(a, b - a);
}

void uiTextInputDeleteSelection(UITextInputState& state)
{
    if (!uiTextInputHasSelection(state)) return;
    int a = std::min(state.selectionStart, state.cursorPos);
    int b = std::max(state.selectionStart, state.cursorPos);
    state.value.erase(a, b - a);
    state.cursorPos = a;
    uiTextInputClearSelection(state);
}

void uiTextInputReplaceSelection(UITextInputState& state, const std::string& replacement)
{
    if (uiTextInputHasSelection(state))
        uiTextInputDeleteSelection(state);
    state.value.insert(state.cursorPos, replacement);
    state.cursorPos += (int)replacement.size();
    uiTextInputClearSelection(state);
    clampCursor(state);
}

int uiTextInputCursorWordLeft(const UITextInputState& state, int pos)
{
    if (pos <= 0) return 0;
    int i = pos - 1;
    while (i > 0 && state.value[i - 1] == ' ') --i;
    while (i > 0 && state.value[i - 1] != ' ') --i;
    return i;
}

int uiTextInputCursorWordRight(const UITextInputState& state, int pos)
{
    int len = (int)state.value.size();
    if (pos >= len) return len;
    int i = pos;
    while (i < len && state.value[i] == ' ') ++i;
    while (i < len && state.value[i] != ' ') ++i;
    return i;
}

// ── Mouse: calculate character index from screen X ────────────────────

int uiTextInputCharacterIndexFromX(const std::string& displayText,
                                   float localMouseX, float textScale)
{
    if (displayText.empty() || localMouseX <= 0.0f) return 0;
    if (localMouseX >= uiMeasureText(displayText.c_str(), textScale))
        return (int)displayText.size();

    for (int i = 0; i < (int)displayText.size(); ++i)
    {
        std::string prefix = displayText.substr(0, i + 1);
        float charEndX = uiMeasureText(prefix.c_str(), textScale);
        std::string beforeChar = displayText.substr(0, i);
        float charStartX = uiMeasureText(beforeChar.c_str(), textScale);
        float midX = (charStartX + charEndX) * 0.5f;
        if (localMouseX < midX)
            return i;
    }
    return (int)displayText.size();
}

// ── Char dispatch ────────────────────────────────────────────────────

bool uiTextInputHandleChar(UITextInputState& state, unsigned int codepoint,
                           const UITextInputOptions& opts)
{
    if (!state.focused) return false;
    if (codepoint >= 32 && codepoint <= 126)
    {
        if (opts.characterFilter && !opts.characterFilter(codepoint))
            return true;

        // Calculate available space accounting for selected text replacement
        size_t selectedLen = uiTextInputHasSelection(state)
            ? (size_t)std::abs(state.selectionStart - state.cursorPos)
            : 0;
        if (state.value.size() - selectedLen >= opts.maxLength)
            return true;

        std::string ch(1, (char)codepoint);
        uiTextInputReplaceSelection(state, ch);
        state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
        return true;
    }
    return false;
}

// ── Key dispatch ─────────────────────────────────────────────────────

bool uiTextInputHandleKey(GLFWwindow* window, UITextInputState& state,
                          int key, int action, int mods,
                          const UITextInputOptions& opts)
{
    if (!state.focused) return false;
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;

    const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;
    const bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    // Ctrl+ shortcuts
    if (ctrl)
    {
        switch (key)
        {
        case GLFW_KEY_V:
        {
            const char* clip = glfwGetClipboardString(window);
            if (clip)
            {
                std::string text = clip;
                for (auto& ch : text)
                    if (ch == '\n' || ch == '\r') ch = ' ';

                size_t selectedLen = uiTextInputHasSelection(state)
                    ? (size_t)std::abs(state.selectionStart - state.cursorPos)
                    : 0;
                size_t available = opts.maxLength - (state.value.size() - selectedLen);

                std::string filtered;
                for (unsigned char c : text)
                {
                    if (filtered.size() >= available) break;
                    if (c >= 32 && c <= 126)
                    {
                        if (opts.characterFilter && !opts.characterFilter(c))
                            continue;
                        filtered += (char)c;
                    }
                }
                if (!filtered.empty())
                    uiTextInputReplaceSelection(state, filtered);
            }
            state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
            return true;
        }
        case GLFW_KEY_C:
        {
            if (uiTextInputHasSelection(state))
                glfwSetClipboardString(window, uiTextInputSelectedText(state).c_str());
            return true;
        }
        case GLFW_KEY_X:
        {
            if (uiTextInputHasSelection(state))
            {
                glfwSetClipboardString(window, uiTextInputSelectedText(state).c_str());
                uiTextInputDeleteSelection(state);
                state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
            }
            return true;
        }
        case GLFW_KEY_A:
        {
            if (!state.value.empty())
            {
                state.selectionStart = 0;
                state.cursorPos = (int)state.value.size();
            }
            return true;
        }
        case GLFW_KEY_LEFT:
        {
            uiTextInputClearSelection(state);
            state.cursorPos = uiTextInputCursorWordLeft(state, state.cursorPos);
            return true;
        }
        case GLFW_KEY_RIGHT:
        {
            uiTextInputClearSelection(state);
            state.cursorPos = uiTextInputCursorWordRight(state, state.cursorPos);
            return true;
        }
        case GLFW_KEY_BACKSPACE:
        {
            if (uiTextInputHasSelection(state))
                uiTextInputDeleteSelection(state);
            else if (state.cursorPos > 0)
            {
                int wordStart = uiTextInputCursorWordLeft(state, state.cursorPos);
                state.value.erase(wordStart, state.cursorPos - wordStart);
                state.cursorPos = wordStart;
            }
            state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
            return true;
        }
        case GLFW_KEY_DELETE:
        {
            if (uiTextInputHasSelection(state))
                uiTextInputDeleteSelection(state);
            else if (state.cursorPos < (int)state.value.size())
            {
                int wordEnd = uiTextInputCursorWordRight(state, state.cursorPos);
                state.value.erase(state.cursorPos, wordEnd - state.cursorPos);
            }
            state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
            return true;
        }
        }
    }

    // ── Navigation ─────────────────────────────────────────
    auto moveCursor = [&](int newPos) {
        if (shift && !uiTextInputHasSelection(state))
            state.selectionStart = state.cursorPos;
        state.cursorPos = std::clamp(newPos, 0, (int)state.value.size());
        if (!shift)
            uiTextInputClearSelection(state);
        state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
    };

    if (key == GLFW_KEY_LEFT) { moveCursor(state.cursorPos - 1); return true; }
    if (key == GLFW_KEY_RIGHT) { moveCursor(state.cursorPos + 1); return true; }
    if (key == GLFW_KEY_HOME) { moveCursor(0); return true; }
    if (key == GLFW_KEY_END) { moveCursor((int)state.value.size()); return true; }

    // ── Backspace ──────────────────────────────────────────
    if (key == GLFW_KEY_BACKSPACE)
    {
        if (uiTextInputHasSelection(state))
            uiTextInputDeleteSelection(state);
        else if (state.cursorPos > 0)
        {
            state.value.erase(state.cursorPos - 1, 1);
            --state.cursorPos;
        }
        state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
        return true;
    }

    // ── Delete ─────────────────────────────────────────────
    if (key == GLFW_KEY_DELETE)
    {
        if (uiTextInputHasSelection(state))
            uiTextInputDeleteSelection(state);
        else if (state.cursorPos < (int)state.value.size())
            state.value.erase(state.cursorPos, 1);
        state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
        return true;
    }

    // ── Escape: blur or clear selection ────────────────────
    if (key == GLFW_KEY_ESCAPE)
    {
        if (uiTextInputHasSelection(state))
            uiTextInputClearSelection(state);
        else
            state.focused = false;
        return true;
    }

    // ── Tab: blur ──────────────────────────────────────────
    if (key == GLFW_KEY_TAB)
    {
        state.focused = false;
        return true;
    }

    // ── Enter ──────────────────────────────────────────────
    if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
    {
        if (opts.submitOnEnter)
            state.focused = false;
        return true;
    }

    return false;
}

// ── Render ───────────────────────────────────────────────────────────

bool uiTextInputRender(GLFWwindow* window, const char* id, UIRect designRect,
                       UITextInputState& state, const UITextInputOptions& opts)
{
    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    UIRect screenRect = cs.designToScreen(designRect);

    clampCursor(state);

    // Background
    glm::vec4 bgColor(0.12f, 0.14f, 0.18f, 1.0f);
    uiDrawRect(screenRect, bgColor, id);

    // Focused border
    glm::vec4 borderColor = state.focused
        ? glm::vec4(0.3f, 0.5f, 0.8f, 1.0f)
        : glm::vec4(0.2f, 0.22f, 0.28f, 0.6f);
    uiDrawRectOutline(screenRect, borderColor, id);

    // Padding inside the box
    const float padX = 10.0f;
    const float padY = 6.0f;
    const float textScale = 0.34f;
    const float textH = (float)fontLineHeight * textScale;

    // Clipping for text area
    float clipX = screenRect.x + cs.designToScreenX(padX);
    float clipY = screenRect.y + cs.designToScreenY(padY);
    float clipW = screenRect.w - cs.designToScreenX(padX * 2.0f);
    float clipH = screenRect.h - cs.designToScreenY(padY * 2.0f);

    // Compute display text (mask password)
    std::string displayText = state.value;
    if (opts.password)
        displayText = std::string(state.value.size(), '*');

    // Text position (vertical center)
    float textY = screenRect.y + (screenRect.h - textH) * 0.5f;

    // Measure text before cursor for horizontal scroll
    std::string beforeCursor = displayText.substr(0, state.cursorPos);
    float beforeW = uiMeasureText(beforeCursor.c_str(), textScale);

    // Auto-scroll: keep caret visible
    float textOriginX = screenRect.x + cs.designToScreenX(padX);
    float rightEdge = screenRect.x + screenRect.w - cs.designToScreenX(padX);
    float caretX = textOriginX - state.horizontalScrollPx + beforeW;
    if (caretX > rightEdge)
        state.horizontalScrollPx += caretX - rightEdge + 20.0f;
    if (caretX < textOriginX)
        state.horizontalScrollPx = std::max(0.0f, state.horizontalScrollPx - (textOriginX - caretX + 20.0f));

    float textX = textOriginX - state.horizontalScrollPx;

    // Draw text with scissor
    GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
    GLint scissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
    glEnable(GL_SCISSOR_TEST);
    int fbX = (int)clipX;
    int fbY = (int)((float)UISys::gFbH - clipY - clipH);
    int fbW = (int)clipW;
    int fbH = (int)clipH;
    glScissor(fbX, fbY, std::max(0, fbW), std::max(0, fbH));

    Debug::logThrottled(Debug::Category::Chat, "chat-debug-input-render", 0.25f,
                        "[CHAT DEBUG GUI INPUT] utc=%s id=%s value=\"%s\" focused=%d designRect=(%.1f,%.1f,%.1f,%.1f) screenRect=(%.1f,%.1f,%.1f,%.1f) textPixels=(x=%.1f,y=%.1f,w=%.1f,h=%.1f) clipPixels=(x=%.1f,y=%.1f,w=%.1f,h=%.1f) glScissor=(x=%d,y=%d,w=%d,h=%d) framebuffer=%dx%d\n",
                        chatUtcNow().c_str(), id ? id : "", displayText.c_str(), (int)state.focused,
                        designRect.x, designRect.y, designRect.w, designRect.h,
                        screenRect.x, screenRect.y, screenRect.w, screenRect.h,
                        textX, textY, uiMeasureText(displayText.c_str(), textScale), textH,
                        clipX, clipY, clipW, clipH, fbX, fbY, fbW, fbH,
                        UISys::gFbW, UISys::gFbH);

    if (!displayText.empty())
    {
        // Draw selection highlight
        if (uiTextInputHasSelection(state))
        {
            int selA = std::min(state.selectionStart, state.cursorPos);
            int selB = std::max(state.selectionStart, state.cursorPos);
            std::string beforeSel = displayText.substr(0, selA);
            std::string selText = displayText.substr(selA, selB - selA);
            float beforeSelW = uiMeasureText(beforeSel.c_str(), textScale);
            float selW = uiMeasureText(selText.c_str(), textScale);
            glm::vec4 selColor(0.3f, 0.55f, 0.9f, 0.4f);
            UIRect selRect = {textX + beforeSelW, textY, selW, textH};
            uiDrawRect(selRect, selColor, "text-selection");
        }

        glm::vec4 textColor(0.95f, 0.98f, 1.0f, 1.0f);
        uiDrawText(displayText.c_str(), textX, textY, textScale, textColor);

        // Draw caret
        if (state.focused)
        {
            uint64_t now = (uint64_t)(glfwGetTime() * 1000.0);
            bool caretVis = (now - state.lastActivityMs) < 400 ||
                            ((now - state.lastActivityMs) % 800) < 400;
            if (caretVis)
            {
                glm::vec4 caretColor(0.8f, 0.9f, 1.0f, 1.0f);
                UIRect caretRect = {caretX, textY, 2.0f, textH};
                uiDrawRect(caretRect, caretColor, "caret");
            }
        }
    }
    else
    {
        // Placeholder
        if (!state.focused)
        {
            uiDrawText(opts.placeholder.c_str(), textX, textY,
                       textScale, opts.placeholderColor);
        }
    }

    // Restore scissor
    if (!scissorWas)
        glDisable(GL_SCISSOR_TEST);
    else
        glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);

    // ── Mouse handling ───────────────────────────────────────
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    double fbx = mx, fby = my;
    cs.cursorWindowToScreen(mx, my, fbx, fby);
    const bool rawHovered = pointIn(fbx, fby, screenRect);

    // Mouse button state tracking
    static bool prevMouseDown = false;
    bool mouseDown = UISys::gMouseDown;
    bool clickEdge = UISys::gMouseClickEdge;

    if (rawHovered && clickEdge && !UISys::gDropdownModalActive)
    {
        // Click inside field → focus and place caret
        float localX = (float)(fbx - textOriginX + state.horizontalScrollPx);
        int clickedIndex = uiTextInputCharacterIndexFromX(displayText, localX, textScale);

        // If we were not focused, apply selectAllOnFocus logic
        if (!state.focused && opts.selectAllOnFocus)
        {
            state.focused = true;
            state.selectionStart = 0;
            state.cursorPos = (int)state.value.size();
        }
        else
        {
            state.focused = true;
            state.cursorPos = clickedIndex;
            state.selectionStart = -1;
        }
        state.mouseSelecting = true;
        state.mouseSelectionAnchor = state.cursorPos;
        state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
    }
    else if (rawHovered && mouseDown && state.mouseSelecting && state.focused)
    {
        // Mouse drag while holding → extend selection
        float localX = (float)(fbx - textOriginX + state.horizontalScrollPx);
        int dragIndex = uiTextInputCharacterIndexFromX(displayText, localX, textScale);
        state.cursorPos = dragIndex;
        state.selectionStart = state.mouseSelectionAnchor;
        state.lastActivityMs = (uint64_t)(glfwGetTime() * 1000.0);
    }

    // Mouse release → stop drag
    if (!mouseDown && prevMouseDown)
    {
        state.mouseSelecting = false;
    }
    prevMouseDown = mouseDown;

    // Click outside → blur
    if (clickEdge && !rawHovered)
    {
        state.focused = false;
        state.mouseSelecting = false;
    }

    return state.focused;
}
