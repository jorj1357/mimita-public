#include "gui-element-render.h"
#include "gui-coord.h"
#include "gui-bindings.h"
#include "debug/debug-log.h"
#include "../avatar/avatar-editor-dropdown.h"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

// Persistent state for slider/dropdown elements keyed by (window, elementId)
namespace {
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

        // Apply hoverScale by expanding the rect
        if (elem.hoverScale != 1.0f) {
            // hoverScale is applied dynamically in uiButton via hover detection
            // For now, pass it through the id string encoded
        }

        return uiButton(win, elem.text.c_str(), designRect, bg, elem.id.c_str(),
                        hasHover ? &hc : nullptr,
                        hasPressed ? &pc : nullptr,
                        elem.hoverSound.c_str(),
                        elem.clickSound.c_str());
    }

    if (type == "checkbox")
    {
        if (!elem.enabled) return {};
        UIRect designRect = {rx, ry, rw, rh};
        glm::vec4 bg = elem.getBackgroundColorVec();
        const char* label = elem.text.empty() ? "checkbox" : elem.text.c_str();
        return uiButton(win, label, designRect, bg, elem.id.c_str());
    }

    if (type == "text" || type == "label" || type == "dynamic_text")
    {
        const std::string& displayText = hasBindingText ? resolvedText : elem.text;

        float padX = elem.padding;
        float padY = elem.padding * 0.5f;
        float sx = cs.designToScreenX(rx + padX);
        float sy = cs.designToScreenY(ry + padY);
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

        // Override header text with binding value when available
        if (hasBindingText && !resolvedText.empty()) {
            GuiCoordinateSystem& csLoc = GuiCoordinateSystem::instance();
            float tx = csLoc.designToScreenX(rx + 6.0f);
            float ty = csLoc.designToScreenY(ry + 6.0f);
            float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.34f;
            glm::vec4 tc = elem.getTextColorVec();
            uiDrawText(resolvedText.c_str(), tx, ty, scale, tc);
        }

        UIButtonState s;
        return s;
    }

    if (type == "text_input" || type == "number_input")
    {
        if (!elem.enabled) return {};

        const std::string& displayText = hasBindingText ? resolvedText : (elem.text.empty() && hasBindingText ? "" : elem.text);
        UIRect designRect = {rx, ry, rw, rh};
        UIRect screenRect = cs.designToScreen(designRect);

        // Draw background
        glm::vec4 bg = elem.getBackgroundColorVec();
        bg.a *= elem.opacity;
        uiDrawRect(screenRect, bg, elem.id.c_str());

        // Draw outline when focused
        bool isFocused = bindings.hasFocus(elem.id);
        if (isFocused && elem.hasOutlineColor())
            uiDrawRectOutline(screenRect, elem.getOutlineColorVec(), elem.id.c_str());
        else if (elem.hasOutlineColor())
            uiDrawRectOutline(screenRect, elem.getOutlineColorVec(), elem.id.c_str());

        // Draw text with padding
        float padX = 6.0f;
        float padY = 4.0f;
        float tx = cs.designToScreenX(rx + padX);
        float ty = cs.designToScreenY(ry + padY);
        float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.34f;
        glm::vec4 tc = elem.getTextColorVec();

        std::string display = displayText;
        if (displayText.empty())
        {
            if (isFocused)
                display = "|";
        }
        else
        {
            if (isFocused)
                display = displayText + "|";
        }

        uiDrawText(display.c_str(), tx, ty, scale, tc);

        // Store binding key for input routing
        if (!elem.binding.empty())
            bindings.set(elem.id + ".binding", elem.binding);

        // Handle click for focus
        UIButtonState s = uiButton(win, "", designRect, {0,0,0,0}, elem.id.c_str());
        if (s.clicked) {
            bindings.setFocusedId(elem.id);
        }

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
