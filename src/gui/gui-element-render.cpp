#include "gui-element-render.h"
#include "gui-coord.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstdio>

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

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();
    const std::string& type = elem.type;

    if (type == "button")
    {
        UIRect designRect = {rx, ry, rw, rh};
        glm::vec4 bg = elem.getBackgroundColorVec();
        return uiButton(win, elem.text.c_str(), designRect, bg, elem.id.c_str());
    }

    if (type == "checkbox")
    {
        UIRect designRect = {rx, ry, rw, rh};
        glm::vec4 bg = elem.getBackgroundColorVec();
        const char* label = elem.text.empty() ? "checkbox" : elem.text.c_str();
        return uiButton(win, label, designRect, bg, elem.id.c_str());
    }

    if (type == "text" || type == "label")
    {
        float sx = cs.designToScreenX(rx);
        float sy = cs.designToScreenY(ry);
        float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.32f;
        glm::vec4 color = elem.getTextColorVec();
        if (!elem.text.empty())
            uiDrawText(elem.text.c_str(), sx, sy, scale, color);
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
        uiDrawRect(screenRect, bg, elem.id.c_str());
        if (elem.hasOutlineColor())
            uiDrawRectOutline(screenRect, elem.getOutlineColorVec(), elem.id.c_str());
        uiTrackWidget(elem.id.c_str(), {rx, ry, rw, rh});
        return {};
    }

    // Fallback: treat unknown types as buttons
    UIRect designRect = {rx, ry, rw, rh};
    glm::vec4 bg = elem.getBackgroundColorVec();
    return uiButton(win, elem.text.c_str(), designRect, bg, elem.id.c_str());
}
