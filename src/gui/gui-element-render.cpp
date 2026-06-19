#include "gui-element-render.h"
#include "gui-coord.h"
#include "debug/debug-log.h"

#include <algorithm>
#include <cstdio>

UIButtonState drawGuiElement(GLFWwindow* win, const GuiElement& elem,
                             bool* checkboxValue)
{
    if (!elem.visible) return {};

    GuiCoordinateSystem& cs = GuiCoordinateSystem::instance();

    const std::string& type = elem.type;

    if (type == "button")
    {
        UIRect designRect = {elem.x, elem.y, elem.w, elem.h};
        glm::vec4 bg = elem.getBackgroundColorVec();
        return uiButton(win, elem.text.c_str(), designRect, bg, elem.id.c_str());
    }

    if (type == "checkbox")
    {
        UIRect designRect = {elem.x, elem.y, elem.w, elem.h};
        glm::vec4 bg = elem.getBackgroundColorVec();
        const char* label = elem.text.empty() ? "checkbox" : elem.text.c_str();
        return uiButton(win, label, designRect, bg, elem.id.c_str());
    }

    if (type == "text" || type == "label")
    {
        float sx = cs.designToScreenX(elem.x);
        float sy = cs.designToScreenY(elem.y);
        float scale = elem.fontSize > 0.0f ? elem.fontSize : 0.32f;
        glm::vec4 color = elem.getTextColorVec();
        if (!elem.text.empty())
            uiDrawText(elem.text.c_str(), sx, sy, scale, color);
        uiTrackWidget(elem.id.c_str(), {elem.x, elem.y, elem.w, elem.h});
        return {};
    }

    if (type == "image")
    {
        UIRect screenRect = cs.designToScreen({elem.x, elem.y, elem.w, elem.h});
        if (!elem.backgroundImage.empty())
            uiDrawImage(elem.backgroundImage.c_str(), screenRect);
        uiTrackWidget(elem.id.c_str(), {elem.x, elem.y, elem.w, elem.h});
        return {};
    }

    if (type == "panel")
    {
        UIRect screenRect = cs.designToScreen({elem.x, elem.y, elem.w, elem.h});
        glm::vec4 bg = elem.getBackgroundColorVec();
        uiDrawRect(screenRect, bg, elem.id.c_str());
        if (elem.hasOutlineColor())
            uiDrawRectOutline(screenRect, elem.getOutlineColorVec(), elem.id.c_str());
        uiTrackWidget(elem.id.c_str(), {elem.x, elem.y, elem.w, elem.h});
        return {};
    }

    // Fallback: treat unknown types as buttons
    UIRect designRect = {elem.x, elem.y, elem.w, elem.h};
    glm::vec4 bg = elem.getBackgroundColorVec();
    return uiButton(win, elem.text.c_str(), designRect, bg, elem.id.c_str());
}
