#include "gui-element-render.h"
#include "gui-coord.h"
#include "gui-bindings.h"
#include "ui-text-input.h"
#include "debug/debug-log.h"
#include "../avatar/avatar-editor-dropdown.h"
#include "gui/font-stuff/font-loader.h"
#include "network/net_common.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <unordered_map>

// Persistent state for slider/dropdown elements keyed by (window, elementId)
// ── Reusable text-input states per element ───────────────────────────
// Used by drawGuiElement text_input rendering and guiBindings input dispatch.
// Not in anonymous namespace so gui-bindings.cpp can reference it.
std::unordered_map<std::string, UITextInputState> gTextInputStates;

namespace {

UITextInputState& getOrCreateTextState(const std::string& id)
{
    return gTextInputStates[id];
}

// ── Text positioning helper ──────────────────────────────────────────
// Returns the screen Y position for text given a design-space rect and alignment.
static float computeTextY(const GuiCoordinateSystem& cs, const UIRect& designRect,
                          float fontSize, const std::string& verticalAlign, float paddingY)
{
    float designCenterY = designRect.y + designRect.h * 0.5f;
    float textHeight = (float)fontLineHeight * fontSize;
    if (verticalAlign == "middle")
    {
        float screenCenterY = cs.designToScreenY(designCenterY);
        return screenCenterY - textHeight * 0.5f;
    }
    else if (verticalAlign == "bottom")
    {
        float screenBottom = cs.designToScreenY(designRect.y + designRect.h - paddingY);
        return screenBottom - textHeight;
    }
    // default: top
    return cs.designToScreenY(designRect.y + paddingY);
}

struct SliderState {
    float value = 0.5f;
    float minVal = 0.0f;
    float maxVal = 1.0f;
};
std::unordered_map<std::string, SliderState> gSliderState;
std::unordered_map<std::string, DropdownState> gDropdownState;
}

// Exposed for overlay post-pass rendering
std::unordered_map<std::string, DropdownState>& getDropdownStates()
{
    return gDropdownState;
}

UIButtonState drawGuiElement(GLFWwindow* win, const GuiElement& elem,
                             bool* checkboxValue,
                             const UIRect* overrideDesignRect)
{
    if (!elem.visible) return {};

    // Resolve visibility from data binding
    if (!elem.visibleWhenBinding.empty())
    {
        std::string val = GuiBindings::instance().get(elem.visibleWhenBinding);
        if (elem.visibleWhenOp == "equals")
        {
            if (val != elem.visibleWhenValue) return {};
        }
        else if (elem.visibleWhenOp == "not_equals")
        {
            if (val == elem.visibleWhenValue) return {};
        }
        else if (elem.visibleWhenOp == "exists")
        {
            if (val.empty()) return {};
        }
        else if (elem.visibleWhenOp == "not_exists")
        {
            if (!val.empty()) return {};
        }
    }

    // Use override rect if provided, else elem's own position
    float rx = overrideDesignRect ? overrideDesignRect->x : elem.x;
    float ry = overrideDesignRect ? overrideDesignRect->y : elem.y;
    float rw = overrideDesignRect ? overrideDesignRect->w : elem.w;
    float rh = overrideDesignRect ? overrideDesignRect->h : elem.h;

    // Apply margin (external spacing around the element)
    if (elem.margin > 0.0f) {
        rx += elem.margin;
        ry += elem.margin;
        rw -= elem.margin * 2.0f;
        rh -= elem.margin * 2.0f;
        if (rw <= 0.0f || rh <= 0.0f) return {};
    }

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    const std::string& type = elem.type;

    // Resolve binding text (for dynamic elements)
    GuiBindings& bindings = GuiBindings::instance();
    std::string resolvedText;
    bool hasBindingText = false;
    if (!elem.binding.empty()) {
        resolvedText = bindings.get(elem.binding, elem.bindingFallback);
        hasBindingText = true;
    }

    if (type == "button")
    {
        if (!elem.enabled) return {};
        UIRect designRect = {rx, ry, rw, rh};
        glm::vec4 bg = elem.getBackgroundColorVec();
        glm::vec4 hc = elem.getHoverColorVec();
        glm::vec4 pc = elem.getPressedColorVec();
        bool hasHover = elem.hasHoverColor();
        bool hasPressed = elem.hasPressedColor();

        float buttonFontSize = elem.fontSize > 0.0f ? elem.fontSize : 0.0f;

        return uiButton(win, elem.text.c_str(), designRect, bg, elem.id.c_str(),
                        hasHover ? &hc : nullptr,
                        hasPressed ? &pc : nullptr,
                        elem.hoverSound.c_str(),
                        elem.clickSound.c_str(),
                        buttonFontSize);
    }

    if (type == "checkbox")
    {
        if (!elem.enabled) return {};
        // Read checkbox state from binding ("true"/"false"), default false
        bool checked = false;
        if (!elem.binding.empty()) {
            checked = GuiBindings::instance().get(elem.binding) == "true";
        }

        UIRect designRect = {rx, ry, rw, rh};
        UIRect screenRect = cs.designToScreen(designRect);
        // Use full height as box size, capped at reasonable value
        float boxSize = std::max(16.0f, std::min(rh, 32.0f));
        UIRect boxRect = {rx, ry, boxSize, boxSize};
        UIRect boxScreen = cs.designToScreen(boxRect);

        // Draw label text to the right of the box
        const std::string& label = hasBindingText && !resolvedText.empty() ? resolvedText : elem.text;
        float textX = cs.designToScreenX(rx + boxSize + 6.0f);
        float textY = computeTextY(cs, {rx + boxSize + 6.0f, ry, rw - boxSize - 6.0f, boxSize},
                                     elem.fontSize > 0.0f ? elem.fontSize : 0.28f,
                                     "middle", 2.0f);
        float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.28f;
        glm::vec4 tc = elem.getTextColorVec();
        uiDrawText(label.c_str(), textX, textY, scale, tc);

        // Draw checkbox box (filled when checked, outline when unchecked)
        glm::vec4 bg = elem.getBackgroundColorVec();
        bg.a *= elem.opacity;
        if (checked) {
            // Filled box with green tint when checked
            glm::vec4 checkBg = bg;
            checkBg.r *= 0.5f; checkBg.g *= 2.0f; checkBg.b *= 0.5f;
            uiDrawRect(boxScreen, checkBg, elem.id.c_str());
            uiDrawRectOutline(boxScreen, {0.3f, 1.0f, 0.4f, 0.8f}, (elem.id + "_outline").c_str());
            // Draw checkmark
            float cx = boxScreen.x + boxScreen.w * 0.5f;
            float cy = boxScreen.y + boxScreen.h * 0.5f;
            uiDrawText("X", cx - 4.0f, cy - 6.0f, scale * 1.2f, {0.3f, 1.0f, 0.4f, 1.0f});
        } else {
            uiDrawRect(boxScreen, bg, elem.id.c_str());
            uiDrawRectOutline(boxScreen, {0.5f, 0.6f, 0.8f, 0.6f}, (elem.id + "_outline").c_str());
        }

        // Handle click on the full design rect (so label and box both toggle)
        UIButtonState s = uiButton(win, "", designRect, {0,0,0,0}, elem.id.c_str());
        if (s.clicked && !elem.binding.empty()) {
            GuiBindings::instance().set(elem.binding, checked ? "false" : "true");
        }

        return s;
    }

    if (type == "text" || type == "label" || type == "dynamic_text")
    {
        const std::string& displayText = hasBindingText ? resolvedText : elem.text;

        float padX = elem.paddingX;
        float padY = elem.paddingY;
        float sx = cs.designToScreenX(rx + padX);
        float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.32f;
        glm::vec4 color = elem.getTextColorVec();
        color.a *= elem.opacity;

        float textX = sx;
        if (elem.textAlign == "center" && !displayText.empty())
        {
            float tw = uiMeasureText(displayText.c_str(), scale);
            float sw = cs.designToScreenX(rw - padX * 2.0f);
            textX = sx + (sw - tw) * 0.5f;
        }
        else if (elem.textAlign == "right" && !displayText.empty())
        {
            float tw = uiMeasureText(displayText.c_str(), scale);
            float sw = cs.designToScreenX(rw - padX * 2.0f);
            textX = sx + sw - tw;
        }

        float sy = computeTextY(cs, {rx, ry, rw, rh}, scale, elem.verticalAlign, padY);

        if (!displayText.empty())
            uiDrawText(displayText.c_str(), textX, sy, scale, color);
        uiTrackWidget(elem.id.c_str(), {rx, ry, rw, rh});
        return {};
    }

    if (type == "image")
    {
        UIRect screenRect = cs.designToScreen({rx, ry, rw, rh});
        if (!elem.backgroundImage.empty())
            uiDrawImage(elem.backgroundImage.c_str(), screenRect);
        uiTrackWidget(elem.id.c_str(), {rx, ry, rw, rh});
        return {};
    }

    if (type == "panel")
    {
        UIRect screenRect = cs.designToScreen({rx, ry, rw, rh});
        glm::vec4 bg = elem.getBackgroundColorVec();
        bg.a *= elem.opacity;
        uiDrawRect(screenRect, bg, elem.id.c_str());
        if (elem.hasOutlineColor())
            uiDrawRectOutline(screenRect, elem.getOutlineColorVec(), elem.id.c_str());
        uiTrackWidget(elem.id.c_str(), {rx, ry, rw, rh});
        return {};
    }

    if (type == "slider")
    {
        if (!elem.enabled) return {};
        SliderState& state = gSliderState[elem.id];
        state.minVal = elem.margin;   // reuse margin as min
        state.maxVal = elem.padding;  // reuse padding as max
        if (state.maxVal <= state.minVal) { state.maxVal = state.minVal + 1.0f; }
        UIRect designRect = {rx, ry, rw, rh};
        bool changed = uiSlider(win, elem.text.c_str(), designRect,
                                 &state.value, state.minVal, state.maxVal);
        UIButtonState s;
        s.clicked = changed;
        return s;
    }

    if (type == "dropdown")
    {
        if (!elem.enabled) return {};
        DropdownState& state = gDropdownState[elem.id];

        // Build items list
        std::vector<std::string> items;
        if (!elem.bindingItems.empty()) {
            std::string itemsStr = GuiBindings::instance().get(elem.bindingItems);
            if (!itemsStr.empty()) {
                size_t pos = 0;
                while ((pos = itemsStr.find(',')) != std::string::npos) {
                    items.push_back(itemsStr.substr(0, pos));
                    itemsStr.erase(0, pos + 1);
                }
                if (!itemsStr.empty()) items.push_back(itemsStr);
            }
        }
        if (items.empty() && !elem.backgroundImage.empty()) {
            std::string itemsStr = elem.backgroundImage;
            size_t pos = 0;
            while ((pos = itemsStr.find(',')) != std::string::npos) {
                items.push_back(itemsStr.substr(0, pos));
                itemsStr.erase(0, pos + 1);
            }
            if (!itemsStr.empty()) items.push_back(itemsStr);
        }

        // Draw header only (items drawn in post-pass overlay for z-order)
        drawDropdown(win, state, rx, ry, rw, rh, nullptr, items);

        UIButtonState s;
        return s;
    }

    if (type == "text_input" || type == "number_input" || type == "password_input")
    {
        if (!elem.enabled) return {};

        const std::string& rawText = hasBindingText ? resolvedText : (elem.text.empty() && hasBindingText ? "" : elem.text);
        UIRect designRect = {rx, ry, rw, rh};

        // Get or create reusable text input state
        UITextInputState& tiState = getOrCreateTextState(elem.id);

        // Sync binding value → text state when not focused (external changes)
        if (!tiState.focused && tiState.value != rawText)
        {
            tiState.value = rawText;
            tiState.cursorPos = (int)tiState.value.size();
            tiState.selectionStart = -1;
        }

        // Build options from element properties
        UITextInputOptions tiOpts;
        tiOpts.maxLength = (size_t)elem.maxLength;
        tiOpts.password = (type == "password_input");
        // For password, check visibility binding
        if (type == "password_input")
        {
            std::string visBinding = bindings.get("server.password_visible");
            if (visBinding == "true")
                tiOpts.password = false;
        }
        tiOpts.selectAllOnFocus = elem.selectAllOnFocus;

        // Character filter based on element property
        tiOpts.characterFilter = nullptr;
        if (!elem.characterFilter.empty())
        {
            if (elem.characterFilter == "server_address")
            {
                tiOpts.characterFilter = [](unsigned int c) -> bool {
                    return (c >= '0' && c <= '9') || c == '.' || c == ':' ||
                           c == '-' || c == '_' ||
                           (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                };
            }
        }

        // Render the reusable text input
        bool stillFocused = uiTextInputRender(win, elem.id.c_str(), designRect, tiState, tiOpts);
        (void)stillFocused;

        // Sync text state → binding (always, so external code sees changes)
        if (!elem.binding.empty() && tiState.value != rawText)
        {
            bindings.set(elem.binding, tiState.value);
        }

        // Store binding key for backward-compat input routing
        if (!elem.binding.empty())
            bindings.set(elem.id + ".binding", elem.binding);
        if (tiState.focused)
            bindings.setFocusedId(elem.id);
        else if (bindings.focusedId() == elem.id)
            bindings.clearFocus();

        // Return click state for external use
        UIButtonState s{};
        s.clicked = tiState.focused;
        return s;
    }

    if (type == "server_list")
    {
        UIRect designRect = {rx, ry, rw, rh};
        UIRect screenRect = cs.designToScreen(designRect);
        glm::vec4 bg = elem.getBackgroundColorVec();
        bg.a *= elem.opacity;
        uiDrawRect(screenRect, bg, elem.id.c_str());
        if (elem.hasOutlineColor())
            uiDrawRectOutline(screenRect, elem.getOutlineColorVec(), elem.id.c_str());

        float tx = cs.designToScreenX(rx + 8.0f);
        float ty = cs.designToScreenY(ry + 8.0f);
        float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.28f;
        glm::vec4 tc = elem.getTextColorVec();
        const std::string& listText = hasBindingText ? resolvedText : elem.text;
        if (!listText.empty())
            uiDrawText(listText.c_str(), tx, ty, scale, tc);

        uiTrackWidget(elem.id.c_str(), {rx, ry, rw, rh});
        return {};
    }

    // Fallback: treat unknown types as buttons
    UIRect designRect = {rx, ry, rw, rh};
    glm::vec4 bg = elem.getBackgroundColorVec();
    if (!elem.enabled) return {};
    const char* btnText = elem.text.c_str();
    return uiButton(win, btnText, designRect, bg, elem.id.c_str());
}
