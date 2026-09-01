#pragma once

#include <string>
#include <functional>
#include <cstdint>
#include <glm/glm.hpp>

struct GLFWwindow;
struct UIRect;

// ── Reusable text-editing state ──────────────────────────────────────
// Modeled after the terminal's input-line editing but without command/history baggage.

struct UITextInputState
{
    std::string value;
    int cursorPos = 0;
    int selectionStart = -1;
    bool focused = false;
    bool selectAllOnFocus = false;
    bool submitOnEnter = false;
    float horizontalScrollPx = 0.0f;
    uint64_t lastActivityMs = 0;
    // Mouse drag selection state
    bool mouseSelecting = false;
    int mouseSelectionAnchor = -1;
};

// ── Text input options ───────────────────────────────────────────────

struct UITextInputOptions
{
    size_t maxLength = 200;
    bool selectAllOnFocus = true;
    bool submitOnEnter = false;
    bool password = false;
    std::string placeholder = "type here...";
    glm::vec4 placeholderColor{0.35f, 0.4f, 0.5f, 1.0f};

    // Return false to reject the character.
    // Called for both typed chars and pasted text (per character).
    std::function<bool(unsigned int)> characterFilter;
};

// ── Selection helpers (same logic as terminal) ───────────────────────

bool uiTextInputHasSelection(const UITextInputState& state);
void uiTextInputClearSelection(UITextInputState& state);
std::string uiTextInputSelectedText(const UITextInputState& state);
void uiTextInputDeleteSelection(UITextInputState& state);
void uiTextInputReplaceSelection(UITextInputState& state, const std::string& replacement);

int uiTextInputCursorWordLeft(const UITextInputState& state, int pos);
int uiTextInputCursorWordRight(const UITextInputState& state, int pos);

// ── Dispatch char/key events ─────────────────────────────────────────

bool uiTextInputHandleChar(UITextInputState& state, unsigned int codepoint,
                           const UITextInputOptions& opts);

bool uiTextInputHandleKey(GLFWwindow* window, UITextInputState& state,
                          int key, int action, int mods,
                          const UITextInputOptions& opts);

// ── Render ───────────────────────────────────────────────────────────

// Returns true if the field is still focused after this frame
// (false means the user clicked outside / submitted).
int uiTextInputCharacterIndexFromX(const std::string& displayText,
                                   float localMouseX, float textScale);

bool uiTextInputRender(GLFWwindow* window, const char* id, UIRect designRect,
                       UITextInputState& state, const UITextInputOptions& opts);
