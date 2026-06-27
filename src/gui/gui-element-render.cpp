#include "gui-element-render.h"
#include "gui-coord.h"
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

UIButtonState drawGuiElement(GLFWwindow* win, const GuiElement& elem,
                             bool* checkboxValue,
                             const UIRect* overrideDesignRect)
{
    if (!elem.visible) return {};

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

    if (type == "text" || type == "label")
    {
        // Apply padding (internal spacing for text)
        float padX = elem.padding;
        float padY = elem.padding * 0.5f;
        float sx = cs.designToScreenX(rx + padX);
        float sy = cs.designToScreenY(ry + padY);
        float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.32f;
        glm::vec4 color = elem.getTextColorVec();
        color.a *= elem.opacity;

        // Apply text alignment with padded width
        float textX = sx;
        if (elem.textAlign == "center" && !elem.text.empty())
        {
            float tw = uiMeasureText(elem.text.c_str(), scale);
            float sw = cs.designToScreenX(rw - padX * 2.0f);
            textX = sx + (sw - tw) * 0.5f;
        }
        else if (elem.textAlign == "right" && !elem.text.empty())
        {
            float tw = uiMeasureText(elem.text.c_str(), scale);
            float sw = cs.designToScreenX(rw - padX * 2.0f);
            textX = sx + sw - tw;
        }

        if (!elem.text.empty())
            uiDrawText(elem.text.c_str(), textX, sy, scale, color);
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
        std::vector<std::string> items;
        if (!elem.backgroundImage.empty()) {
            std::string itemsStr = elem.backgroundImage;
            size_t pos = 0;
            while ((pos = itemsStr.find(',')) != std::string::npos) {
                items.push_back(itemsStr.substr(0, pos));
                itemsStr.erase(0, pos + 1);
            }
            if (!itemsStr.empty()) items.push_back(itemsStr);
        }
        if (items.empty()) items.push_back("None");
        int sel = drawDropdown(win, state, rx, ry, rw, rh,
                                elem.text.c_str(), items);
        UIButtonState s;
        if (sel >= 0) s.clicked = true;
        return s;
    }

    // Fallback: treat unknown types as buttons
    UIRect designRect = {rx, ry, rw, rh};
    glm::vec4 bg = elem.getBackgroundColorVec();
    if (!elem.enabled) return {};
    return uiButton(win, elem.text.c_str(), designRect, bg, elem.id.c_str());
}
